#include "ConfigReader.h"
#include <algorithm>
#include <string_view>

static std::string stripComment(const std::string& line)
{
    bool inQuotes = false;
    for (std::size_t i = 0; i < line.size(); ++i)
    {
        if (line[i] == '"')
            inQuotes = !inQuotes;
        else if (line[i] == '#' && !inQuotes)
            return line.substr(0, i);
    }
    return line;
}

static std::string_view trim(std::string_view sv)
{
    const auto start = sv.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos)
        return {};
    const auto end = sv.find_last_not_of(" \t\r\n");
    return sv.substr(start, end - start + 1);
}

static std::string_view firstToken(std::string_view line)
{
    return line.substr(0, line.find_first_of(" \t{"));
}

static int countChar(std::string_view sv, char c)
{
    return static_cast<int>(std::count(sv.begin(), sv.end(), c));
}

ConfigReader::ConfigReader(const std::string& file)
{
    std::ifstream rawsettings(file);
    if (!rawsettings.is_open())
        throw HttpServerException("Config file could not be opened: " + file);

    switch (readConfig(rawsettings))
    {
    case ParseResult::TotalFailure:
    case ParseResult::SuccessWithWarnings:
        throw HttpServerException("Error reading config file: " + file);
    case ParseResult::SuccessWithWarnings:
    case ParseResult::Success:
        break;
    }
    // We do not need to close. rawsettings is a local ifstream and will be closed by deconstructor
    // even if the we throw an exeption
}

const WebserverSettings* ConfigReader::getSettings(const std::string& route) const
{
    const auto it = vhosts.find(route);
    if (it == vhosts.end())
        throw HttpServerException("Route not found: " + route);
    return it->second;
}

std::unique_ptr<WebserverSettings> ConfigReader::readConfigBlock(const std::string& block)
{
    return std::make_unique<WebserverSettings>(WebserverSettings::fromBlock(block));
}

ConfigReader::ParseResult ConfigReader::readConfig(std::ifstream& rawsettings)
{
    int bad = 0;
    int total = 0;
    int depth = 0;
    bool pendingServerBrace = false;  // "server" is alone and  { is missing
    std::size_t lineNo = 0;
    std::string line;
    std::string block;
    std::map<std::string, std::size_t> defaultServerLines;  // "address:port" -> line of the block that first claimed it

    while (std::getline(rawsettings, line))
    {
        ++lineNo;
        line = stripComment(line);
        std::string trimmed(trim(line));
        if (trimmed.empty())
            continue;

        if (depth == 0)
        {
            if (pendingServerBrace)
            {
                if (trimmed != "{")
                    throw HttpServerException(
                        "Config error at line " + std::to_string(lineNo) +
                        ": expected '{' after 'server'");
                pendingServerBrace = false;
                depth = 1;
                block.clear();
                continue;
            }

            if (firstToken(trimmed) == "server")
            {
                const auto bracePos = trimmed.find('{');
                if (bracePos == std::string::npos)
                {
                    pendingServerBrace = true;
                    continue;
                }
                block.clear();
                depth = 1;
                trimmed = trim(trimmed.substr(bracePos + 1));
                if (trimmed.empty())
                    continue;
            }
            else
            {
                std::cerr << "Config warning (line " << lineNo << "): '" << trimmed << "' is outside of any server block, ignoring it\n";
                continue;
            }
        }

        // We are inside a "server { ... }" block (possibly further
        // nested, e.g., in a "location { ... }"). We count ALL
        // braces in the line, not just whether one occurs - otherwise lines like
        // "} location /x {" or single-line blocks "location / { root /www; }"
        // are miscounted (it was a bug bevor).
        const int opens = countChar(trimmed, '{');
        const int closes = countChar(trimmed, '}');
        depth += opens - closes;

        if (depth < 0)
            throw HttpServerException(
                "Config error at line " + std::to_string(lineNo) + ": unmatched '}'");

        if (depth == 0)
        {
            try
            {
                auto settings = readConfigBlock(block);
                if (settings->listen.empty())
                    throw HttpServerException("server block requires at least one 'listen' directive");
                for (const auto& listenDir : settings->listen)
                {
                    if (!listenDir.is_default)
                        continue;
                    const std::string key = listenDir.address + ":" + std::to_string(listenDir.port);
                    const auto [it, inserted] = defaultServerLines.try_emplace(key, lineNo);
                    if (!inserted)
                        throw HttpServerException(
                            "duplicate default_server for " + key + " (already claimed by the server block ending at line " +
                            std::to_string(it->second) + ")");
                }
                for (const auto& name : settings->server_name)
                {
                    const auto [it, inserted] = vhosts.try_emplace(name, settings.get());
                    if (!inserted)
                        std::cerr << "Config warning: duplicate server_name '" << name << "', keeping the first definition\n"; // like Nginx
                }
                blocks.push_back(std::move(settings));
            }
            catch (const std::exception& e)
            {
                ++bad;
                std::cerr << "Config error (server block ending at line " << lineNo
                           << "): " << e.what() << '\n';
            }
            ++total;
            block.clear();
            continue;
        }

        block += trimmed;
        block += '\n';
    }

    if (depth != 0 || pendingServerBrace)
        throw HttpServerException("Config error: file ended with an unclosed 'server' block");

    if (total == 0)
        throw HttpServerException("Config error: no 'server { ... }' block found in file");

    if (bad == total)
        return ParseResult::TotalFailure;
    if (bad > 0)
        return ParseResult::SuccessWithWarnings;
    return ParseResult::Success;
}
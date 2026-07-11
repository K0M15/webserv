#include "WebserverSettings.hpp"
#include <sstream>
#include <cstdlib>

static std::string valueAfter(const std::string& line, const std::string& keyword)
{
    size_t pos = keyword.size();
    while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t'))
        ++pos;
    std::string val = line.substr(pos);
    if (!val.empty() && val.back() == ';')
        val.pop_back();
    return val;
}

WebserverSettings WebserverSettings::getDefaultSettings()
{
    WebserverSettings settings;
    settings.dirindex = false;
    settings.index = "index.html";
    settings.root = "";
    return settings;
}

struct SMParser{
    LocationConfig loc;
    std::string location_path;
    bool in_location;
};

WebserverSettings WebserverSettings::fromBlock(const std::string& block)
{
    WebserverSettings settings = getDefaultSettings();
    SMParser parser;
    parser.in_location = false;
    parser.loc = {};
    parser.location_path = "";
    std::istringstream stream(block);
    std::string line;

    while (std::getline(stream, line))
    {
        if (line.empty()) continue;

        if (!parser.in_location)
        {
            if (line.compare(0, 6, "listen") == 0)
            {
                ListenDirective dir;
                dir.is_default = (line.find("default_server") != std::string::npos);
                std::string val = valueAfter(line, "listen");

                size_t colon = val.rfind(':');
                if (colon != std::string::npos)
                {
                    dir.address = val.substr(0, colon);
                    dir.port = std::atoi(val.substr(colon + 1).c_str());
                }
                else
                {
                    dir.address = "0.0.0.0";
                    dir.port = std::atoi(val.c_str());
                }
                settings.listen.push_back(dir);
            }
            else if (line.compare(0, 11, "server_name") == 0)
            {
                std::string val = valueAfter(line, "server_name");
                // split by spaces
                std::istringstream ns(val);
                std::string name;
                while (ns >> name)
                    settings.server_name.push_back(name);
            }
            else if (line.compare(0, 4, "root") == 0)
            {
                settings.root = valueAfter(line, "root");
            }
            else if (line.compare(0, 5, "index") == 0)
            {
                settings.index = valueAfter(line, "index");
            }
            else if (line.compare(0, 8, "dirindex") == 0)
            {
                std::string val = valueAfter(line, "dirindex");
                settings.dirindex = (val == "on" || val == "true");
            }
            else if (line.compare(0, 8, "location") == 0)
            {
                parser.in_location = true;
                size_t open = line.find('{');
                if (open == std::string::npos)
                    throw std::runtime_error("Open bracket not found in location");
                parser.location_path = valueAfter(line, "location");
                // strip ' {' or '{' suffix
                size_t brace = parser.location_path.find('{');
                if (brace != std::string::npos)
                    parser.location_path = parser.location_path.substr(0, brace);
                // trim trailing space
                while (!parser.location_path.empty() && parser.location_path.back() == ' ')
                    parser.location_path.pop_back();
            }
            else if (line.front() != '#' && !line.empty())
            {
                // silently skip unknown top-level directives for now
            }
        }
        else // inside location block
        {
            if (line.compare(0, 1, "}") == 0)
            {
                settings.locations[parser.location_path] = parser.loc;
                parser.in_location = false;
                parser.loc = LocationConfig();
                parser.location_path = "";
            }
            else if (line.compare(0, 4, "root") == 0)
            {
                parser.loc.root = valueAfter(line, "root");
            }
            else if (line.compare(0, 5, "index") == 0)
            {
                parser.loc.index = valueAfter(line, "index");
            }
            else if (line.compare(0, 8, "dirindex") == 0)
            {
                std::string val = valueAfter(line, "dirindex");
                parser.loc.dirindex = (val == "on" || val == "true");
            }
            else if (line.compare(0, 6, "listen") == 0)
            {
                // listen is a server property, not location
            }
            else if (line.compare(0, 10, "upload_dir") == 0)
            {
                parser.loc.upload_dir = valueAfter(line, "upload_dir");
            }
            else if (line.compare(0, 9, "redirect") == 0)
            {
                parser.loc.redirect = valueAfter(line, "redirect");
            }
            else if (line.compare(0, 13, "cgi_extension") == 0)
            {
                parser.loc.cgi_extension = valueAfter(line, "cgi_extension");
            }
            else if (line.front() != '#' && !line.empty())
            {
                // silently skip unknown location directives
            }
        }
    }
    return settings;
}

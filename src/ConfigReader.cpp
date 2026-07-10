#include "ConfigReader.h"
#include <iostream> //debug

ConfigReader::ConfigReader(const std::string& file)
{
    std::ifstream rawsettings(file);
    if (!rawsettings.is_open())
        throw HttpServerException("File could not be opened!");
    int configresult = readConfig(rawsettings);
    switch (configresult)
    {
    case 1:
        // break;
    case 2:
        throw HttpServerException("Error reading file!");
        break;
    default:
        break;
    }
    rawsettings.close();
}

ConfigReader::ConfigReader(const ConfigReader& other)
{
    data = other.data;
}

ConfigReader& ConfigReader::operator=(const ConfigReader& other)
{
    if (this != &other)
    {
        data = other.data; 
    }
    return *this;
}

ConfigReader::~ConfigReader()
{
}

const WebserverSettings& ConfigReader::getSettings(const std::string& route){
    std::map<std::string, WebserverSettings>::iterator
        settings = data.find(route);
    if (settings == data.end())
        throw HttpServerException("Route not found");
    return settings->second;
}

std::pair<std::string, WebserverSettings> ConfigReader::readConfigBlock(const std::string& block)
{
    std::pair<std::string, WebserverSettings> res;
    std::cout << "Block: " << block << "\n"; //debug
    res.second = WebserverSettings::fromBlock(block);
    return res;
}

static std::string stripWhitespace(std::string& str){
    std::string result;
    result.reserve(str.size());
    std::copy_if(str.begin(), str.end(), std::back_inserter(result),
        [](unsigned char c){ return !std::isspace(c);});
    return result;
}

int ConfigReader::readConfig(std::ifstream& rawsettings)
{
    int bad = 0;
    int total = 0;
    int depth = 0;
    std::string line;
    std::string block;
    while (std::getline(rawsettings, line))
    {
        size_t hash =  line.find('#');
        if (hash != line.npos)
            line = line.substr(0, hash); // Remove comments
        line = stripWhitespace(line);       // Remove whitespaces
        if (line.find("server{") != line.npos)
        {
            depth++;
            continue;
        }
        if (line.find("{") != line.npos)
        {
            depth++;
        }
        if (line.find('}') != line.npos && depth == 1)
        {
            try
            {
                // block.append(line);
                auto p = readConfigBlock(block);
                data[p.first] = p.second;
            }
            catch(const std::exception& e)
            {
                bad++;
                std::cerr << e.what() << '\n';
            }
            depth--;
            total++;
            continue;
        }
        if (line.find('}') != line.npos)
            depth--;
        block.append(line);
        block.append("\n");
    }
    if (depth != 0)
    {
        throw "Config error";
    }
    if (bad == total)
        return 2;
    if (bad > 0)
        return 1;
    return 0;
}



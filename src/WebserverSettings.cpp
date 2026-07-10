#include "WebserverSettings.hpp"

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
        if (line.compare(0, 6, "listen") == 0 && !parser.in_location)
        {
            ListenDirective dir;
            size_t splitter = line.find(':'); // split between port and ip 127.0.0.1:4000
            if (splitter != line.npos)
            {
                dir.address = line.substr(6, splitter - 6);
                dir.port = std::atoi(line.substr(splitter).c_str());
                dir.is_default = line.find("default") != line.npos;
            }
            else
            {
                dir.port = std::atoi(line.substr(6).c_str());
                dir.is_default = line.find("default") != line.npos;
            }
            settings.listen.push_back(dir);
            continue;
        }
        if (!line.compare(0, 4, "root") && !parser.in_location){
            settings.root = line.substr(5);
            continue;
        }
        if (!line.compare(0, 11, "server_name") && !parser.in_location){ throw std::runtime_error("Not implemented"); }
        if (!line.compare(0, 5, "index") && !parser.in_location){ throw std::runtime_error("Not implemented"); }
        if (!line.compare(0, 8, "dirindex") && !parser.in_location){ throw std::runtime_error("Not implemented"); }
        if (!line.compare(0, 8, "location") && !parser.in_location){
            parser.in_location = true;
            size_t open = line.find('{');
            if(open == line.npos)
                throw std::runtime_error("Open Bracket not found");
            parser.location_path = line.substr(8, open - 8);
            continue;
        }
        if (parser.in_location && line.compare(0, 6, "listen") == 0)
        {
            // THis is a server property
            throw std::runtime_error("Listen is a server property");
        }
        if (parser.in_location && line.compare(0, 8, "}") == 0){
            settings.locations[parser.location_path] = parser.loc;
            parser.in_location = false;
            parser.loc = {};
            parser.location_path = "";
            continue;
        }
        if (parser.in_location && line.compare(0, 8, "dirindex") == 0){
            parser.loc.dirindex = line.find("true") != line.npos;
            continue;
        }
        if (parser.in_location && line.compare(0, 10, "upload_dir") == 0){
            parser.loc.upload_dir = line.substr(10);
            continue;
        }
        if (parser.in_location && line.compare(0, 10, "redirect") == 0){
            parser.loc.redirect = line.substr(10);
            continue;
        }
        if (parser.in_location && line.compare(0, 4, "root") == 0){
            parser.loc.root = line.substr(4);
            continue;
        }
        throw std::runtime_error("Unexpected Input");

    }
    return settings;
}

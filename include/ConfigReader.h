#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <fstream>
#include "WebserverSettings.hpp"
#include "HttpServerException.hpp"
#include <iostream>
#include <memory>

class ConfigReader{
private:
    // this contains host as first element and settings as second
    std::unique_ptr<WebserverSettings> readConfigBlock(const std::string& block);
    /* Will find each block of {} to parse inside readConfigBlock()
    Return Value:
    0 = fully parsed, no errors
    1 = partial error, see logs
    2 = full error, crash
    */
    int readConfig(std::ifstream& rawsettings);
public:
    ConfigReader() : blocks(), vhosts() {}
    std::vector<std::unique_ptr<WebserverSettings>> blocks;
    std::unordered_map<std::string, WebserverSettings*> vhosts;
    ConfigReader(const std::string& file);
    ~ConfigReader();
    const WebserverSettings* getSettings(const std::string& route);
};

inline std::ostream& operator<<(std::ostream& os, const ConfigReader& cr) {
    os << "=== ConfigReader (" << cr.blocks.size() << " server block(s)) ===\n\n";
    for (const auto& [host, settings] : cr.vhosts) {
        os << "server {\n";
        os << "  host:          " << host << "\n";
        os << *settings;
        os << "}\n\n";
    }
    return os;
}

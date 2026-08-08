#pragma once

#include <string>
#include <map>
#include <fstream>
#include "WebserverSettings.hpp"
#include "HttpServerException.hpp"
#include <iostream>

class ConfigReader{
private:
    // this contains host as first element and settings as second
    std::pair<std::string, WebserverSettings> readConfigBlock(const std::string& block);
    /* Will find each block of {} to parse inside readConfigBlock()
    Return Value:
    0 = fully parsed, no errors
    1 = partial error, see logs
    2 = full error, crash
    */
    int readConfig(std::ifstream& rawsettings);
    ConfigReader() : data() {}
public:
    std::map<std::string, WebserverSettings> data;
    ConfigReader(const std::string& file);
    ConfigReader(const ConfigReader& other);
    ConfigReader& operator=(const ConfigReader& other);
    ~ConfigReader();
    const WebserverSettings& getSettings(const std::string& route);
};

inline std::ostream& operator<<(std::ostream& os, const ConfigReader& cr) {
    os << "=== ConfigReader (" << cr.data.size() << " server block(s)) ===\n\n";
    for (const auto& [host, settings] : cr.data) {
        os << "server {\n";
        os << "  host:          " << host << "\n";
        os << settings;
        os << "}\n\n";
    }
    return os;
}

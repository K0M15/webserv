#pragma once

#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "HttpServerException.hpp"
#include "WebserverSettings.hpp"

class ConfigReader
{
public:
    explicit ConfigReader(const std::string& file);
    ~ConfigReader() = default;
    ConfigReader(const ConfigReader&) = delete;
    ConfigReader& operator=(const ConfigReader&) = delete;
    ConfigReader(ConfigReader&&) = default;
    ConfigReader& operator=(ConfigReader&&) = default;

    [[nodiscard]] const WebserverSettings* getSettings(const std::string& route) const;

    [[nodiscard]] const std::vector<std::unique_ptr<WebserverSettings>>& getAllServers() const
    {
        return blocks;
    }

private:
    enum class ParseResult 
    {
        Success,
        SuccessWithWarnings,
        TotalFailure 
    };

    ParseResult readConfig(std::ifstream& rawsettings);
    std::unique_ptr<WebserverSettings> readConfigBlock(const std::string& block);

    std::map<std::string, WebserverSettings*> vhosts;
    std::vector<std::unique_ptr<WebserverSettings>> blocks;
};
#pragma once

#include <string>
#include <unordered_map>
#include <iostream>
#include "HttpServerException.hpp"
#include "PathUtils.hpp"

class URL {
public:
    URL();
    explicit URL(const std::string& s);
    
    const std::string str() const;
    void setURL(const std::string& s);
    const std::string getRawQuery() const;
    const std::string getFileExt() const;
    const std::unordered_map<std::string, std::string> getQuery() const;

private:
    static bool isValidURL(const std::string& url);
    std::string value;
};

std::istream& operator>>(std::istream& is, URL& url);
#pragma once

#include <map>
#include <charconv>
#include <iostream>
#include <sstream>
#include <string>
#include "URL.hpp"


class Request {
private:
    std::string method;
    URL url;
    std::string query;
    std::string version;
    std::map<std::string, std::string> headers;
    std::string body;
    bool chunked;

    Request() : chunked(false) {}
    
public:
    ~Request() = default;
    Request(const Request& other) = default;
    Request& operator=(const Request& other);
    static Request fromString(const std::string& rawRequest);
    const std::string& getMethod() const { return method; }
    const std::string& getBody() const { return body; }
    bool isChunked() const { return chunked; }
    const std::string& getHeader(const std::string& key) const;
    const URL& getURL() const { return url; }
    const std::string getVersion() const { return version; }
    const std::map<std::string, std::string>& getHeaders() const { return headers; }
    std::string getCookie(const std::string& name) const;
};

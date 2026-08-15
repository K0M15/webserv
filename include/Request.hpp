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
    const std::string& getHeader(const std::string& key) const {
        // fromString() stores header keys lowercased, so lookups must
        // lowercase the key too, or e.g. getHeader("Content-Type") never matches.
        std::string lower_key = key;
        std::transform(lower_key.begin(), lower_key.end(), lower_key.begin(), [](unsigned char c)
            {
                return std::tolower(c);
            });
        std::map<std::string, std::string>::const_iterator it = headers.find(lower_key);
        if (it != headers.end()) {
            return it->second;
        }
        static const std::string empty;
        return empty;
    }
    const URL& getURL() const { return url; }
    const std::string getVersion() const { return version; }
    const std::map<std::string, std::string>& getHeaders() const { return headers; }
};

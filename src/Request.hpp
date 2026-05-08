
#include <map>
#include <iostream>
#include <sstream>
#include <string>
#include "URL.hpp"


class Request {
private:
    std::string method;
    URL url;
    std::string version;
    std::map<std::string, std::string> headers;
    std::string body;

    Request(){}
    
public:
    ~Request() = default;
    Request(const Request& other) = default;
    Request& operator=(const Request& other);
    static Request fromString(const std::string& rawRequest);
    const std::string& getMethod() const { return method; }
    const std::string& getBody() const { return body; }
    const std::string& getHeader(const std::string& key) const {
        std::map<std::string, std::string>::const_iterator it = headers.find(key);
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

#pragma once

#include <map>
#include <charconv>
#include <iostream>
#include <sstream>
#include <string>
#include <algorithm>
#include "URL.hpp"

struct Connection;

enum class RequestReadState {
    INCOMPLETE,         // wait for more
    COMPLETE,           // ready to be handled
    BAD_REQUEST,        // malformed framing or request smuggling -> 400
    PAYLOAD_TOO_LARGE,  // body exceeds max_body_size             -> 413
    NOT_IMPLEMENTED,    // unsupported transfer coding            -> 501
    EXPECTATION_FAILED  // wrong expectation in header            -> 417
};

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
    class HTTPVersionNotSupportedException : public std::exception {
    private:
        std::string _reason;
    public:
        HTTPVersionNotSupportedException(const std::string& reason) : _reason(reason) {}
        const char* what() const noexcept override {
            return this->_reason.c_str();
        }
    };
    class HTTPMethodNotAllowedException : public std::exception {
    private:
        std::string _reason;
    public:
        HTTPMethodNotAllowedException(const std::string& reason) : _reason(reason) {}
        const char* what() const noexcept override {
            return this->_reason.c_str();
        }
    };
    ~Request() = default;
    Request(const Request& other) = default;
    Request& operator=(const Request& other);
    static Request fromString(const std::string& rawRequest);
    static RequestReadState isRequestComplete(Connection& conn);

    const std::string& getMethod() const { return method; }
    const std::string& getBody() const { return body; }
    bool isChunked() const { return chunked; }
    const std::string& getHeader(const std::string& key) const;
    const URL& getURL() const { return url; }
    const std::string getVersion() const { return version; }
    const std::map<std::string, std::string>& getHeaders() const { return headers; }
    std::string getCookie(const std::string& name) const;
};

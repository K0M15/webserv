#pragma once

#include <string>
#include <exception>


class HttpServerException : public std::exception {
private:
    std::string _reason;
public:
    HttpServerException(const std::string& reason) : _reason(reason) {}
    const char* what() const noexcept override {
        return this->_reason.c_str();
    }
};

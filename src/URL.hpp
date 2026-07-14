#pragma once

#include <string>
#include <regex>
#include "HttpServerException.hpp"

class URL{
public:
    URL() : value() {}
    explicit URL(const std::string& s) : value(std::move(s)){
        // allow absolute URI, absolute path (origin-form), or asterisk (*)
        const std::regex url_pattern(
        R"(^(?!.*\/\.\.?\/)((https?|ftp)://[^\s/$.?#].[^\s]*|/[^\s]*|\*)$)",
        std::regex::icase);
        if (!std::regex_match(value, url_pattern))
        {
            throw HttpServerException("URL String does not match any valid request-target pattern");
        }
    }
    const std::string str() const { return value;}
    void setURL(const std::string& s){
        URL url(s); //checks data
        this->value = std::move(s); // replaces current value
    }
private:
    std::string value;
};

inline std::istream& operator>>(std::istream& is, URL& url){
    std::string temp;
    if (is >> temp)
        url.setURL(temp);
    return is;
}
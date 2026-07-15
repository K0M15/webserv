#pragma once

#include <string>
#include <unordered_map>
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
    const std::string getRawQuery()
    {
        size_t separator = value.find('?');
        if (separator == value.npos)
            return "";
        return value.substr(separator + 1);
    }
    const std::unordered_map<std::string, std::string> getQuery()
    {
        std::unordered_map<std::string, std::string> result;
        std::string raw = getRawQuery();
        size_t pos = 0;
        while (pos < raw.size())
        {
            size_t next_and = raw.find("&", pos);
            std::string curr_pair = (next_and == raw.npos ? raw.substr(pos) : raw.substr(pos, next_and - pos));
            size_t curr_equ = curr_pair.find('=');
            if (curr_equ != curr_pair.npos)
                result[curr_pair.substr(0, curr_equ)] = curr_pair.substr(curr_equ + 1);
            else if (curr_pair.length() != 0)
                result[curr_pair] = "";
            if (next_and == raw.npos) break;
            pos = next_and + 1;
        }
        return result;
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
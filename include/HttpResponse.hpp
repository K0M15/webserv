#pragma once

#include <string>
#include <map>
#include "HttpStatusReason.hpp"

class HttpResponse {
public:
    HttpResponse();
    ~HttpResponse();

    void setStatus(unsigned int code);
    void setBody(const std::string& body);
    void setKeepAlive(bool keep);

    void addHeader(const std::string& key, const std::string& value);
    void removeHeader(const std::string& key);

    std::string toString() const;

    unsigned int getStatus() const;
    const std::map<std::string, std::string>& getHeaders() const;
    const std::string& getBody() const;
    bool getKeepAlive() const;

    static HttpResponse error(unsigned int code);
    static HttpResponse dirindex(const std::string& path, const std::string prefix);

private:
    unsigned int m_status;
    std::map<std::string, std::string> m_headers;
    std::string m_body;
    bool m_keep_alive;
};

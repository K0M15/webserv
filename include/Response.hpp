#pragma once

#include <string>
#include <map>
#include <ctime>
#include "HttpStatusReason.hpp"
#include "WebserverSettings.hpp"
#include "Defines.hpp"

class Response {
public:
    Response();
    ~Response();

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

    static Response error(unsigned int code);
    static Response dirindex(const std::string& path, const std::string prefix);
    static Response errorResponse(unsigned int code,
                                  const WebserverSettings* settings,
                                  const LocationConfig* location);
    static std::string httpDate(time_t t);

private:
    unsigned int m_status;
    std::map<std::string, std::string> m_headers;
    std::string m_body;
    bool m_keep_alive;
};


#include "HttpResponse.hpp"
#include "HttpStatusReason.hpp"
#include <sstream>

HttpResponse::HttpResponse()
    : m_status(200), m_keep_alive(false)
{
}

HttpResponse::~HttpResponse()
{
}

void HttpResponse::setStatus(unsigned int code)
{
    m_status = code;
}

void HttpResponse::setBody(const std::string& body)
{
    m_body = body;
}

void HttpResponse::setKeepAlive(bool keep)
{
    m_keep_alive = keep;
}

void HttpResponse::addHeader(const std::string& key, const std::string& value)
{
    m_headers[key] = value;
}

void HttpResponse::removeHeader(const std::string& key)
{
    m_headers.erase(key);
}

std::string HttpResponse::toString() const
{
    std::ostringstream oss;

    oss << "HTTP/1.1 " << m_status << " " << HttpStatusReason::reason(m_status) << "\r\n";

    if (m_headers.find("Content-Type") == m_headers.end() && !m_body.empty())
        oss << "Content-Type: text/html\r\n";

    for (const auto& header : m_headers)
        oss << header.first << ": " << header.second << "\r\n";

    if (m_headers.find("Content-Length") == m_headers.end())
        oss << "Content-Length: " << m_body.size() << "\r\n";

    oss << "Connection: " << (m_keep_alive ? "keep-alive" : "close") << "\r\n";

    oss << "\r\n";

    if (!m_body.empty())
        oss << m_body;

    return oss.str();
}

unsigned int HttpResponse::getStatus() const
{
    return m_status;
}

const std::map<std::string, std::string>& HttpResponse::getHeaders() const
{
    return m_headers;
}

const std::string& HttpResponse::getBody() const
{
    return m_body;
}

bool HttpResponse::getKeepAlive() const
{
    return m_keep_alive;
}

HttpResponse HttpResponse::error(unsigned int code)
{
    HttpResponse resp;
    resp.setStatus(code);
    resp.setBody("<h1>" + std::to_string(code) + " " + HttpStatusReason::reason(code) + "</h1>");
    resp.m_headers["Content-Type"] = "text/html";
    return resp;
}

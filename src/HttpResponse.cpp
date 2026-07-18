#include "HttpResponse.hpp"
#include "HttpStatusReason.hpp"
#include <sstream>
#include <iostream>
#include <iomanip>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

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

HttpResponse HttpResponse::dirindex(const std::string& path, const std::string prefix)
{
    HttpResponse resp;
    resp.setStatus(200);
    resp.addHeader("Content-Type", "text/html");
    std::ostringstream document;
    document << "<!DOCTYPE html><html><head><title>Index of " << prefix << "</title>";
    document << "<style type=\"text/css\">td{min-width:300px}thead{text-align:left}body{font-family:monospace}\n</style>";
    document << "</head>";
    document << "<body><h1>Index of "<< prefix <<"</h1><hr><table>";
    document << "<thead><tr><th>Name</th><th>Size</th><th>Last modified</th></tr></thead>";
    DIR* dir = opendir(path.c_str());
    if (dir)
    {
        struct dirent* entry;
        struct stat entryStat{};
        while ((entry = readdir(dir)) != nullptr)
        {
            if (std::string(entry->d_name) == "." || std::string(entry->d_name) == "..")
                continue;
            if (stat(std::string(path + entry->d_name).c_str(), &entryStat))
            {
#ifndef DEBUG
                std::cout << "[Error] stat reading " << path << entry->d_name << " , returned error: " << strerror(errno) << std::endl;
                document << "<tr><td><a href=\"" << prefix + "/" + entry->d_name << "\">" << entry->d_name <<"</a></td><td>" << entry->d_reclen <<" byte </td></tr>\n";                
                continue;
#endif /* DEBUG */
            }
            size_t s = entryStat.st_size;
            timespec mtime = entryStat.st_mtim;
            std::tm local_tm = *std::localtime(&mtime.tv_sec);
            document << "<tr><td><a href=\"" << prefix + "/" + entry->d_name << "\">" << entry->d_name <<"</a></td><td>" << s <<" byte </td><td>" << std::put_time(&local_tm, "%Y-%m-%d %H:%M:%S") << "</td></tr>\n";
        }
    }
    document << "</table>";
    document << "</body></html>";
    resp.setBody(document.str());
    return resp;
}

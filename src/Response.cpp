#include "Response.hpp"
#include "HttpStatusReason.hpp"
#include "StandardErrorPages.hpp"
#include "PathUtils.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <ctime>

std::string Response::httpDate(time_t t)
{
    struct tm tm_buf;
    gmtime_r(&t, &tm_buf);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S GMT", &tm_buf);
    return buf;
}

Response::Response()
    : m_status(200), m_keep_alive(false)
{
}

Response::~Response()
{
}

void Response::setStatus(unsigned int code)
{
    m_status = code;
}

void Response::setBody(const std::string& body)
{
    m_body = body;
}

void Response::setKeepAlive(bool keep)
{
    m_keep_alive = keep;
}

void Response::addHeader(const std::string& key, const std::string& value)
{
    m_headers[key] = value;
}

void Response::removeHeader(const std::string& key)
{
    m_headers.erase(key);
}

std::string Response::toString() const
{
    std::ostringstream oss;

    oss << HTTP_VERSION << " " << m_status << " " << HttpStatusReason::reason(m_status) << "\r\n";

    if (m_headers.find("Content-Type") == m_headers.end() && !m_body.empty())
        oss << "Content-Type: text/html\r\n";

    for (const auto& header : m_headers)
        oss << header.first << ": " << header.second << "\r\n";

    if (m_headers.find("Content-Length") == m_headers.end() && m_status != 204)
        oss << "Content-Length: " << m_body.size() << "\r\n";

    oss << "Connection: " << (m_keep_alive ? "keep-alive" : "close") << "\r\n";

    if (m_headers.find("Date") == m_headers.end())
        oss << "Date: " << httpDate(time(nullptr)) << "\r\n";

    if (m_headers.find("Server") == m_headers.end())
        oss << "Server: " << APPLICATION_ID << "\r\n";

    oss << "\r\n";

    if (!m_body.empty())
        oss << m_body;

    return oss.str();
}

unsigned int Response::getStatus() const
{
    return m_status;
}

const std::map<std::string, std::string>& Response::getHeaders() const
{
    return m_headers;
}

const std::string& Response::getBody() const
{
    return m_body;
}

bool Response::getKeepAlive() const
{
    return m_keep_alive;
}

Response Response::error(unsigned int code)
{
    Response resp;
    resp.setStatus(code);
    switch (code)
    {
        case 204:{
            resp.setBody("");
            break;
        }
        default:{
            resp.addHeader("Content-Type", "text/html");
            resp.setBody(HtmlPages::construct_errorpage(code, HttpStatusReason::reason(code)));
        }
    }
    return resp;
}

Response Response::dirindex(const std::string& path, const std::string prefix)
{
    Response resp;
    resp.setStatus(200);
    resp.addHeader("Content-Type", "text/html");
    std::ostringstream document;
    document << "<!DOCTYPE html><html><head><title>Index of " << prefix << "</title>";
    document << "<style type=\"text/css\">td{min-width:300px}thead{text-align:left}body{font-family:monospace}\n</style>";
    document << "</head>";
    document << "<body><h1>Index of "<< prefix <<"</h1><hr><table>";
    document << "<thead><tr><th>Name</th><th>Size</th><th>Last modified</th></tr></thead>";
    DIR* dir = ::opendir(path.c_str());
    if (dir)
    {
        struct dirent* entry;
        struct stat entryStat{};
        while ((entry = readdir(dir)) != nullptr)
        {
            if (std::string(entry->d_name) == "." || std::string(entry->d_name) == "..")
                continue;
            if (stat(std::string(path + "/" + entry->d_name).c_str(), &entryStat))
            {
#ifndef DEBUG
                std::cout << "[Error] stat reading " << path << entry->d_name << " , error reading directory entry" << std::endl;
                document << "<tr><td><a href=\"" << prefix + "/" + entry->d_name << "\">" << entry->d_name <<"</a></td><td>" << entry->d_reclen <<" byte </td></tr>\n";                
                continue;
#endif /* DEBUG */
            }
            size_t s = entryStat.st_size;
            timespec mtime = entryStat.st_mtim;
            std::tm local_tm = *std::localtime(&mtime.tv_sec);
            document << "<tr><td><a href=\"" << prefix + "/" + entry->d_name << "\">" << entry->d_name <<"</a></td><td>" << s <<" byte </td><td>" << std::put_time(&local_tm, "%Y-%m-%d %H:%M:%S") << "</td></tr>\n";
        }
        ::closedir(dir);
    }
    document << "</table>";
    document << "</body></html>";
    resp.setBody(document.str());
    return resp;
}

Response Response::errorResponse(
    unsigned int code,
    const WebserverSettings *settings,
    const LocationConfig *location)
{
    const std::string *error_path = nullptr;
    if (location)
    {
        auto it = location->error_page.find(code);
        if (it != location->error_page.end())
            error_path = &it->second;
    }
    if (!error_path && settings)
    {
        auto it = settings->error_page.find(code);
        if (it != settings->error_page.end())
            error_path = &it->second;
    }

    if (error_path && settings)
    {
        std::string root = (location && !location->root.empty())
                               ? location->root
                               : settings->root;
        std::string full_path = root + *error_path;

        std::ifstream file(full_path);
        if (file.is_open())
        {
            std::stringstream ss;
            ss << file.rdbuf();
            file.close();

            Response resp;
            resp.setStatus(code);
            resp.setBody(ss.str());
            resp.addHeader("Content-Type", PathUtils::mimeType(full_path));
            return resp;
        }
    }

    return Response::error(code);
}


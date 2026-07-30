#include "ConnectionManager.hpp"
#include "Request.hpp"
#include "HttpResponse.hpp"
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <sstream>

Connection::Connection(int fd, const sockaddr_in& a, const WebserverSettings* s)
    : fd(fd), addr(a), state(READING),
      headers_complete(false), content_length(0),
      bytes_sent(0), keep_alive(false),
      last_active(std::time(nullptr)), settings(s)
{
}

Connection::~Connection()
{
}

ConnectionManager::ConnectionManager()
{
}

ConnectionManager::~ConnectionManager()
{
    for (auto& pair : m_connections)
    {
        ::close(pair.first);
        PollHandler::getInstance().unsubscribe(pair.first);
    }
    m_connections.clear();
}

void ConnectionManager::acceptConnection(int listen_fd, const WebserverSettings* settings)
{
    sockaddr_in client_addr;
    socklen_t   len = sizeof(client_addr);

    int client_fd = accept(listen_fd, reinterpret_cast<sockaddr*>(&client_addr), &len);
    if (client_fd < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
            return;
        std::cerr << "accept() error: " << std::strerror(errno) << std::endl;
        return;
    }

    if (fcntl(client_fd, F_SETFL, O_NONBLOCK) < 0)
    {
        ::close(client_fd);
        return;
    }

    m_connections.emplace(client_fd, Connection(client_fd, client_addr, settings));

    auto& poll = PollHandler::getInstance();
    poll.subscribe_read(client_fd,
        [this, client_fd]() { onClose(client_fd); },
        [this, client_fd]() { onReadable(client_fd); }
    );
}

void ConnectionManager::onReadable(int fd)
{
    auto it = m_connections.find(fd);
    if (it == m_connections.end())
        return;

    Connection& conn = it->second;

    char buf[8192];
    ssize_t n = read(fd, buf, sizeof(buf));

    if (n == 0)
    {
        onClose(fd);
        return;
    }
    if (n < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return;
        onClose(fd);
        return;
    }

    conn.last_active = std::time(nullptr);
    conn.read_buffer.append(buf, static_cast<size_t>(n));

    if (isRequestComplete(conn))
    {
        conn.state = PROCESSING;
        handleRequest(fd);
    }
}

void ConnectionManager::onWritable(int fd)
{
    auto it = m_connections.find(fd);
    if (it == m_connections.end())
        return;

    Connection& conn = it->second;
    if (conn.state != WRITING)
        return;

    size_t remaining = conn.response_buffer.size() - conn.bytes_sent;
    ssize_t n = write(fd, conn.response_buffer.data() + conn.bytes_sent, remaining);

    if (n <= 0)
    {
        onClose(fd);
        return;
    }

    conn.bytes_sent += static_cast<size_t>(n);
    conn.last_active = std::time(nullptr);

    if (conn.bytes_sent >= conn.response_buffer.size())
    {
        if (conn.keep_alive)
        {
            conn.state = READING;
            conn.read_buffer.clear();
            conn.response_buffer.clear();
            conn.bytes_sent = 0;
            conn.headers_complete = false;
            conn.content_length = 0;

            auto& poll = PollHandler::getInstance();
            poll.subscribe_read(conn.fd,
                [this, fd]() { onClose(fd); },
                [this, fd]() { onReadable(fd); }
            );
        }
        else
        {
            onClose(fd);
        }
    }
}

void ConnectionManager::onClose(int fd)
{
    auto it = m_connections.find(fd);
    if (it != m_connections.end())
    {
        ::close(fd);
        PollHandler::getInstance().unsubscribe(fd);
        m_connections.erase(it);
    }
}

void ConnectionManager::closeConnection(int fd)
{
    onClose(fd);
}

bool ConnectionManager::isRequestComplete(const Connection& conn)
{
    if (!conn.headers_complete)
    {
        size_t header_end = conn.read_buffer.find("\r\n\r\n");
        if (header_end == std::string::npos)
            return false;

        const_cast<Connection&>(conn).headers_complete = true;

        std::string header_part = conn.read_buffer.substr(0, header_end);
        size_t pos = header_part.find("Content-Length: ") ? header_part.find("Content-Length: ") : header_part.find("content-length: ") ? header_part.find("content-length: ") : std::string::npos ;
        if (pos != std::string::npos)
        {
            pos += 16;
            size_t end = header_part.find("\r\n", pos);
            std::string len_str = header_part.substr(pos, end - pos);
            const_cast<Connection&>(conn).content_length = std::stoul(len_str);
        }
        else
        {
            const_cast<Connection&>(conn).content_length = 0;
        }
    }

    size_t header_end = conn.read_buffer.find("\r\n\r\n") + 4;
    return conn.read_buffer.size() >= header_end + conn.content_length;
}

static const char* mimeType(const std::string& filename)
{
    auto pos = filename.rfind('.');
    if (pos == std::string::npos) return "application/octet-stream";
    std::string ext = filename.substr(pos);
    if (ext == ".html") return "text/html";
    if (ext == ".htm")  return "text/html";
    if (ext == ".css")  return "text/css";
    if (ext == ".js")   return "application/javascript";
    if (ext == ".png")  return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".gif")  return "image/gif";
    if (ext == ".ico")  return "image/x-icon";
    if (ext == ".txt")  return "text/plain";
    if (ext == ".pdf")  return "application/pdf";
    if (ext == ".json") return "application/json";
    if (ext == ".xml")  return "application/xml";
    if (ext == ".svg")  return "image/svg+xml";
    return "application/octet-stream";
}

static const LocationConfig* matchLocation(
    const std::unordered_map<std::string, LocationConfig>& locations,
    const std::string& url_path)
{
    const LocationConfig* best = nullptr;
    for (const auto& [path, loc] : locations) {
        if (url_path.compare(0, path.size(), path) == 0) {
            if (!best || path.size() > best->path.size())
                best = &loc;
        }
    }
    return best;
}

static Method parseMethod(const std::string& method) {
    if (method == "GET")     return GET;
    if (method == "HEAD")    return HEAD;
    if (method == "POST")    return POST;
    if (method == "PUT")     return PUT;
    if (method == "PATCH")   return PATCH;
    if (method == "OPTIONS") return OPTIONS;
    if (method == "DELETE")  return DELETE;
    return GET;
}

static bool isMethodAllowed(
    const std::string& method,
    const WebserverSettings* settings,
    const LocationConfig* location)
{
    const std::vector<Method>& allowed = (location && !location->methods.empty())
        ? location->methods : settings->methods;
    if (allowed.empty())
        return true;
    Method m = parseMethod(method);
    for (auto a : allowed)
        if (a == m)
            return true;
    return false;
}

static std::string buildAllowHeader(const std::vector<Method>& methods) {
    std::string h;
    for (size_t i = 0; i < methods.size(); ++i) {
        if (i) h += ", ";
        h += method_name(methods[i]);
    }
    return h;
}

HttpResponse ConnectionManager::errorResponse(
    unsigned int code,
    const WebserverSettings* settings,
    const LocationConfig* location)
{
    const std::string* error_path = nullptr;
    if (location) {
        auto it = location->error_page.find(code);
        if (it != location->error_page.end())
            error_path = &it->second;
    }
    if (!error_path) {
        auto it = settings->error_page.find(code);
        if (it != settings->error_page.end())
            error_path = &it->second;
    }

    if (error_path) {
        std::string root = (location && !location->root.empty())
            ? location->root : settings->root;
        std::string full_path = root + *error_path;

        std::ifstream file(full_path);
        if (file.is_open()) {
            std::stringstream ss;
            ss << file.rdbuf();
            file.close();

            HttpResponse resp;
            resp.setStatus(code);
            resp.setBody(ss.str());
            resp.addHeader("Content-Type", mimeType(full_path));
            return resp;
        }
    }

    return HttpResponse::error(code);
}

void ConnectionManager::handleRequest(int fd)
{
    auto it = m_connections.find(fd);
    if (it == m_connections.end())
        return;

    Connection& conn = it->second;

    try
    {
        Request req = Request::fromString(conn.read_buffer);
        std::cout << req.getMethod().c_str() << " " << req.getURL().str();
        const std::string& method = req.getMethod();
        const std::string& url_path = req.getURL().str();
        const LocationConfig* location = matchLocation(conn.settings->locations, url_path);

        if (!isMethodAllowed(method, conn.settings, location))
        {
            const std::vector<Method>& allowed = (location && !location->methods.empty())
                ? location->methods : conn.settings->methods;
            HttpResponse resp = errorResponse(405, conn.settings, location);
            resp.addHeader("Allow", buildAllowHeader(allowed));
            resp.setKeepAlive(false);
            sendResponse(conn, resp);
            return;
        }

        if (method == "GET" || method == "HEAD")
        {
            std::string root = (location && !location->root.empty())
                ? location->root : conn.settings->root;
            std::string path;

            if (url_path.back() == '/')
                path = root + url_path + conn.settings->index;
            else
                path = root + url_path;
            std::ifstream file(path);
            if (file.is_open())
            {
                std::stringstream ss;
                ss << file.rdbuf();
                file.close();

                HttpResponse resp;
                resp.setStatus(200);
                if (method == "GET")
                    resp.setBody(ss.str());
                resp.addHeader("Content-Type", mimeType(path));
                sendResponse(conn, resp);
                return;
            }

            if (url_path.back() == '/' && conn.settings->dirindex)
            {
                sendResponse(conn, HttpResponse::dirindex(root + url_path, url_path));
                return;
            }
            sendResponse(conn, errorResponse(404, conn.settings, location));
        }
        else if (method == "POST")
        {
            std::string contentType = req.getHeader("Content-Type");

            if (contentType.empty() && !req.getBody().empty())
            {
                MissingContentTypePolicy policy = conn.settings->missing_content_type_policy;
                std::string defaultCt = conn.settings->missing_content_type_default;

                if (location && location->missing_content_type_policy != MissingContentTypePolicy::UNSET)
                {
                    policy = location->missing_content_type_policy;
                    if (!location->missing_content_type_default.empty())
                        defaultCt = location->missing_content_type_default;
                }

                switch (policy)
                {
                    case MissingContentTypePolicy::UNSET:
                        break;
                    case MissingContentTypePolicy::REJECT:
                        sendResponse(conn, errorResponse(400, conn.settings, location));
                        return;
                    case MissingContentTypePolicy::DEFAULT:
                        contentType = defaultCt;
                        break;
                }
            }
            sendResponse(conn, errorResponse(501, conn.settings, location));
        }
        else if (method == "DELETE")
        {
            std::string root = (location && !location->root.empty())
                ? location->root : conn.settings->root;
            std::string path;

            if (url_path == "/" || req.getBody().length() != 0)
            {
                sendResponse(conn, errorResponse(403, conn.settings, location));
                return;
            }
            else
                path = root + url_path;

            std::ifstream file(path);
            if (!file.good())
            {
               sendResponse(conn, errorResponse(404, conn.settings, location));
               return;
            }
            file.close();
            if (!std::remove(path.c_str()))
                sendResponse(conn, errorResponse(500, conn.settings, location));
            sendResponse(conn, HttpResponse::error(204));
        }
        else
        {
            sendResponse(conn, errorResponse(501, conn.settings, location));
        }
    }
    catch (const std::exception& e)
    {
        HttpResponse resp = errorResponse(400, conn.settings, nullptr);
        resp.setKeepAlive(false);
        conn.keep_alive = false;
        sendResponse(conn, resp);
    }
}

void ConnectionManager::sendResponse(Connection& conn, const HttpResponse& response)
{
    conn.response_buffer = response.toString();
    conn.bytes_sent = 0;
    conn.state = WRITING;
    
    std::cout << ", sent " << response.getStatus() << std::endl;

    auto& poll = PollHandler::getInstance();
    poll.subscribe_write(conn.fd,
        [this, fd = conn.fd]() { onClose(fd); },
        [this, fd = conn.fd]() { onWritable(fd); }
    );
}

void ConnectionManager::checkTimeouts(int timeout_seconds)
{
    time_t now = std::time(nullptr);

    auto it = m_connections.begin();
    while (it != m_connections.end())
    {
        if (now - it->second.last_active > timeout_seconds)
        {
            int fd = it->first;
            ++it;
            onClose(fd);
        }
        else
        {
            ++it;
        }
    }
}

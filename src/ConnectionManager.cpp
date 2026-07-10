#include "ConnectionManager.hpp"
#include "Request.hpp"
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
        return;

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

    if (n <= 0)
    {
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
        size_t pos = header_part.find("Content-Length: ");
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

static std::string codeToPhrase(int code)
{
    switch (code) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 301: return "Moved Permanently";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 413: return "Payload Too Large";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 505: return "HTTP Version Not Supported";
        default:  return "Unknown";
    }
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

void ConnectionManager::handleRequest(int fd)
{
    auto it = m_connections.find(fd);
    if (it == m_connections.end())
        return;

    Connection& conn = it->second;

    try
    {
        Request req = Request::fromString(conn.read_buffer);

        const std::string& method = req.getMethod();

        if (method == "GET")
        {
            std::string path;
            const std::string& url_path = req.getURL().str();

            // Try location-specific root first
            const std::string* root = &conn.settings->root;
            for (const auto& loc : conn.settings->locations)
            {
                if (url_path.compare(0, loc.second.path.size(), loc.second.path) == 0)
                {
                    if (loc.second.root.has_value())
                        root = &loc.second.root.value();
                    break;
                }
            }

            if (url_path == "/")
            {
                path = *root + "/" + conn.settings->index;
            }
            else
            {
                path = *root + url_path;
            }

            std::ifstream file(path);
            if (!file.is_open())
            {
                buildResponse(conn, 404, "<h1>404 Not Found</h1>");
                return;
            }

            std::stringstream ss;
            ss << file.rdbuf();
            std::string body = ss.str();
            file.close();

            // Build response manually with proper headers
            std::string resp;
            resp += "HTTP/1.1 200 OK\r\n";
            resp += "Content-Type: ";
            resp += mimeType(path);
            resp += "\r\n";
            resp += "Content-Length: ";
            resp += std::to_string(body.size());
            resp += "\r\n";

            std::string connection_header = req.getHeader("Connection");
            if (connection_header == "keep-alive")
            {
                resp += "Connection: keep-alive\r\n";
                conn.keep_alive = true;
            }
            else
            {
                resp += "Connection: close\r\n";
                conn.keep_alive = false;
            }

            resp += "\r\n";
            resp += body;

            conn.response_buffer = resp;
            conn.bytes_sent = 0;
            conn.state = WRITING;

            auto& poll = PollHandler::getInstance();
            poll.subscribe_write(conn.fd,
                [this, fd]() { onClose(fd); },
                [this, fd]() { onWritable(fd); }
            );
        }
        else if (method == "POST")
        {
            buildResponse(conn, 501, "<h1>501 Not Implemented</h1>");
        }
        else if (method == "DELETE")
        {
            buildResponse(conn, 501, "<h1>501 Not Implemented</h1>");
        }
        else
        {
            buildResponse(conn, 501, "<h1>501 Not Implemented</h1>");
        }
    }
    catch (const std::exception& e)
    {
        buildResponse(conn, 400, "<h1>400 Bad Request</h1>");
    }
}

void ConnectionManager::buildResponse(Connection& conn, int status_code, const std::string& body)
{
    std::string resp;
    resp += "HTTP/1.1 ";
    resp += std::to_string(status_code);
    resp += " ";
    resp += codeToPhrase(status_code);
    resp += "\r\n";
    resp += "Content-Type: text/html\r\n";
    resp += "Content-Length: ";
    resp += std::to_string(body.size());
    resp += "\r\n";
    resp += "Connection: close\r\n";
    resp += "\r\n";
    resp += body;

    conn.response_buffer = resp;
    conn.bytes_sent = 0;
    conn.state = WRITING;

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

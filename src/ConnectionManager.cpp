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
        return; // maybe this should throw

    Connection& conn = it->second;

    try
    {
        Request req = Request::fromString(conn.read_buffer);
        std::cout << req.getURL().str() << req.getMethod().c_str() << std::endl;
        const std::string& method = req.getMethod();
        if (method == "GET" || method == "HEAD")
        {
            std::string path;
            const std::string& url_path = req.getURL().str();
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
                path = *root + "/" + conn.settings->index;
            else
                path = *root + url_path;

            std::ifstream file(path);
            if (!file.is_open())
            {
                sendResponse(conn, HttpResponse::error(404));
                return;
            }

            std::stringstream ss;
            ss << file.rdbuf();
            std::string body = ss.str();
            file.close();

            HttpResponse resp;
            resp.setStatus(200);
            if (method == "GET")
                resp.setBody(body);
            resp.addHeader("Content-Type", mimeType(path));

            std::string connection_header = req.getHeader("Connection");
            bool keep_alive = (connection_header == "keep-alive");
            resp.setKeepAlive(keep_alive);
            conn.keep_alive = keep_alive;

            sendResponse(conn, resp);
        }
        else if (method == "POST")
        {
            sendResponse(conn, HttpResponse::error(501));
        }
        else if (method == "DELETE")
        {
            // If file is inside root is checked by the URL parser regex

            std::string path;
            const std::string& url_path = req.getURL().str();
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

            if (url_path == "/" || req.getBody().length() != 0)
            {
                // not allowed to delete directorys or have a body
                sendResponse(conn, HttpResponse::error(403));
                return;
            }
            else
                path = *root + url_path;

            std::ifstream file(path);
            // Check if target is file and exists
            if (!file.good())
            {
                /*
                    we could send 404 -> not found but that might be a security risk,
                    however it is correct, since delete without auth is insecure
                    in itself.
                    OR  a 403 -> not allowed
                */
               sendResponse(conn, HttpResponse::error(404));
               return;
            }
            file.close();
            if (!std::remove(path.c_str()))
                sendResponse(conn, HttpResponse::error(500));
            sendResponse(conn, HttpResponse::error(204));
            // sendResponse(conn, HttpResponse::error(501));
        }
        else
        {
            sendResponse(conn, HttpResponse::error(501));
        }
    }
    catch (const std::exception& e)
    {
        HttpResponse resp = HttpResponse::error(400);
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
    
    std::cout << "Sent " << response.getStatus() << std::endl;

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

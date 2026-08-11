#include "ConnectionManager.hpp"
#include "Request.hpp"
#include "HttpResponse.hpp"
#include "PathUtils.hpp"
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

    if (conn.read_buffer.length() >= conn.settings->max_body_size + 300) // TODO expand with headersize instead of 300 when setting is available.
    {
        sendResponse(conn, HttpResponse::error(413));
        return;
    } 

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

bool ConnectionManager::isRequestComplete(Connection& conn)
{
    if (!conn.headers_complete)
    {
        size_t header_end = conn.read_buffer.find("\r\n\r\n");
        if (header_end == std::string::npos)
            return false;

        conn.headers_complete = true;

        std::string header_part = conn.read_buffer.substr(0, header_end);
        size_t pos = header_part.find("content-length:");
        if (pos != std::string::npos)
        {
            pos += 16;
            size_t end = header_part.find("\r\n", pos);
            std::string len_str = header_part.substr(pos, end - pos);
            conn.content_length = std::stoul(len_str);
        }
        else
        {
            conn.content_length = 0;
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

// Picks the most specific (longest) location whose path is a full-segment
// prefix of url_path - e.g. location "/upload" matches "/upload" and
// "/upload/x", but NOT "/upload.txt" or "/uploadFoo" 
// Mirrors how nginx resolves prefix locations.
static const LocationConfig* matchLocation(const std::string& url_path,
    const std::map<std::string, LocationConfig>& locations)
{
    const LocationConfig* matched = nullptr;
    for (const auto& loc : locations)
    {
        const std::string& consider = loc.second.path;
        // do we have an exact match?
        if (url_path.compare(0, consider.size(), consider) != 0)
            continue;

        bool at_boundary = url_path.size() == consider.size()
            || (!consider.empty() && consider.back() == '/')
            || url_path[consider.size()] == '/';
        if (!at_boundary)
            continue;
        if (!matched || consider.size() > matched->path.size())
            matched = &loc.second;
    }
    return matched;
}

void ConnectionManager::handleRequest(int fd)
{
    auto it = m_connections.find(fd);
    if (it == m_connections.end())
        return;

    Connection& conn = it->second;

    try
    {
        Request req = Request::fromString(conn.read_buffer, conn.settings->max_body_size);

        const std::string& method   = req.getMethod();
        const std::string  url_path = req.getURL().str();

        std::cout << method << " " << url_path << std::endl;

        const LocationConfig* matched = matchLocation(url_path, conn.settings->locations);

        if (method == "GET" || method == "HEAD")
        {
            const std::string* root = &conn.settings->root;
            if (matched && matched->root.has_value())
                root = &matched->root.value();

            std::string clean = PathUtils::stripQuery(url_path);
            bool wants_dir = !clean.empty() && clean.back() == '/';

            std::string path;
            PathUtils::ResolveResult r =
                PathUtils::resolveUnder(*root, clean, "", path);

            if (r == PathUtils::RESOLVE_BAD_PATH)
            {
                sendResponse(conn, HttpResponse::error(400));
                return;
            }
            if (r == PathUtils::RESOLVE_EMPTY)
                path = *root;

            std::string dir_path = path;
            if (wants_dir)
            {
                if (path.empty() || path[path.size() - 1] != '/')
                    path += "/";
                path += conn.settings->index;
            }

            std::ifstream file(path.c_str(), std::ios::binary);
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

            if (wants_dir && conn.settings->dirindex)
            {
                sendResponse(conn, HttpResponse::dirindex(dir_path, clean));
                return;
            }
            sendResponse(conn, HttpResponse::error(404));
        }
        else if (method == "POST")
        {
            std::string contentType = req.getHeader("Content-Type");

            if (contentType.empty() && !req.getBody().empty())
            {
                MissingContentTypePolicy policy = conn.settings->missing_content_type_policy;
                std::string defaultCt = conn.settings->missing_content_type_default;

                if (matched && matched->missing_content_type_policy.has_value())
                {
                    policy = matched->missing_content_type_policy.value();
                    if (matched->missing_content_type_default.has_value())
                        defaultCt = matched->missing_content_type_default.value();
                }

                switch (policy)
                {
                    case MissingContentTypePolicy::REJECT:
                        sendResponse(conn, HttpResponse::error(400));
                        return;
                    case MissingContentTypePolicy::DEFAULT:
                        contentType = defaultCt;
                        break;
                    default:
                        sendResponse(conn, HttpResponse::error(500));
                        return;
                }
            }

            // TODO: add CGI handler when merged

            if (!matched || matched->upload_dir.empty())
            {
                sendResponse(conn, HttpResponse::error(403));
                return;
            }
            std::string dest_path;
            PathUtils::ResolveResult r = PathUtils::resolveUnder(
                    matched->upload_dir, url_path, matched->path, dest_path);
            if (r != PathUtils::RESOLVE_OK)
            {
                sendResponse(conn, HttpResponse::error(400));
                return;
            }

            std::ofstream outfile(dest_path.c_str(),
                                  std::ios::binary | std::ios::trunc);
            if (!outfile.is_open())
            {
                sendResponse(conn, HttpResponse::error(500));
                return;
            }
            outfile.write(req.getBody().data(),
                          static_cast<std::streamsize>(req.getBody().size()));
            outfile.close();

            HttpResponse resp;
            resp.setStatus(201);
            resp.addHeader("Content-Type", "text/html");
            resp.addHeader("Location", PathUtils::stripQuery(url_path));
            resp.setBody("<h1>201 Created</h1>");
            sendResponse(conn, resp);
        }
        else if (method == "DELETE")
        {
            const std::string* base = &conn.settings->root;
            std::string prefix;

            if (matched && !matched->upload_dir.empty())
            {
                base = &matched->upload_dir;
                prefix = matched->path;
            }
            else if (matched && matched->root.has_value())
            {
                base = &matched->root.value();
            }

            std::string clean = PathUtils::stripQuery(url_path);

            if (clean == "/" || !req.getBody().empty())
            {
                sendResponse(conn, HttpResponse::error(403));
                return;
            }

            std::string path;
            PathUtils::ResolveResult r =
                PathUtils::resolveUnder(*base, clean, prefix, path);
            if (r != PathUtils::RESOLVE_OK)
            {
                sendResponse(conn, HttpResponse::error(400));
                return;
            }

            std::ifstream file(path.c_str(), std::ios::binary);
            if (!file.good())
            {
                sendResponse(conn, HttpResponse::error(404));
                return;
            }
            file.close();

            if (std::remove(path.c_str()) != 0)
            {
                sendResponse(conn, HttpResponse::error(500));
                return;
            }

            HttpResponse resp;
            resp.setStatus(204);
            sendResponse(conn, resp);
        }
        else
        {
            sendResponse(conn, HttpResponse::error(501));
        }
    }
    catch (const PayloadTooLargeError& e)
    {
        std::cerr << "413: " << e.what() << std::endl;
        HttpResponse resp = HttpResponse::error(413);
        resp.setKeepAlive(false);
        conn.keep_alive = false;
        sendResponse(conn, resp);
    }
    catch (const std::exception& e)
    {
        std::cerr << "400: " << e.what() << std::endl;
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

#include "ConnectionManager.hpp"
#include "Request.hpp"
#include "Response.hpp"
#include "CGIHandler.hpp"
#include "PathUtils.hpp"
#include "Chunked.hpp"
#include "Defines.hpp"
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <iostream>

ConnectionManager::ConnectionManager()
    : m_connections(), m_sessionManager(), m_requestHandler()
{
}

ConnectionManager::~ConnectionManager()
{
    for (auto &pair : m_connections)
    {
        ::close(pair.first);
        PollHandler::getInstance().unsubscribe(pair.first);
    }
    m_connections.clear();
}

void ConnectionManager::acceptConnection(int listen_fd, const std::vector<const WebserverSettings*>& candidates)
{
    sockaddr_in client_addr;
    socklen_t len = sizeof(client_addr);

    int client_fd = accept(listen_fd, reinterpret_cast<sockaddr *>(&client_addr), &len);
    if (client_fd < 0)
    {
        std::cerr << "accept() error: error accepting new connection" << std::endl;
        return;
    }
    int flags = 0;
#if defined(REAPPLY_SET_FLAGS) && defined(SETFD_ALLOWED)
    flags = fcntl(client_fd, F_GETFD);
    if (fcntl(client_fd, F_SETFD, flags | FD_CLOEXEC) == -1) {
#else
    (void) flags;
    if (fcntl(client_fd, F_SETFD, FD_CLOEXEC) == -1) {
#endif
        std::cerr << "accept() error: setting file descriptor close-on-exec flags" << std::endl;
        ::close(client_fd);
        return;
    }
#ifdef REAPPLY_SET_FLAGS
    flags = fcntl(client_fd, F_GETFL);
    if (fcntl(client_fd, F_SETFL, flags | O_NONBLOCK) == -1)
#else
    if (fcntl(client_fd, F_SETFL, O_NONBLOCK) == -1)
#endif
    {
        std::cerr << "accept() error: setting file descriptor non-blocking flags" << std::endl;
        ::close(client_fd);
        return;
    }

    m_connections.emplace(client_fd, Connection(client_fd, client_addr, candidates));

    auto &poll = PollHandler::getInstance();
    poll.subscribe_read(client_fd, [this, client_fd]()
                        { onClose(client_fd); }, [this, client_fd]()
                        { onReadable(client_fd); });
}

void ConnectionManager::onReadable(int fd)
{
    auto it = m_connections.find(fd);
    if (it == m_connections.end())
        return;

    Connection &conn = it->second;

    char buf[8192];
    ssize_t n = read(fd, buf, sizeof(buf));

    if (n == 0)
    {
        onClose(fd);
        return;
    }
    if (n < 0)
    {
        std::cerr << "read() error: error reading from socket" << std::endl;
        onClose(fd);
        return;
    }

    conn.last_active = std::time(nullptr);
    conn.read_buffer.append(buf, static_cast<size_t>(n));

    if (conn.settings && !conn.headers_complete)
    {
        size_t header_end = conn.read_buffer.find("\r\n\r\n");
        size_t header_len = (header_end != std::string::npos) ? header_end + 4 : conn.read_buffer.size();

        if (header_len > conn.settings->max_header_size)
        {
            Response resp = Response::errorResponse(431, conn.settings, nullptr);
            resp.setKeepAlive(false);
            sendResponse(conn, resp);
            return;
        }
    }

    RequestReadState state = Request::isRequestComplete(conn);
    switch (state)
    {
    case RequestReadState::INCOMPLETE:
        return;
    case RequestReadState::BAD_REQUEST:
    case RequestReadState::PAYLOAD_TOO_LARGE:
    case RequestReadState::NOT_IMPLEMENTED:
    {
        unsigned int code = (state == RequestReadState::PAYLOAD_TOO_LARGE) ? 413
                            : (state == RequestReadState::NOT_IMPLEMENTED) ? 501
                                                                           : 400;
        Response resp = Response::errorResponse(code, conn.settings, nullptr);
        resp.setKeepAlive(false);
        sendResponse(conn, resp);
        return;
    }
    case RequestReadState::EXPECTATION_FAILED:
    {
        Response resp = Response::errorResponse(417, conn.settings, nullptr);
        resp.setKeepAlive(false);
        sendResponse(conn, resp);
        return;
    }
    case RequestReadState::COMPLETE:
        conn.state = PROCESSING;
        handleRequestFD(fd);
        return;
    }
}

void ConnectionManager::onWritable(int fd)
{
    auto it = m_connections.find(fd);
    if (it == m_connections.end())
        return;

    Connection &conn = it->second;
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
            size_t consumed = conn.header_end + 4 + conn.raw_body_length;
            if (conn.read_buffer.size() >= consumed)
                conn.read_buffer.erase(0, consumed);
            else
                conn.read_buffer.clear();

            conn.state = READING;
            conn.response_buffer.clear();
            conn.bytes_sent = 0;
            conn.headers_complete = false;
            conn.content_length = 0;
            conn.raw_body_length = 0;
            conn.header_end = 0;
            conn.chunked = false;
            conn.is_head_request = false;
            conn.sent_100_continue = false;

            if (!conn.read_buffer.empty() && Request::isRequestComplete(conn) == RequestReadState::COMPLETE)
            {
                handleRequestFD(conn.fd);
                return;
            }

            auto &poll = PollHandler::getInstance();
            poll.subscribe_read(conn.fd, [this, fd]()
                                { onClose(fd); }, [this, fd]()
                                { onReadable(fd); });
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

void ConnectionManager::handleRequestFD(int fd)
{
    auto it = m_connections.find(fd);
    if (it == m_connections.end())
        return;

    Connection &conn = it->second;

    try
    {
        Request req = Request::fromString(conn.read_buffer);
        m_requestHandler.handleRequest(conn, req, m_sessionManager, *this);
    }
    catch (const std::runtime_error& e)
    {
        Response resp = Response::errorResponse(400, conn.settings, nullptr);
        resp.setKeepAlive(false);
        conn.keep_alive = false;
        sendResponse(conn, resp);
    }
    catch (const Request::HTTPVersionNotSupportedException& e)
    {
        Response resp = Response::errorResponse(505, conn.settings, nullptr);
        resp.setKeepAlive(false);
        conn.keep_alive = false;
        sendResponse(conn, resp);
    }
    catch (const Request::HTTPMethodNotAllowedException& e)
    {
        Response resp = Response::errorResponse(501, conn.settings, nullptr);
        resp.setKeepAlive(false);
        conn.keep_alive = false;
        sendResponse(conn, resp);
    }
    catch (const std::exception& e)
    {
        Response resp = Response::errorResponse(400, conn.settings, nullptr);
        resp.setKeepAlive(false);
        conn.keep_alive = false;
        sendResponse(conn, resp);
    }
}

bool ConnectionManager::tryCGI(int fd, const std::string &filePath,
                               const std::string &interpreter,
                               const Request &req,
                               const std::string &scriptName,
                               const std::string &pathInfo,
                               const std::string &pathTranslated)
{
    auto it = m_connections.find(fd);
    if (it == m_connections.end())
        return false;
    Connection &conn = it->second;

    try
    {
        conn.cgi_handler = std::make_unique<CGIHandler>(
            filePath, interpreter, req, conn,
            [this, fd]()
            { onCGIComplete(fd); },
            scriptName, pathInfo, pathTranslated);
        return true;
    }
    catch (const std::exception &e)
    {
        return false;
    }
}

void ConnectionManager::onCGIComplete(int fd)
{
    auto it = m_connections.find(fd);
    if (it == m_connections.end())
        return;
    Connection &conn = it->second;
    if (!conn.cgi_handler)
        return;

    Response resp = conn.cgi_handler->buildResponse(conn.settings);
    sendResponse(conn, resp);
}

void ConnectionManager::sendResponse(Connection& conn, const Response& response)
{
    conn.response_buffer = response.toString();
    if (conn.is_head_request)
    {
        size_t header_end = conn.response_buffer.find("\r\n\r\n");
        if (header_end != std::string::npos)
            conn.response_buffer.resize(header_end + 4);
    }
    conn.bytes_sent = 0;
    conn.state = WRITING;

    std::cout << ", sent " << response.getStatus() << std::endl;

    auto &poll = PollHandler::getInstance();
    poll.subscribe_write(conn.fd,
            [this, fd = conn.fd](){ onClose(fd); },
            [this, fd = conn.fd](){ onWritable(fd);}
        );
}

void ConnectionManager::checkTimeouts(int timeout_seconds)
{
    time_t now = std::time(nullptr);

    auto it = m_connections.begin();
    while (it != m_connections.end())
    {
        Connection &conn = it->second;
        if (conn.cgi_handler && !conn.cgi_handler->isDone())
        {
            conn.cgi_handler->checkTimeout(DEFAULT_CGI_TIMEOUT);
        }
        if (now - conn.last_active > timeout_seconds)
        {
            int fd = it->first;
            ++it;
            onClose(fd);
        }
        else
            ++it;
    }
    m_sessionManager.deleteExpiredSessions();
}

#pragma once

#include <map>
#include <vector>
#include "Connection.hpp"
#include "PollHandler.hpp"
#include "Response.hpp"
#include "SessionManager.hpp"
#include "RequestHandler.hpp"

class ConnectionManager {
public:
    ConnectionManager();
    ~ConnectionManager();

    void acceptConnection(int listen_fd, const std::vector<const WebserverSettings*>& candidates);

    void onReadable(int fd);
    void onWritable(int fd);
    void onClose(int fd);

    void checkTimeouts(int timeout_seconds);

    void sendResponse(Connection& conn, const Response& response);
    bool tryCGI(int fd, const std::string& filePath,
                const std::string& interpreter, const Request& req,
                const std::string& scriptName = "",
                const std::string& pathInfo = "",
                const std::string& pathTranslated = "");

private:
    std::map<int, Connection> m_connections;
    SessionManager            m_sessionManager;
    RequestHandler            m_requestHandler;

    void closeConnection(int fd);
    void handleRequestFD(int fd);
    void onCGIComplete(int fd);
};

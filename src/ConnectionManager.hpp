#pragma once

#include <map>
#include "Connection.hpp"
#include "PollHandler.hpp"
#include "HttpResponse.hpp"

class ConnectionManager {
public:
    ConnectionManager();
    ~ConnectionManager();

    void acceptConnection(int listen_fd, const WebserverSettings* settings);

    void onReadable(int fd);
    void onWritable(int fd);
    void onClose(int fd);

    void checkTimeouts(int timeout_seconds);

private:
    std::map<int, Connection> m_connections;

    void    closeConnection(int fd);
    bool    isRequestComplete(const Connection& conn);
    void    handleRequest(int fd);
    void    sendResponse(Connection& conn, const HttpResponse& response);
};

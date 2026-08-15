#pragma once

#include <map>
#include "Connection.hpp"
#include "PollHandler.hpp"
#include "HttpResponse.hpp"

enum class RequestReadState {
    INCOMPLETE,         // wait for more
    COMPLETE,           // ready to be handled
    BAD_REQUEST,        // malformed framing or request smuggling -> 400
    PAYLOAD_TOO_LARGE,  // body exceeds max_body_size             -> 413
    NOT_IMPLEMENTED     // unsupported transfer coding            -> 501
};

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
    RequestReadState isRequestComplete(Connection& conn);

    void    handleRequestFD(int fd);
    void    handleRequest(Connection& conn, const Request& req);

    bool    tryCGI(int fd, const std::string& filePath,
                   const std::string& interpreter, const Request& req);
    void    onCGIComplete(int fd);

    void    handleGet(Connection& conn, const std::string& root,
                      const std::string& url_path, const LocationConfig* location);
    void    handleHead(Connection& conn, const std::string& root,
                       const std::string& url_path, const LocationConfig* location);
    void    handlePost(Connection& conn, const Request& req,
                       const LocationConfig* location);
    void    handleDelete(Connection& conn, const std::string& root,
                         const std::string& url_path, const LocationConfig* location);
    void    handleOptions(Connection& conn, const std::vector<Method>& allowed);

    void    sendResponse(Connection& conn, const HttpResponse& response);
    HttpResponse errorResponse(unsigned int code,
                               const WebserverSettings* settings,
                               const LocationConfig* location);
};

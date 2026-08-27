#pragma once

#include <map>
#include "Connection.hpp"
#include "PollHandler.hpp"
#include "HttpResponse.hpp"
#include "HttpStatusReason.hpp"
#include <unordered_set>
#include "InMemoryDB.hpp"
#include <ctime>

struct SessionInfo
{
    std::string username;
    std::string role;
    time_t      expiresAt;
};

struct UserCredentials
{
    std::string password;
    std::string role;
};

enum class RequestReadState {
    INCOMPLETE,         // wait for more
    COMPLETE,           // ready to be handled
    BAD_REQUEST,        // malformed framing or request smuggling -> 400
    PAYLOAD_TOO_LARGE,  // body exceeds max_body_size             -> 413
    NOT_IMPLEMENTED,    // unsupported transfer coding            -> 501
    EXPECTATION_FAILED  // wrong expection in header              -> 417
};

class ConnectionManager {
public:
    ConnectionManager();
    ~ConnectionManager();

    void acceptConnection(int listen_fd, const std::vector<const WebserverSettings*>& candidates);

    void onReadable(int fd);
    void onWritable(int fd);
    void onClose(int fd);

    void checkTimeouts(int timeout_seconds);

private:
    std::map<int, Connection> m_connections;
    InMemoryDB<std::string, SessionInfo> m_activeSessions;
    InMemoryDB<std::string, UserCredentials> m_userCredentials;

    void    closeConnection(int fd);
    RequestReadState isRequestComplete(Connection& conn);

    void    handleRequestFD(int fd);
    void    handleRequest(Connection& conn, const Request& req);


    bool    tryCGI(int fd, const std::string& filePath,
                   const std::string& interpreter, const Request& req,
                   const std::string& scriptName = "",
                   const std::string& pathInfo = "",
                   const std::string& pathTranslated = "");
    void    onCGIComplete(int fd);

    void    handleGet(Connection& conn, const std::string& root,
                        const std::string& url_path, const LocationConfig* location,
                        const Request& req);
    void    handleHead(Connection& conn, const std::string& root,
                        const std::string& url_path, const LocationConfig* location);
    void    handlePost(Connection& conn, const Request& req,
                        const LocationConfig* location);
    void    handleDelete(Connection& conn, const std::string& root,
                        const std::string& url_path, const LocationConfig* location);
    void    handleOptions(Connection& conn, const std::vector<Method>& allowed);
    void    handlePut(Connection& conn, const std::string& root,
                        const Request &req,
                        const std::string& url_path,
                        const LocationConfig* location);
    void    handlePatch(Connection& conn, const std::string& root,
                        const std::string& url_path,
                        const LocationConfig* location);

    bool    handleRole(Connection& conn, const Request& req, const LocationConfig* location, const std::string& requiredRole);

    bool    tryRedirect(Connection& conn, const LocationConfig* location);

    const WebserverSettings* resolveSettings(Connection& conn, const Request& req) const;

    void    sendResponse(Connection& conn, const HttpResponse& response);
    HttpStatusReason::Code checkRole(const Request &req, const std::string& requiredRole);
    void    handleLogin(Connection& conn, const Request& req);
    void    handleLogout(Connection& conn, const Request& req);
    void    deleteExpiredSessions();
    std::string addCookie(const std::string& username, const std::string& role);
};

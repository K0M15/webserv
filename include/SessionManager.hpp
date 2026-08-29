#pragma once

#include <string>
#include <optional>
#include <vector>
#include <ctime>
#include "InMemoryDB.hpp"
#include "HttpStatusReason.hpp"
#include "Request.hpp"
#include "Response.hpp"
#include "WebserverSettings.hpp"

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

struct SessionCookie
{
    std::string id;
    std::string path;
    int maxAgeSeconds;
};

class SessionManager {
public:
    SessionManager();
    ~SessionManager() = default;

    Response handleLogin(const Request& req, const WebserverSettings* settings);
    Response handleLogout(const Request& req);

    std::string addCookie(const std::string& username, const std::string& role);
    void deleteExpiredSessions();

    HttpStatusReason::Code checkRole(const Request& req, const std::string& requiredRole);
    bool handleRole(const Request& req, const WebserverSettings* settings,
                    const LocationConfig* location, const std::string& requiredRole,
                    Response& outErrorResponse);

    bool hasActiveSession(const std::string& sessionId) const;

private:
    InMemoryDB<std::string, SessionInfo> m_activeSessions;
    InMemoryDB<std::string, UserCredentials> m_userCredentials;

    static std::string generateSessionId();
    static std::string formatCookieHeader(const SessionCookie& cookie);
    static std::string extractField(const std::string& body, const std::string& fieldName);
};


#include "SessionManager.hpp"
#include "PathUtils.hpp"
#include <cstdlib>
#include <sstream>
#include <iomanip>
#include <iostream>

SessionManager::SessionManager()
{
    UserCredentials adminCredentials;
    adminCredentials.password = "finedining";
    adminCredentials.role = "admin";
    m_userCredentials.set("admin", adminCredentials);

    UserCredentials guestCredentials;
    guestCredentials.password = "guestpass";
    guestCredentials.role = "user";
    m_userCredentials.set("guest", guestCredentials);
}

std::string SessionManager::generateSessionId()
{
    int byteLengthSessionID = 8;
    static bool seeded = false;
    if (!seeded)
    {
        std::srand(static_cast<unsigned int>(std::time(nullptr)));
        seeded = true;
    }

    std::ostringstream stream;
    for (int i = 0; i < byteLengthSessionID; ++i)
    {
        unsigned int byte = static_cast<unsigned int>(std::rand() % 256);
        stream << std::hex << std::setw(2) << std::setfill('0') << byte;
    }
    return stream.str();
}

std::string SessionManager::formatCookieHeader(const SessionCookie &cookie)
{
#ifdef DEBUG
    std::cout << " Setting cookie format:\n"
              << "ID: <" << cookie.id << ">"
              << " Path: " << cookie.path
              << " Lifetime: " << cookie.maxAgeSeconds << "s" << std::endl;
#endif
    return "session_id=" + cookie.id +
           "; Path=" + cookie.path +
           "; Max-Age=" + std::to_string(cookie.maxAgeSeconds) +
           "; HttpOnly";
}

std::string SessionManager::extractField(const std::string &body, const std::string &fieldName)
{
    std::string fieldPrefix = fieldName + "=";
    size_t position = 0;

    while (position < body.size())
    {
        size_t ampersandPosition = body.find('&', position);
        std::string pair;
        if (ampersandPosition == std::string::npos)
            pair = body.substr(position);
        else
            pair = body.substr(position, ampersandPosition - position);

        if (pair.compare(0, fieldPrefix.size(), fieldPrefix) == 0)
        {
            // decode % encoded url params
            std::string rawVal = pair.substr(fieldPrefix.size());
            std::string decoded;
            if (PathUtils::urlDecode(rawVal, decoded, true))
                return decoded;
            return rawVal;
        }
        if (ampersandPosition == std::string::npos)
            break;
        position = ampersandPosition + 1;
    }
    return "";
}

Response SessionManager::handleLogin(const Request &req, const WebserverSettings *settings)
{
    std::string username = extractField(req.getBody(), "username");
    std::string password = extractField(req.getBody(), "password");

    std::optional<UserCredentials> credentials = m_userCredentials.get(username);
    if (credentials.has_value() && credentials->password == password)
    {
        Response resp;
        resp.setStatus(200);
        resp.addHeader("Set-Cookie", addCookie(username, credentials->role));
        std::cout << username << " logged in with role: " << credentials->role << std::endl;
        return resp;
    }
    return Response::errorResponse(401, settings, nullptr);
}

Response SessionManager::handleLogout(const Request &req)
{
    std::string sessionID = req.getCookie("session_id");
    if (!sessionID.empty())
        m_activeSessions.del(sessionID);

    SessionCookie cookie;
    cookie.id = "";
    cookie.path = "/";
    cookie.maxAgeSeconds = 0;

    Response resp;
    resp.setStatus(200);
    resp.addHeader("Set-Cookie", formatCookieHeader(cookie));
    resp.setBody("<h1>Logged out</h1>");
    return resp;
}

std::string SessionManager::addCookie(const std::string& username, const std::string& role)
{
    SessionCookie cookie;
    cookie.id = generateSessionId();
    cookie.path = "/";

#ifdef DEBUG
    cookie.maxAgeSeconds = 60;
#else  
    cookie.maxAgeSeconds = 60 * 60 * 24 * 7;
#endif

    SessionInfo info;
    info.username = username;
    info.role = role;
    info.expiresAt = std::time(nullptr) + cookie.maxAgeSeconds;

    m_activeSessions.set(cookie.id, info);

    return formatCookieHeader(cookie);
}

void SessionManager::deleteExpiredSessions()
{
    time_t now = std::time(nullptr);
    std::vector<std::string> sessionIds = m_activeSessions.keys();

    for (size_t i = 0; i < sessionIds.size(); ++i)
    {
        std::optional<SessionInfo> session = m_activeSessions.get(sessionIds[i]);
        if (!session.has_value())
            continue;
#ifdef DEBUG
        std::cout << sessionIds[i] 
                  << " expires in " 
                  << (session->expiresAt - now) 
                  << " seconds." << std::endl;
#endif        
        if (now > session->expiresAt)
            m_activeSessions.del(sessionIds[i]);
    }
}

HttpStatusReason::Code SessionManager::checkRole(const Request &req, const std::string& requiredRole)
{
    std::string sessionID = req.getCookie("session_id");
    if (sessionID.empty())
        return HttpStatusReason::Code::UNAUTHORIZED;
    std::optional<SessionInfo> session = m_activeSessions.get(sessionID);
    if (!session.has_value() || session->username.empty())
        return HttpStatusReason::Code::UNAUTHORIZED;
    if (session->role != requiredRole)
        return HttpStatusReason::Code::FORBIDDEN;
    return HttpStatusReason::Code::OK;
}

bool SessionManager::handleRole(const Request& req, const WebserverSettings* settings,
                               const LocationConfig* location, const std::string& requiredRole,
                               Response& outErrorResponse)
{
    HttpStatusReason::Code authResult = checkRole(req, requiredRole);
    if (authResult != HttpStatusReason::Code::OK)
    {
        outErrorResponse = Response::errorResponse(static_cast<unsigned int>(authResult), settings, location);
        return false;
    }
    return true;
}

bool SessionManager::hasActiveSession(const std::string& sessionId) const
{
    return m_activeSessions.exists(sessionId);
}


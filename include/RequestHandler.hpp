#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include "Request.hpp"
#include "Response.hpp"
#include "Connection.hpp"
#include "WebserverSettings.hpp"
#include "SessionManager.hpp"

class ConnectionManager;

class RequestHandler {
public:
    RequestHandler() = default;
    ~RequestHandler() = default;

    void handleRequest(Connection& conn, const Request& req,
                       SessionManager& sessionManager,
                       ConnectionManager& connManager);

    static const WebserverSettings* resolveSettings(const Connection& conn, const Request& req);
    static const LocationConfig* matchLocation(const std::string& url_path,
                                               const std::unordered_map<std::string, LocationConfig>& locations);
    static std::string resolvePath(const std::string& root,
                                   const std::string& url_path,
                                   const WebserverSettings* settings);
    static Method parseMethod(const std::string& method);
    static bool isMethodAllowed(const std::string& method,
                                const WebserverSettings* settings,
                                const LocationConfig* location);
    static std::string buildAllowHeader(const std::vector<Method>& methods);
    static bool tryRedirect(const Connection& conn, const LocationConfig* location, Response& outResponse);

private:
    void handleGet(Connection& conn, const std::string& root,
                   const std::string& url_path, const LocationConfig* location,
                   const Request& req, SessionManager& sessionManager,
                   ConnectionManager& connManager);
    void handleHead(Connection& conn, const std::string& root,
                    const std::string& url_path, const LocationConfig* location,
                    ConnectionManager& connManager);
    void handlePost(Connection& conn, const Request& req,
                    const LocationConfig* location,
                    ConnectionManager& connManager);
    void handleDelete(Connection& conn, const std::string& root,
                      const std::string& url_path, const LocationConfig* location,
                      ConnectionManager& connManager);
    void handleOptions(Connection& conn, const std::vector<Method>& allowed,
                       ConnectionManager& connManager);
    void handlePut(Connection& conn, const std::string& root,
                   const Request& req, const std::string& url_path,
                   const LocationConfig* location,
                   ConnectionManager& connManager);
    void handlePatch(Connection& conn, const std::string& root,
                     const Request& req, const std::string& url_path,
                     const LocationConfig* location,
                     ConnectionManager& connManager);
};


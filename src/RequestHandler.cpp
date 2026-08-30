#include "RequestHandler.hpp"
#include "ConnectionManager.hpp"
#include "PathUtils.hpp"
#include "Defines.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <sys/stat.h>
#include <unistd.h>

static bool iequals(const std::string& a, const std::string& b)
{
    if (a.size() != b.size())
        return false;
    for (size_t i = 0; i < a.size(); ++i)
    {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i])))
            return false;
    }
    return true;
}

Method RequestHandler::parseMethod(const std::string &method)
{
    if (method == "GET")
        return GET;
    if (method == "HEAD")
        return HEAD;
    if (method == "POST")
        return POST;
    if (method == "PUT")
        return PUT;
    if (method == "PATCH")
        return PATCH;
    if (method == "OPTIONS")
        return OPTIONS;
    if (method == "DELETE")
        return DELETE;
    return GET;
}

bool RequestHandler::isMethodAllowed(
    const std::string &method,
    const WebserverSettings *settings,
    const LocationConfig *location)
{
    const std::vector<Method> &allowed = (location && !location->methods.empty())
                                             ? location->methods
                                             : settings->methods;
    if (allowed.empty())
        return true;
    Method m = parseMethod(method);
    if (m == HEAD)
        m = GET;
    for (auto a : allowed)
        if (a == m)
            return true;
    return false;
}

std::string RequestHandler::buildAllowHeader(const std::vector<Method> &methods)
{
    std::string h;
    for (size_t i = 0; i < methods.size(); ++i)
    {
        if (i)
            h += ", ";
        h += method_name(methods[i]);
    }
    return h;
}

std::string RequestHandler::resolvePath(const std::string &root,
                                        const std::string &url_path,
                                        const WebserverSettings *settings)
{
    if (url_path.empty())
        return root + "/" + settings->index;
    std::string path = url_path;
    if (path.back() == '/')
        path += settings->index; // /dir/ -> /dir/index.html
    std::string out;
    if (PathUtils::resolveUnder(root, path, "", out) != PathUtils::RESOLVE_OK)
        return ""; // unsafe segment -> open fails -> 404
    return out;
}

const LocationConfig *RequestHandler::matchLocation(
    const std::string &url_path,
    const std::unordered_map<std::string, LocationConfig> &locations)
{
    const LocationConfig *matched = nullptr;
    for (const auto &loc : locations)
    {
        const std::string &consider = loc.second.path;
        if (url_path.compare(0, consider.size(), consider) != 0)
            continue;
        bool at_boundary = url_path.size() == consider.size() || (!consider.empty() && consider.back() == '/') || url_path[consider.size()] == '/';
        if (!at_boundary)
            continue;
        if (!matched || consider.size() > matched->path.size())
            matched = &loc.second;
    }
    return matched;
}

bool RequestHandler::tryRedirect(const Connection &conn, const LocationConfig *location, Response &outResponse)
{
    const std::string *target = nullptr;
    if (location && !location->redirect.empty())
        target = &location->redirect;
    else if (!conn.settings->redirect.empty())
        target = &conn.settings->redirect;
    if (!target)
        return false;

    outResponse = Response();
    outResponse.setStatus(301);
    outResponse.addHeader("Location", *target);
    outResponse.setBody("<html><body>Moved Permanently: <a href=\"" + *target + "\">" + *target + "</a></body></html>");
    return true;
}

const WebserverSettings *RequestHandler::resolveSettings(const Connection &conn, const Request &req)
{
    std::string target = req.getHeader("host");
    if (target.find(":") != std::string::npos)
        target = target.substr(0, target.find(":"));
    if (target.empty())
        return conn.candidates[0]; // default
    std::transform(target.begin(), target.end(), target.begin(), [](char c)
                   { return std::tolower(c); });
    for (auto &it : conn.candidates)
    {
        for (auto &name : it->server_name)
        {
            std::string lower;
            lower.reserve(name.size());
            std::transform(name.begin(), name.end(), std::back_inserter(lower),
                           [](unsigned char c)
                           { return std::tolower(c); });
            if (target == lower)
                return it;
        }
    }
    return conn.candidates[0]; // default
}

void RequestHandler::handleRequest(Connection &conn, const Request &req,
                                   SessionManager &sessionManager,
                                   ConnectionManager &connManager)
{
    const std::string conn_hdr = req.getHeader("connection");
    if (req.getVersion() == "HTTP/1.1")
        conn.keep_alive = !iequals(conn_hdr, "close");
    else
        conn.keep_alive = iequals(conn_hdr, "keep-alive");
    conn.settings = resolveSettings(conn, req);
    Method m = parseMethod(req.getMethod());
    conn.is_head_request = (m == HEAD);
    std::string url_path = req.getURL().getPath();
    std::string url_file = url_path.substr(0, url_path.find('?'));

    if (url_file == "/login" && m == POST)
    {
        connManager.sendResponse(conn, sessionManager.handleLogin(req, conn.settings));
        return;
    }
    if (url_file == "/logout" && m == POST)
    {
        connManager.sendResponse(conn, sessionManager.handleLogout(req));
        return;
    }

    const LocationConfig *location = matchLocation(url_file, conn.settings->locations);
    Response redirectResp;
    if (tryRedirect(conn, location, redirectResp))
    {
        connManager.sendResponse(conn, redirectResp);
        return;
    }
    std::string root = (location && !location->root.empty())
                           ? location->root
                           : conn.settings->root;

    if (!isMethodAllowed(req.getMethod(), conn.settings, location))
    {
        const std::vector<Method> &allowed = (location && !location->methods.empty())
                                                 ? location->methods
                                                 : conn.settings->methods;
        Response resp = Response::errorResponse(405, conn.settings, location);
        resp.addHeader("Allow", buildAllowHeader(allowed));
        resp.setKeepAlive(false);
        connManager.sendResponse(conn, resp);
        return;
    }

    std::cout << method_name(m) << " " << url_file;
    auto &interpreters = (location && !location->cgi_ext_interpreter.empty())
                             ? location->cgi_ext_interpreter
                             : conn.settings->cgi_ext_interpreter;
    std::string script_name, path_info, matched_ext;
    if (PathUtils::splitPathInfo(url_file, interpreters, script_name, path_info, matched_ext))
    {
        auto interp = interpreters.find(matched_ext);
        if (interp != interpreters.end())
        {
            std::string script_file_path = root + script_name;
            std::string path_translated = PathUtils::translatePath(root, path_info);
            if (connManager.tryCGI(conn.fd, script_file_path, interp->second, req, script_name, path_info, path_translated))
            {
                std::cout << " (CGI)";
                return;
            }
        }
    }

    try
    {
        switch (m)
        {
        case GET:
            handleGet(conn, root, url_path, location, req, sessionManager, connManager);
            break;
        case HEAD:
            handleHead(conn, root, url_path, location, connManager);
            break;
        case POST:
            handlePost(conn, req, location, connManager);
            break;
        case DELETE:
        {
            Response authErr;
#ifndef DISABLE_DELETE_AUTH
            if (sessionManager.handleRole(req, conn.settings, location, "admin", authErr))
#endif
                handleDelete(conn, root, url_path, location, connManager);
#ifndef DISABLE_DELETE_AUTH
            else
                connManager.sendResponse(conn, authErr);
#endif
            break;
        }
        case PUT:
        {
            Response authErr;
#ifndef DISABLE_PUT_AUTH
            if (sessionManager.handleRole(req, conn.settings, location, "admin", authErr))
#endif
                handlePut(conn, root, req, url_path, location, connManager);
#ifndef DISABLE_PUT_AUTH
            else
                connManager.sendResponse(conn, authErr);
#endif
            break;
        }
        case PATCH:
        {
            Response authErr;
#ifndef DISABLE_PATCH_AUTH
            if (sessionManager.handleRole(req, conn.settings, location, "admin", authErr))
#endif
                handlePatch(conn, root, req, url_path, location, connManager);
#ifndef DISABLE_PATCH_AUTH
            else
                connManager.sendResponse(conn, authErr);
#endif
            break;
        }
        case OPTIONS:
        {
            const std::vector<Method> &allowed = (location && !location->methods.empty())
                                                     ? location->methods
                                                     : conn.settings->methods;
            handleOptions(conn, allowed, connManager);
            break;
        }
        default:
            connManager.sendResponse(conn, Response::errorResponse(501, conn.settings, location));
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        connManager.sendResponse(conn, Response::errorResponse(501, conn.settings, location));
    }
}

void RequestHandler::handleGet(Connection &conn, const std::string &root,
                               const std::string &url_path,
                               const LocationConfig *location,
                               const Request &req,
                               SessionManager &sessionManager,
                               ConnectionManager &connManager)
{
    std::string path = resolvePath(root, url_path, conn.settings);
    std::ifstream file(path);
    if (file.is_open())
    {
        std::stringstream ss;
        ss << file.rdbuf();
        file.close();

        Response resp;
        resp.setStatus(200);
        resp.setBody(ss.str());
        resp.addHeader("Content-Type", PathUtils::mimeType(path));

        struct stat st;
        if (stat(path.c_str(), &st) == 0)
            resp.addHeader("Last-Modified", Response::httpDate(st.st_mtime));

        std::string session_id = req.getCookie("session_id");
        if (session_id.empty() || !sessionManager.hasActiveSession(session_id))
            resp.addHeader("Set-Cookie", sessionManager.addCookie("", ""));

        connManager.sendResponse(conn, resp);
        return;
    }

    if (url_path.back() == '/' && (conn.settings->dirindex || (location && location->dirindex)))
    {
        connManager.sendResponse(conn, Response::dirindex(root + url_path, url_path));
        return;
    }

    if (path.empty())
    {
        connManager.sendResponse(conn, Response::errorResponse(404, conn.settings, location));
        return;
    }

    if (path.back() != '/') {
        std::string check_path = path + "/";
        struct stat buff;
        if (::stat(check_path.c_str(), &buff)) {
            connManager.sendResponse(conn, Response::errorResponse(404, conn.settings, location));
            return;
        }
        if (S_ISDIR(buff.st_mode)) {
            Response resp;
            resp.setStatus(301);
            resp.addHeader("Location", url_path + "/");
            resp.setBody("<html><body>Moved Permanently: <a href=\"" + url_path + "/" + "\">" + url_path + "/" + "</a></body></html>");
            connManager.sendResponse(conn, resp);
            return;
        }
    }
    connManager.sendResponse(conn, Response::errorResponse(404, conn.settings, location));
}

void RequestHandler::handleHead(Connection &conn, const std::string &root,
                                const std::string &url_path,
                                const LocationConfig *location,
                                ConnectionManager &connManager)
{
    std::string path = resolvePath(root, url_path, conn.settings);

    std::ifstream file(path);
    if (file.is_open())
    {
        file.close();

        Response resp;
        struct stat st;
        if (stat(path.c_str(), &st) != 0)
        {
            connManager.sendResponse(conn, Response::errorResponse(500, conn.settings, location));
            return;
        }
        else
        {
            resp.addHeader("Last-Modified", Response::httpDate(st.st_mtime));
        }
        resp.setStatus(200);
        resp.addHeader("Content-Type", PathUtils::mimeType(path));
        resp.addHeader("Content-Length", std::to_string(st.st_size));
        connManager.sendResponse(conn, resp);
        return;
    }

    if (url_path.back() == '/' && (conn.settings->dirindex || (location && location->dirindex)))
    {
        connManager.sendResponse(conn, Response::dirindex(root + url_path, url_path));
        return;
    }

    if (path.empty())
    {
        connManager.sendResponse(conn, Response::errorResponse(404, conn.settings, location));
        return;
    }

    if (path.back() != '/') {
        std::string check_path = path + "/";
        struct stat buff;
        if (::stat(check_path.c_str(), &buff)) {
            connManager.sendResponse(conn, Response::errorResponse(404, conn.settings, location));
            return;
        }
        if (S_ISDIR(buff.st_mode)) {
            Response resp;
            resp.setStatus(301);
            resp.addHeader("Location", url_path + "/");
            resp.setBody("<html><body>Moved Permanently: <a href=\"" + url_path + "/" + "\">" + url_path + "/" + "</a></body></html>");
            connManager.sendResponse(conn, resp);
            return;
        }
    }
    connManager.sendResponse(conn, Response::errorResponse(404, conn.settings, location));
}

void RequestHandler::handlePost(Connection &conn, const Request &req,
                                const LocationConfig *location,
                                ConnectionManager &connManager)
{
    std::string contentType = req.getHeader("Content-Type");

    if (contentType.empty() && !req.getBody().empty())
    {
        MissingContentTypePolicy policy = conn.settings->missing_content_type_policy;
        std::string defaultCt = conn.settings->missing_content_type_default;

        if (location && location->missing_content_type_policy != MissingContentTypePolicy::UNSET)
        {
            policy = location->missing_content_type_policy;
            if (!location->missing_content_type_default.empty())
                defaultCt = location->missing_content_type_default;
        }
        switch (policy)
        {
        case MissingContentTypePolicy::UNSET:
            break;
        case MissingContentTypePolicy::REJECT:
            connManager.sendResponse(conn, Response::errorResponse(400, conn.settings, location));
            return;
        case MissingContentTypePolicy::DEFAULT:
            contentType = defaultCt;
            break;
        }
    }
    if (req.getBody().size() > conn.settings->max_body_size)
    {
        connManager.sendResponse(conn, Response::errorResponse(413, conn.settings, location));
        return;
    }

    if (!location || location->upload_dir.empty())
    {
        connManager.sendResponse(conn, Response::errorResponse(403, conn.settings, location));
        return;
    }

    std::string url_path = PathUtils::stripQuery(req.getURL().getPath());
    std::string dest_path;
    PathUtils::ResolveResult r = PathUtils::resolveUnder(
        location->upload_dir, url_path, location->path, dest_path, false);

    if (r != PathUtils::RESOLVE_OK)
    {
        connManager.sendResponse(conn, Response::errorResponse(400, conn.settings, location));
        return;
    }

    std::ofstream outfile(dest_path, std::ios::binary | std::ios::trunc);
    if (!outfile.is_open())
    {
        connManager.sendResponse(conn, Response::errorResponse(500, conn.settings, location));
        return;
    }
    outfile.write(req.getBody().data(), static_cast<std::streamsize>(req.getBody().size()));
    if (!outfile.good())
    {
        outfile.close();
        std::remove(dest_path.c_str());
        connManager.sendResponse(conn, Response::errorResponse(500, conn.settings, location));
        return;
    }
    outfile.close();
    if (!outfile.good())
    {
        std::remove(dest_path.c_str());
        connManager.sendResponse(conn, Response::errorResponse(500, conn.settings, location));
        return;
    }

    Response resp;
    resp.setStatus(201);
    resp.addHeader("Content-Type", "text/html");
    resp.addHeader("Location", url_path);
    resp.setBody("<h1>201 Created</h1>");
    connManager.sendResponse(conn, resp);
}

void RequestHandler::handleDelete(Connection &conn, const std::string &root,
                                  const std::string &url_path,
                                  const LocationConfig *location,
                                  ConnectionManager &connManager)
{
    if (url_path == "/")
    {
        connManager.sendResponse(conn, Response::errorResponse(403, conn.settings, location));
        return;
    }

    std::string path;
    if (PathUtils::resolveUnder(root, url_path, "", path) != PathUtils::RESOLVE_OK)
    {
        connManager.sendResponse(conn, Response::errorResponse(403, conn.settings, location));
        return;
    }

    std::ifstream file(path);
    if (!file.good())
    {
        connManager.sendResponse(conn, Response::errorResponse(404, conn.settings, location));
        return;
    }
    file.close();

    if (std::remove(path.c_str()) != 0)
    {
        connManager.sendResponse(conn, Response::errorResponse(500, conn.settings, location));
        return;
    }
    connManager.sendResponse(conn, Response::error(204));
}

void RequestHandler::handleOptions(Connection &conn,
                                   const std::vector<Method> &allowed,
                                   ConnectionManager &connManager)
{
    (void)allowed;
    connManager.sendResponse(conn, Response::error(204));
}

void RequestHandler::handlePut(Connection &conn, const std::string &root,
                               const Request &req,
                               const std::string &url_path,
                               const LocationConfig *location,
                               ConnectionManager &connManager)
{
    std::string path;
    if (PathUtils::resolveUnder(root, url_path, "", path) != PathUtils::RESOLVE_OK)
    {
        connManager.sendResponse(conn, Response::errorResponse(403, conn.settings, location));
        return;
    }

    std::ofstream outfile(path, std::ios::binary);
    if (!outfile.good())
    {
        connManager.sendResponse(conn, Response::errorResponse(500, conn.settings, location));
        return;
    }
    outfile.write(req.getBody().data(), req.getBody().size());
    outfile.close();
    if (!outfile.good())
    {
        std::remove(path.c_str());
        connManager.sendResponse(conn, Response::errorResponse(500, conn.settings, location));
        return;
    }
    connManager.sendResponse(conn, Response::error(201));
}

void RequestHandler::handlePatch(Connection &conn, const std::string &root,
                                 const Request &req,
                                 const std::string &url_path,
                                 const LocationConfig *location,
                                 ConnectionManager &connManager)
{
    std::string path;
    if (PathUtils::resolveUnder(root, url_path, "", path) != PathUtils::RESOLVE_OK)
    {
        connManager.sendResponse(conn, Response::errorResponse(403, conn.settings, location));
        return;
    }

    std::ofstream outfile(path, std::ios::binary);
    if (!outfile.good())
    {
        connManager.sendResponse(conn, Response::errorResponse(500, conn.settings, location));
        return;
    }
    outfile.write(req.getBody().data(), req.getBody().size());
    outfile.close();
    if (!outfile.good())
    {
        std::remove(path.c_str());
        connManager.sendResponse(conn, Response::errorResponse(500, conn.settings, location));
        return;
    }
    connManager.sendResponse(conn, Response::error(204));
}


#include "ConnectionManager.hpp"
#include "Request.hpp"
#include "HttpResponse.hpp"
#include "CGIHandler.hpp"
#include "PathUtils.hpp"
#include "Chunked.hpp"
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <sstream>
#include <iterator>
#include <cctype>
#include <algorithm>
#include <charconv>
#include <sys/stat.h>
#include <cstdlib>
#include <iomanip>


#pragma region cookies

struct SessionCookie
{
    std::string id;
    std::string path;
    int         maxAgeSeconds;
};

static std::string generateSessionId()
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

/**
 * helper to concatinate the key value pairs and setting up a proper response to browser.
 */
static std::string formatCookieHeader(const SessionCookie& cookie)
{
    #ifdef DEBUG
    // terminal debug use only
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

#pragma endregion

Connection::Connection(int fd, const sockaddr_in& a, const std::vector<const WebserverSettings*> candidates)
    :   fd(fd), addr(a), state(READING),
        headers_complete(false), content_length(0),
        header_end(0), chunked(false),
        bytes_sent(0), keep_alive(false),
        last_active(std::time(nullptr)), 
        settings(candidates.empty() ? nullptr : candidates.front()),
        candidates(candidates){}

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

void ConnectionManager::acceptConnection(int listen_fd, const std::vector<const WebserverSettings*>& candidates)
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

    m_connections.emplace(client_fd, Connection(client_fd, client_addr, candidates));

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

    // Header flooding guard: while the header block is incomplete, bound how
    // much we are willing to buffer. Body limits are enforced per-frame in
    // isRequestComplete() so chunked framing overhead is not penalised.
    if (conn.settings && !conn.headers_complete &&
        conn.read_buffer.size() > conn.settings->max_header_size + 16384)
    {
        HttpResponse resp = errorResponse(400, conn.settings, nullptr);
        resp.setKeepAlive(false);
        sendResponse(conn, resp);
        return;
    }

    RequestReadState state = isRequestComplete(conn);
    switch (state)
    {
        case RequestReadState::INCOMPLETE:
            return;
        case RequestReadState::BAD_REQUEST:
        case RequestReadState::PAYLOAD_TOO_LARGE:
        case RequestReadState::NOT_IMPLEMENTED:
        {
            unsigned int code = (state == RequestReadState::PAYLOAD_TOO_LARGE) ? 413
                              : (state == RequestReadState::NOT_IMPLEMENTED) ? 501 : 400;
            HttpResponse resp = errorResponse(code, conn.settings, nullptr);
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
            conn.header_end = 0;
            conn.chunked = false;

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

static std::string headerFieldValue(const std::string& lower_haystack, const std::string& name)
{
    std::string needle = "\r\n" + name + ":";
    size_t pos = lower_haystack.find(needle);
    if (pos == std::string::npos)
        return "";
    pos += needle.size();
    size_t end = lower_haystack.find("\r\n", pos);
    if (end == std::string::npos)
        end = lower_haystack.size();
    std::string value = lower_haystack.substr(pos, end - pos);
    size_t b = value.find_first_not_of(" \t");
    size_t e = value.find_last_not_of(" \t");
    if (b == std::string::npos)
        return "";
    return value.substr(b, e - b + 1);
}

RequestReadState ConnectionManager::isRequestComplete(Connection& conn)
{
    if (!conn.headers_complete)
    {
        size_t header_end = conn.read_buffer.find("\r\n\r\n");
        if (header_end == std::string::npos)
            return RequestReadState::INCOMPLETE;

        conn.headers_complete = true;
        conn.header_end = header_end;

        std::string header_part = conn.read_buffer.substr(0, header_end);
        std::transform(header_part.begin(), header_part.end(), header_part.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        std::string cl_value = headerFieldValue(header_part, "content-length");
        std::string te_value = headerFieldValue(header_part, "transfer-encoding");
        if (!te_value.empty() && !cl_value.empty())
            return RequestReadState::BAD_REQUEST; // request smuggling

        if (!te_value.empty())
        {
            if (!iequals(te_value, "chunked"))
                return RequestReadState::NOT_IMPLEMENTED;
            conn.chunked = true;
            conn.content_length = 0;
        }
        else
        {
            conn.chunked = false;
            if (!cl_value.empty())
            {
                unsigned long len{};
                auto r = std::from_chars(cl_value.data(), cl_value.data() + cl_value.size(), len);
                if (r.ec != std::errc() || r.ptr != cl_value.data() + cl_value.size())
                    return RequestReadState::BAD_REQUEST; // malformed content-length
                conn.content_length = static_cast<size_t>(len);
            }
            else
                conn.content_length = 0;
        }
    }

    size_t body_start = conn.header_end + 4;

    if (conn.chunked)
    {
        size_t max = conn.settings ? conn.settings->max_body_size : 0;
        std::string framed = conn.read_buffer.substr(body_start);
        std::string decoded;
        size_t consumed = 0;
        switch (ChunkedBody::decode(framed, max, decoded, consumed))
        {
            case ChunkResult::INCOMPLETE:
                return RequestReadState::INCOMPLETE;
            case ChunkResult::MALFORMED:
                return RequestReadState::BAD_REQUEST;
            case ChunkResult::TOO_LARGE:
                return RequestReadState::PAYLOAD_TOO_LARGE;
            case ChunkResult::COMPLETE:
                return conn.read_buffer.size() >= body_start + consumed
                     ? RequestReadState::COMPLETE
                     : RequestReadState::INCOMPLETE;
        }
    }

    size_t max = conn.settings ? conn.settings->max_body_size : 0;
    if (conn.content_length > max)
        return RequestReadState::PAYLOAD_TOO_LARGE;
    return conn.read_buffer.size() >= body_start + conn.content_length
         ? RequestReadState::COMPLETE
         : RequestReadState::INCOMPLETE;
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

static Method parseMethod(const std::string& method) {
    if (method == "GET")     return GET;
    if (method == "HEAD")    return HEAD;
    if (method == "POST")    return POST;
    if (method == "PUT")     return PUT;
    if (method == "PATCH")   return PATCH;
    if (method == "OPTIONS") return OPTIONS;
    if (method == "DELETE")  return DELETE;
    return GET;
}

static bool isMethodAllowed(
    const std::string& method,
    const WebserverSettings* settings,
    const LocationConfig* location)
{
    const std::vector<Method>& allowed = (location && !location->methods.empty())
        ? location->methods : settings->methods;
    if (allowed.empty())
        return true;
    Method m = parseMethod(method);
    for (auto a : allowed)
        if (a == m)
            return true;
    return false;
}

static std::string buildAllowHeader(const std::vector<Method>& methods) {
    std::string h;
    for (size_t i = 0; i < methods.size(); ++i) {
        if (i) h += ", ";
        h += method_name(methods[i]);
    }
    return h;
}

HttpResponse ConnectionManager::errorResponse(
    unsigned int code,
    const WebserverSettings* settings,
    const LocationConfig* location)
{
    const std::string* error_path = nullptr;
    if (location) {
        auto it = location->error_page.find(code);
        if (it != location->error_page.end())
            error_path = &it->second;
    }
    if (!error_path) {
        auto it = settings->error_page.find(code);
        if (it != settings->error_page.end())
            error_path = &it->second;
    }

    if (error_path) {
        std::string root = (location && !location->root.empty())
            ? location->root : settings->root;
        std::string full_path = root + *error_path;

        std::ifstream file(full_path);
        if (file.is_open()) {
            std::stringstream ss;
            ss << file.rdbuf();
            file.close();

            HttpResponse resp;
            resp.setStatus(code);
            resp.setBody(ss.str());
            resp.addHeader("Content-Type", mimeType(full_path));
            return resp;
        }
    }

    return HttpResponse::error(code);
}

static std::string resolvePath(const std::string& root,
                                const std::string& url_path,
                                const WebserverSettings* settings)
{
    if (url_path.empty())
        return root + "/" + settings->index;
    std::string path = url_path;
    if (path.back() == '/')
        path += settings->index; // /dir/ → /dir/index.html
    std::string out;
    if (PathUtils::resolveUnder(root, path, "", out) != PathUtils::RESOLVE_OK)
        return ""; // unsafe segment → open fails → 404
    return out;
}
// Picks the most specific (longest) location whose path is a full-segment
// prefix of url_path - e.g. location "/upload" matches "/upload" and
// "/upload/x", but NOT "/upload.txt" or "/uploadFoo" 
// Mirrors how nginx resolves prefix locations.
static const LocationConfig* matchLocation(const std::string& url_path,
    const std::unordered_map<std::string, LocationConfig>& locations)
{
    const LocationConfig* matched = nullptr;
    for (const auto& loc : locations)
    {
        const std::string& consider = loc.second.path;
        // do we have an exact match?
        if (url_path.compare(0, consider.size(), consider) != 0)
            continue;
        //
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

void ConnectionManager::handleRequestFD(int fd)
{
    auto it = m_connections.find(fd);
    if (it == m_connections.end())
        return;

    Connection& conn = it->second;

    try
    {
        Request req = Request::fromString(conn.read_buffer);
        handleRequest(conn, req);
    }
    catch (const std::exception& e)
    {
        HttpResponse resp = errorResponse(400, conn.settings, nullptr);
        resp.setKeepAlive(false);
        conn.keep_alive = false;
        sendResponse(conn, resp);
    }
}

void ConnectionManager::handleRequest(Connection& conn, const Request& req)
{
    const std::string keep_alive = req.getHeader("keep-alive");
    if (!keep_alive.empty() && keep_alive == "true")
    {
        int optval = 1;
        int idle = 60;
        int interval = 10;
        int count = 3;
        setsockopt(conn.fd, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));
        setsockopt(conn.fd, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));
        setsockopt(conn.fd, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval));
        setsockopt(conn.fd, IPPROTO_TCP, TCP_KEEPCNT, &count, sizeof(count));
    }
    conn.settings = resolveSettings(conn, req);
    Method m = parseMethod(req.getMethod());
    std::string url_path = req.getURL().str();
    std::string url_file = url_path.substr(0, url_path.find('?'));
    const LocationConfig* location = matchLocation(url_file, conn.settings->locations);
    if (tryRedirect(conn, location))
        return;
    std::string root = (location && !location->root.empty())
                     ? location->root : conn.settings->root;

    if (!isMethodAllowed(req.getMethod(), conn.settings, location))
    {
        const std::vector<Method>& allowed = (location && !location->methods.empty())
            ? location->methods : conn.settings->methods;
        HttpResponse resp = errorResponse(405, conn.settings, location);
        resp.addHeader("Allow", buildAllowHeader(allowed));
        resp.setKeepAlive(false);
        sendResponse(conn, resp);
        return;
    }

    std::cout << method_name(m) << " " << url_file;
    std::string ext = req.getURL().getFileExt();
    auto& interpreters = (location && !location->cgi_ext_interpreter.empty())
                       ? location->cgi_ext_interpreter
                       : conn.settings->cgi_ext_interpreter;
    auto interp = interpreters.find(ext);
    if (interp != interpreters.end())
    {
        if (tryCGI(conn.fd, root + url_file, interp->second, req))
        {
            std::cout << " (CGI)";
            return;
        }
    }

    try
    {
        switch (m)
        {
            case GET:
                handleGet(conn, root, url_path, location, req); break;
            case HEAD:
                handleHead(conn, root, url_path, location); break;
            case POST:
                handlePost(conn, req, location); break;
            case DELETE:
                handleDelete(conn, root, url_path, location); break;
            case OPTIONS:
            {
                const std::vector<Method>& allowed = (location && !location->methods.empty())
                    ? location->methods : conn.settings->methods;
                handleOptions(conn, allowed); break;
            }
            default:
                sendResponse(conn, errorResponse(501, conn.settings, location));
        }
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        sendResponse(conn, errorResponse(501, conn.settings, location));
    }
    
}

bool ConnectionManager::tryCGI(int fd, const std::string& filePath,
                                const std::string& interpreter,
                                const Request& req)
{
    auto it = m_connections.find(fd);
    if (it == m_connections.end()) return false;
    Connection& conn = it->second;

    try
    {
        conn.cgi_handler = std::make_unique<CGIHandler>(
            filePath, interpreter, req, conn,
            [this, fd]() { onCGIComplete(fd); });
        return true;
    }
    catch (const std::exception& e)
    {
        return false;
    }
}

void ConnectionManager::onCGIComplete(int fd)
{
    auto it = m_connections.find(fd);
    if (it == m_connections.end()) return;
    Connection& conn = it->second;
    if (!conn.cgi_handler) return;

    int status = conn.cgi_handler->getExitStatus();
    const std::string& raw = conn.cgi_handler->getOutput();

    HttpResponse resp;
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
    {
        resp = errorResponse(502, conn.settings, nullptr);
    }
    else
    {
        size_t header_end = raw.find("\r\n\r\n");
        std::string body = (header_end != std::string::npos)
                         ? raw.substr(header_end + 4) : raw;

        resp.setStatus(200);
        resp.setBody(body);

        if (header_end != std::string::npos)
        {
            std::string cgi_headers = raw.substr(0, header_end);
            size_t pos = 0;
            while (pos < cgi_headers.size())
            {
                size_t nl = cgi_headers.find("\r\n", pos);
                std::string line = (nl != std::string::npos)
                                 ? cgi_headers.substr(pos, nl - pos)
                                 : cgi_headers.substr(pos);
                size_t colon = line.find(':');
                if (colon != std::string::npos)
                {
                    std::string key = line.substr(0, colon);
                    std::string val = line.substr(colon + 1);
                    size_t first = val.find_first_not_of(' ');
                    if (first != std::string::npos) val = val.substr(first);
                    if (key != "Status" && !key.empty())
                        resp.addHeader(key, val);
                }
                if (nl == std::string::npos) break;
                pos = nl + 2;
            }
        }
    }
    sendResponse(conn, resp);
}

void ConnectionManager::handleGet(Connection& conn, const std::string& root,
                                   const std::string& url_path,
                                   const LocationConfig* location,
                                   const Request& req)
{
    std::string path = resolvePath(root, url_path, conn.settings);
    std::ifstream file(path);
    if (file.is_open())
    {
        std::stringstream ss;
        ss << file.rdbuf();
        file.close();

        HttpResponse resp;
        resp.setStatus(200);
        resp.setBody(ss.str());
        resp.addHeader("Content-Type", mimeType(path));

        struct stat st;
        if (stat(path.c_str(), &st) == 0)
            resp.addHeader("Last-Modified", HttpResponse::httpDate(st.st_mtime));

        std::string session_id = req.getCookie("session_id");
        if (session_id.empty() || m_activeSessions.find(session_id) == m_activeSessions.end())
        {
            SessionCookie cookie;
            cookie.id = generateSessionId();
            cookie.path = "/"; // assigning the cookie to every path
            #ifdef DEBUG
                cookie.maxAgeSeconds = 60; // using this time for testing clarity. Should be much longer.
            #else
                cookie.maxAgeSeconds = 60 * 60 * 24 * 7; // 1 week
            #endif
            m_activeSessions.insert(cookie.id);
            resp.addHeader("Set-Cookie", formatCookieHeader(cookie));
        }
        
        sendResponse(conn, resp);
        return;
    }

    if (url_path.back() == '/' && conn.settings->dirindex)
    {
        sendResponse(conn, HttpResponse::dirindex(root + url_path, url_path));
        return;
    }
    sendResponse(conn, errorResponse(404, conn.settings, location));
}

void ConnectionManager::handleHead(Connection& conn, const std::string& root,
                                    const std::string& url_path,
                                    const LocationConfig* location)
{
    std::string path = resolvePath(root, url_path, conn.settings);

    std::ifstream file(path);
    if (file.is_open())
    {
        file.close();
        HttpResponse resp;
        resp.setStatus(200);
        resp.addHeader("Content-Type", mimeType(path));

        struct stat st;
        if (stat(path.c_str(), &st) == 0)
            resp.addHeader("Last-Modified", HttpResponse::httpDate(st.st_mtime));

        sendResponse(conn, resp);
        return;
    }

    if (url_path.back() == '/' && conn.settings->dirindex)
    {
        sendResponse(conn, HttpResponse::dirindex(root + url_path, url_path));
        return;
    }
    sendResponse(conn, errorResponse(404, conn.settings, location));
}

void ConnectionManager::handlePost(Connection& conn, const Request& req,
                                    const LocationConfig* location)
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
                sendResponse(conn, errorResponse(400, conn.settings, location));
                return;
            case MissingContentTypePolicy::DEFAULT:
                contentType = defaultCt;
                break;
        }
    }
    if (req.getBody().size() > conn.settings->max_body_size)
    {
        sendResponse(conn, errorResponse(413, conn.settings, location));
        return;
    }

    if (!location || location->upload_dir.empty())
    {
        sendResponse(conn, errorResponse(403, conn.settings, location));
        return;
    }

    std::string url_path = PathUtils::stripQuery(req.getURL().str());
    std::string dest_path;
    PathUtils::ResolveResult r = PathUtils::resolveUnder(
            location->upload_dir, url_path, location->path, dest_path, false);

    if (r != PathUtils::RESOLVE_OK)
    {
        sendResponse(conn, errorResponse(400, conn.settings, location));
        return;
    }

    std::ofstream outfile(dest_path, std::ios::binary | std::ios::trunc);
    if (!outfile.is_open())
    {
        sendResponse(conn, errorResponse(500, conn.settings, location));
        return;
    }
    outfile.write(req.getBody().data(), static_cast<std::streamsize>(req.getBody().size()));
    if (!outfile.good())
    {
        outfile.close();
        std::remove(dest_path.c_str());
        sendResponse(conn, errorResponse(500, conn.settings, location));
        return;
    }
    outfile.close();
    if (!outfile.good())
    {
        std::remove(dest_path.c_str());
        sendResponse(conn, errorResponse(500, conn.settings, location));
        return;
    }

    HttpResponse resp;
    resp.setStatus(201);
    resp.addHeader("Content-Type", "text/html");
    resp.addHeader("Location", url_path);
    resp.setBody("<h1>201 Created</h1>");
    sendResponse(conn, resp);
}

void ConnectionManager::handleDelete(Connection& conn, const std::string& root,
                                      const std::string& url_path,
                                      const LocationConfig* location)
{
    if (url_path == "/")
    {
        sendResponse(conn, errorResponse(403, conn.settings, location));
        return;
    }

    std::string path;
    if (PathUtils::resolveUnder(root, url_path, "", path) != PathUtils::RESOLVE_OK)
    {
        sendResponse(conn, errorResponse(403, conn.settings, location));
        return;
    }

    std::ifstream file(path);
    if (!file.good())
    {
        sendResponse(conn, errorResponse(404, conn.settings, location));
        return;
    }
    file.close();

    if (std::remove(path.c_str()) != 0)
    {
        sendResponse(conn, errorResponse(500, conn.settings, location));
        return;
    }
    sendResponse(conn, HttpResponse::error(204));
}

void ConnectionManager::handleOptions(Connection& conn,
                                       const std::vector<Method>& allowed)
{
    std::string allow = allowed.empty()
        ? std::string("GET, HEAD, POST, OPTIONS, DELETE")
        : buildAllowHeader(allowed);

    HttpResponse resp;
    resp.setStatus(204);
    resp.addHeader("Allow", allow);
    resp.addHeader("Content-Length", "0");
    sendResponse(conn, resp);
}

bool ConnectionManager::tryRedirect(Connection& conn, const LocationConfig* location)
{
    const std::string* target = nullptr;
    if (location && !location->redirect.empty())
        target = &location->redirect;
    else if (!conn.settings->redirect.empty())
        target = &conn.settings->redirect;
    if (!target) return false;

    HttpResponse resp;
    resp.setStatus(301);
    resp.addHeader("Location", *target);
    resp.setBody("<html><body>Moved Permanently: <a href=\"" + *target + "\">" + *target + "</a></body></html>");
    sendResponse(conn, resp);
    return true;
}

const WebserverSettings* ConnectionManager::resolveSettings(Connection& conn, const Request& req) const
{
    std::string target = req.getHeader("host");
    // remove port and colon
    if (target.find(":") != std::string::npos)
        target = target.substr(0, target.find(":"));
    if (target.empty())
        return conn.candidates[0]; // for now default
    // lowercase target
    std::transform(target.begin(), target.end(), target.begin(), [](char c){
        return std::tolower(c);
    });
    for (auto& it : conn.candidates)
    {
        for (auto& name : it->server_name)
        {
            std::string lower;
            lower.reserve(name.size());
            std::transform(name.begin(), name.end(), std::back_inserter(lower),
                           [](unsigned char c) { return std::tolower(c); });
            if (target == lower)
                return it;
        }
    }
    return conn.candidates[0]; // for now default
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

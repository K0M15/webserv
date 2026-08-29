#include "CGIHandler.hpp"

#include <unistd.h>     // fork, dup2, pipe, chdir, _exit
#include <fcntl.h>      // F_SETFD, FD_CLOEXEC, F_SETFL, O_NONBLOCK
#include <sys/wait.h>   // waitpid, WNOHANG, WIFEXITED, WEXITSTATUS
#include <sys/socket.h> // inet_ntoa
#include <signal.h>     // kill, SIGKILL
#include <limits.h>     // PATH_MAX
#include <cstdio>       // perror
#include <cstdlib>      // getenv
#include <cstring>      // strerror
#include <map>
#include <charconv>
#include <algorithm>
#include <iostream>
#include "PollHandler.hpp"
#include "PathUtils.hpp"
#include "Connection.hpp"
#include "Defines.hpp"

/*
    SERVER and META Keys
        GATEWAY_INTERFACE    CGI/1.1
        SERVER_SOFTWARE      webserv/0.1
        SERVER_NAME          Host header (port stripped) or settings->server_name[0]
        SERVER_PORT          from the matched ListenDirective (stringified)
        SERVER_PROTOCOL      from the request line (req.getVersion())
    REQUEST keys
        REQUEST_METHOD       req.getMethod()
        REQUEST_URI          raw request target as sent
        QUERY_STRING         everything after '?', raw (req.getURL().getRawQuery())
        SCRIPT_NAME          virtual path of the script
        SCRIPT_FILENAME      filesystem path (m_filePath) — php-cgi requires it
        PATH_INFO            extra path after the script name (eg. script.py/data/dev)
        PATH_TRANSLATED      root + PATH_INFO (may be omitted)
        CONTENT_TYPE         from request header, only if present
        CONTENT_LENGTH       from request header, only if present
        REMOTE_ADDR          client IP
    HTTP keys
        Every request header except Content-Type / Content-Length,
        uppercased with '-' -> '_' and prefixed "HTTP_"
    Others:
        PATH                 interpreter search path for the script itself
        DOCUMENT_ROOT        settings->root
        REDIRECT_STATUS=200  php-cgi refuses to run without this
    
    RFC https://www.rfc-editor.org/info/rfc3875/#section-4
*/

static std::string convHeaderKey(const std::string& value){
    std::string result;
    for (size_t i = 0; i < value.length(); ++i)
    {
        unsigned char c = static_cast<unsigned char>(value[i]);
        if (c == '-') { result += '_'; continue; }
        if (c >= 'a' && c <= 'z') c = static_cast<unsigned char>(c - 'a' + 'A');
        result += static_cast<char>(c);
    }
    return result;
}

static bool iequals(const std::string& a, const std::string& b){
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) return false;
    return true;
}

// Resolve the script to an absolute path when relative
static std::string resolveScriptPath(const std::string& p)
{
    if (p.empty() || p[0] == '/')
        return p;
    const char* pwd = std::getenv("PWD");
    if (pwd)
        return std::string(pwd) + "/" + p;
    return p;
}

void CGIHandler::setEnv(){
    m_env_strings.clear();
    m_env.clear();
    auto add = [&](const std::string& name, const std::string& value){
        if (name.empty()) return;
        m_env_strings.push_back(name + "=" + value);
    };

    add("GATEWAY_INTERFACE", CGI_GATEWAY_INTERFACE);
    add("SERVER_SOFTWARE", APPLICATION_ID);

    std::string host = m_req.getHeader("Host");
    size_t colon = host.find(':');
    std::string server_name = colon != std::string::npos ? host.substr(0, colon) : host;
    if (server_name.empty() && !m_conn.settings->server_name.empty())
        server_name = m_conn.settings->server_name[0];
    add("SERVER_NAME", server_name);

    int port = m_conn.settings->listen.empty() ? 80 : m_conn.settings->listen[0].port;
    add("SERVER_PORT", std::to_string(port));
    add("SERVER_PROTOCOL", m_req.getVersion());

    add("REQUEST_METHOD", m_req.getMethod());
    add("REQUEST_URI", m_req.getURL().str());
    add("QUERY_STRING", m_req.getURL().getRawQuery());
    add("SCRIPT_NAME", m_scriptName);
    add("SCRIPT_FILENAME", resolveScriptPath(m_filePath));
    add("PATH_INFO", m_pathInfo);
    add("PATH_TRANSLATED", m_pathTranslated);

    std::string ct = m_req.getHeader("Content-Type");
    std::string cl = m_req.getHeader("Content-Length");
    if (!ct.empty()) add("CONTENT_TYPE", ct);
    if (cl.empty() && m_req.isChunked())
        cl = std::to_string(m_req.getBody().size());
    if (!cl.empty()) add("CONTENT_LENGTH", cl);

    add("REMOTE_ADDR", inet_ntoa(m_conn.addr.sin_addr));

    for (const auto& [hdr, val] : m_req.getHeaders())
    {
        if (iequals(hdr, "content-type") || iequals(hdr, "content-length"))
            continue;
        add("HTTP_" + convHeaderKey(hdr), val);
    }

    const char* p = std::getenv("PATH");
    add("PATH", p ? p : "/usr/local/bin:/usr/bin:/bin");
    add("DOCUMENT_ROOT", m_conn.settings->root);
    add("REDIRECT_STATUS", "200");

    m_env.reserve(m_env_strings.size() + 1);
    for (auto& e : m_env_strings)
        m_env.push_back(e.data());
    m_env.push_back(nullptr);
}

CGIHandler::CGIHandler(
    const std::string filePath,
    const std::string iPath,
    const Request req,
    const Connection& conn,
    std::function<void()> onComplete,
    const std::string scriptName,
    const std::string pathInfo,
    const std::string pathTranslated
) : m_filePath(filePath), m_iPath(iPath), m_req(req), m_conn(conn),
    m_scriptName(scriptName), m_pathInfo(pathInfo), m_pathTranslated(pathTranslated),
    m_env_strings(), m_env(), m_output(),
    m_maxOutputSize(conn.settings ? conn.settings->max_cgi_output : DEFAULT_MAX_CGI_OUTPUT),
    m_startTime(std::time(nullptr)),
    m_exitStatus(0), m_pid(-1),
    m_stdin_fd(-1), m_stdout_fd(-1),
    m_done(false), m_output_drained(false), m_status_collected(false),
    m_output_exceeded(false), m_timed_out(false),
    m_onComplete(onComplete), m_input_write_offset(0)
{
    if (m_scriptName.empty())
    {
        std::unordered_map<std::string, std::string> interpreters;
        if (m_conn.settings && !m_conn.settings->cgi_ext_interpreter.empty())
            interpreters = m_conn.settings->cgi_ext_interpreter;

        std::string fileExt;
        size_t dot = m_filePath.rfind('.');
        if (dot != std::string::npos)
            fileExt = m_filePath.substr(dot);
        if (!fileExt.empty() && interpreters.find(fileExt) == interpreters.end())
            interpreters[fileExt] = m_iPath;

        std::string s_name, p_info, matched_ext;
        if (PathUtils::splitPathInfo(m_req.getURL().getPath(), interpreters, s_name, p_info, matched_ext))
        {
            m_scriptName = s_name;
            m_pathInfo = p_info;
            if (m_pathTranslated.empty() && !m_pathInfo.empty())
            {
                std::string root = m_conn.settings ? m_conn.settings->root : "";
                m_pathTranslated = PathUtils::translatePath(root, m_pathInfo);
            }
        }
        else
        {
            m_scriptName = PathUtils::stripQuery(m_req.getURL().getPath());
            m_pathInfo = "";
            m_pathTranslated = "";
        }
    }
    else
    {
        if (m_pathTranslated.empty() && !m_pathInfo.empty())
        {
            std::string root = m_conn.settings ? m_conn.settings->root : "";
            m_pathTranslated = PathUtils::translatePath(root, m_pathInfo);
        }
    }
    setEnv();
    spawnCGI();
}

void CGIHandler::spawnCGI(){
    int stdin_pipe[2];
    int stdout_pipe[2];

    if (pipe(stdin_pipe) < 0 || pipe(stdout_pipe) < 0)
        return;

    fcntl(stdin_pipe[0], F_SETFD, FD_CLOEXEC);
    fcntl(stdin_pipe[1], F_SETFD, FD_CLOEXEC);
    fcntl(stdout_pipe[0], F_SETFD, FD_CLOEXEC);
    fcntl(stdout_pipe[1], F_SETFD, FD_CLOEXEC);

    fcntl(m_conn.fd, F_SETFD, FD_CLOEXEC);

    pid_t pid = fork();
    if (pid < 0)
        throw std::runtime_error("CGI Handler: Fork not successful");

    if (pid == 0)
    {
        // child 
        dup2(stdin_pipe[0], STDIN_FILENO);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        close(stdin_pipe[0]);  close(stdin_pipe[1]);
        close(stdout_pipe[0]); close(stdout_pipe[1]);

        std::string absPath = resolveScriptPath(m_filePath);

        // relative paths
        std::string dir = m_filePath.substr(0, m_filePath.find_last_of('/'));
        if (!dir.empty())
            chdir(dir.c_str());
        char* argv[] = { const_cast<char*>(m_iPath.c_str()),
                         const_cast<char*>(absPath.c_str()), nullptr };
        execve(m_iPath.c_str(), argv, m_env.data());

        std::cerr << "CGI Handler child: Failed exec of interpreter" << std::endl;
        _exit(127);
    }

    // parent 
    close(stdin_pipe[0]);
    close(stdout_pipe[1]);
    fcntl(stdin_pipe[1], F_SETFL, O_NONBLOCK);
    fcntl(stdout_pipe[0], F_SETFL, O_NONBLOCK);

    m_pid = pid;
    m_stdout_fd = stdout_pipe[0];
    m_stdin_fd = stdin_pipe[1];

    const std::string& body = m_req.getBody();
    PollHandler& poll = PollHandler::getInstance();

    auto drain = [this](){ // readable && on_close
        if (m_stdout_fd < 0) return;
        char buf[CGI_BUFFER_SIZE];
        ssize_t n;
        while ((n = read(m_stdout_fd, buf, sizeof(buf))) > 0)
        {
            m_output.append(buf, static_cast<size_t>(n));
            if (m_output.size() > m_maxOutputSize)
            {
                m_output_exceeded = true;
                killProcess();
                return;
            }
        }

        if (n == 0) // EOF: child closed stdout (or exited)
        {
            m_output_drained = true;
            if (m_stdout_fd >= 0)
            {
                int fd = m_stdout_fd;
                m_stdout_fd = -1;
                PollHandler::getInstance().unsubscribe(fd);
                close(fd);
            }

            int status = 0;
            pid_t reaped = waitpid(m_pid, &status, WNOHANG);
            if (reaped == m_pid)
            {
                m_exitStatus = status;
                m_status_collected = true;
            }
            finish();
        }
    };

    poll.subscribe_read(m_stdout_fd, drain, drain);

    int stdin_fd = m_stdin_fd;
    poll.subscribe_write(m_stdin_fd,
        [stdin_fd](){ // close
            PollHandler::getInstance().unsubscribe(stdin_fd);
        },
        [this, body, stdin_fd](){ // writeable
            if (m_stdin_fd < 0) return;
            while (this->m_input_write_offset < body.size())
            {
                ssize_t n = write(stdin_fd, body.data() + this->m_input_write_offset, body.size() - this->m_input_write_offset);
                if (n <= 0) return;
                this->m_input_write_offset += static_cast<size_t>(n);
            }
            m_stdin_fd = -1;
            PollHandler::getInstance().unsubscribe(stdin_fd);
            close(stdin_fd); // close to start script
        }
    );
}

void CGIHandler::killProcess()
{
    if (m_stdout_fd >= 0)
    {
        int fd = m_stdout_fd;
        m_stdout_fd = -1;
        PollHandler::getInstance().unsubscribe(fd);
        close(fd);
    }
    if (m_stdin_fd >= 0)
    {
        int fd = m_stdin_fd;
        m_stdin_fd = -1;
        PollHandler::getInstance().unsubscribe(fd);
        close(fd);
    }
    if (m_pid > 0)
    {
        kill(m_pid, SIGKILL);
        int status = 0;
        waitpid(m_pid, &status, WNOHANG);
        m_exitStatus = status;
        m_status_collected = true;
    }
    m_output_drained = true;
    finish();
}

bool CGIHandler::checkTimeout(int timeout_seconds)
{
    if (m_done)
        return false;

    if (!m_status_collected && m_pid > 0)
    {
        int status = 0;
        pid_t reaped = waitpid(m_pid, &status, WNOHANG);
        if (reaped == m_pid)
        {
            m_exitStatus = status;
            m_status_collected = true;
            if (m_output_drained)
                finish();
        }
    }

    time_t now = std::time(nullptr);
    if (now - m_startTime >= timeout_seconds)
    {
        m_timed_out = true;
        killProcess();
        return true;
    }
    return false;
}

void CGIHandler::finish()
{
    if (m_done)
        return;
    if (m_output_drained && !m_status_collected && m_pid > 0)
    {
        int status = 0;
        pid_t reaped = waitpid(m_pid, &status, WNOHANG);
        if (reaped == m_pid)
        {
            m_exitStatus = status;
            m_status_collected = true;
        }
    }
    if (!m_output_drained || !m_status_collected)
        return;
    m_done = true;
    m_onComplete();
}

CGIHandler::~CGIHandler()
{
    if (m_stdin_fd >= 0)
    {
        int fd = m_stdin_fd;
        m_stdin_fd = -1;
        PollHandler::getInstance().unsubscribe(fd);
        close(fd);
    }
    if (m_stdout_fd >= 0)
    {
        int fd = m_stdout_fd;
        m_stdout_fd = -1;
        PollHandler::getInstance().unsubscribe(fd);
        close(fd);
    }
    if (m_pid > 0 && !m_status_collected)
    {
        kill(m_pid, SIGKILL);
        waitpid(m_pid, nullptr, WNOHANG);
    }
}

static bool cgi_iequals(const std::string& a, const std::string& b)
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

Response CGIHandler::buildResponse(const WebserverSettings* settings) const
{
    Response resp;
    if (!WIFEXITED(m_exitStatus) || WEXITSTATUS(m_exitStatus) != 0 || m_output_exceeded || m_timed_out)
    {
        int errCode = m_timed_out ? 504 : 502;
        return Response::errorResponse(errCode, settings, nullptr);
    }

    // https://www.rfc-editor.org/info/rfc3875/#section-6.2  
    size_t header_end = m_output.find("\r\n\r\n");
    size_t header_len = 4;
    if (header_end == std::string::npos)
    {
        header_end = m_output.find("\n\n");
        header_len = 2;
    }

    std::string body = (header_end != std::string::npos)
                           ? m_output.substr(header_end + header_len)
                           : m_output;

    resp.setStatus(200);
    resp.setBody(body);

    bool status_set = false;
    bool has_location = false;

    if (header_end != std::string::npos)
    {
        std::string cgi_headers = m_output.substr(0, header_end);
        size_t pos = 0;
        while (pos < cgi_headers.size())
        {
            size_t nl = cgi_headers.find('\n', pos);
            std::string line = (nl != std::string::npos)
                                   ? cgi_headers.substr(pos, nl - pos)
                                   : cgi_headers.substr(pos);
            if (!line.empty() && line.back() == '\r')
                line.pop_back();

            size_t colon = line.find(':');
            if (colon != std::string::npos)
            {
                std::string key = line.substr(0, colon);
                std::string val = line.substr(colon + 1);

                size_t k_first = key.find_first_not_of(" \t");
                size_t k_last = key.find_last_not_of(" \t");
                if (k_first != std::string::npos)
                    key = key.substr(k_first, (k_last - k_first + 1));

                size_t v_first = val.find_first_not_of(" \t");
                size_t v_last = val.find_last_not_of(" \t");
                if (v_first != std::string::npos)
                    val = val.substr(v_first, (v_last - v_first + 1));
                else
                    val = "";

                if (cgi_iequals(key, "status"))
                {
                    unsigned long code = 0;
                    auto r = std::from_chars(val.data(), val.data() + val.size(), code);
                    if (r.ec == std::errc() && code >= 100 && code <= 599)
                    {
                        resp.setStatus(static_cast<int>(code));
                        status_set = true;
                    }
                }
                else
                {
                    if (cgi_iequals(key, "location"))
                        has_location = true;
                    if (!key.empty())
                        resp.addHeader(key, val);
                }
            }
            if (nl == std::string::npos)
                break;
            pos = nl + 1;
        }
    }
    if (has_location && !status_set)
    {
        resp.setStatus(302);
    }
    return resp;
}

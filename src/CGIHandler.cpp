#include "CGIHandler.hpp"

#include <unistd.h>     // fork, execvpe, dup2, pipe2, chdir, _exit
#include <fcntl.h>      // O_CLOEXEC, F_SETFD, FD_CLOEXEC
#include <sys/wait.h>   // waitpid
#include <sys/signalfd.h> // signalfd
#include <sys/socket.h> // inet_ntoa needs arpa/inet.h; Connection.hpp pulls it in
#include <signal.h>     // sigemptyset, sigaddset, sigprocmask
#include <limits.h>     // PATH_MAX
#include <cstdio>       // perror
#include <cstdlib>      // getenv
#include <cstring>      // strerror
#include <map>
#include "PollHandler.hpp"

std::map<pid_t, std::function<void(int)>> g_pending;
int g_sig_fd = -1;

void setupSignalfd()
{
    if (g_sig_fd >= 0)
        return;

    // Block SIGCHLD from normal handling so it is queued for the signalfd
    // instead of interrupting poll() or invoking a signal handler.
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &mask, nullptr);

    g_sig_fd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
    if (g_sig_fd < 0)
        throw std::runtime_error("CGI Handler: signalfd() failed");

    PollHandler::getInstance().subscribe_read(g_sig_fd,
        []() {},
        []() {
            // Consume the notification (may represent several children).
            struct signalfd_siginfo info;
            if (read(g_sig_fd, &info, sizeof(info)) != sizeof(info))
                return;

            // Reap every child that exited since the last poll.
            int status;
            pid_t pid;
            while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
            {
                auto it = g_pending.find(pid);
                if (it != g_pending.end())
                {
                    auto cb = it->second;
                    g_pending.erase(it);
                    cb(status);
                }
            }
        }
    );
}

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
        SCRIPT_FILENAME      absolute filesystem path (m_filePath) — php-cgi requires it
        PATH_INFO            extra path after the script name ("" for now)
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
*/

static std::string convHeaderKey(const std::string& value){
    std::string result;
    for (size_t i = 0; i < value.length(); ++i)
    {
        // use static cast<unsigned char> to resolve negative chars
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

void CGIHandler::setEnv(){
    m_env_strings.clear();
    m_env.clear();
    auto add = [&](const std::string& name, const std::string& value){
        if (name.empty()) return;
        m_env_strings.push_back(name + "=" + value);
    };

    add("GATEWAY_INTERFACE", "CGI/1.1");
    add("SERVER_SOFTWARE", "webserv/0.1");

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
    add("SCRIPT_NAME", m_req.getURL().str().substr(0, m_req.getURL().str().find('?')));
    add("SCRIPT_FILENAME", m_filePath);
    add("PATH_INFO", "");
    add("PATH_TRANSLATED", "");

    std::string ct = m_req.getHeader("Content-Type");
    std::string cl = m_req.getHeader("Content-Length");
    if (!ct.empty()) add("CONTENT_TYPE", ct);
    if (!cl.empty()) add("CONTENT_LENGTH", cl);

    add("REMOTE_ADDR", inet_ntoa(m_conn.addr.sin_addr));

    for (const auto& [hdr, val] : m_req.getHeaders())
    {
        if (iequals(hdr, "Content-Type") || iequals(hdr, "Content-Length"))
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

void CGIHandler::spawnCGI(){
    int stdin_pipe[2];
    int stdout_pipe[2];

    if (pipe2(stdin_pipe, O_CLOEXEC) < 0 || pipe2(stdout_pipe, O_CLOEXEC) < 0)
        return;

    // Do not leak the client socket into the CGI child (it would keep the
    // connection open after the script exits). fcntl(-1,...) is a harmless EBADF.
    // we should do that on all the fds on opening them
    fcntl(m_conn.fd, F_SETFD, FD_CLOEXEC);

    pid_t pid = fork();
    if (pid < 0)
        throw std::runtime_error("CGI Handler: Fork not successful");
    setupSignalfd();
    

    if (pid == 0)
    {
        // child 
        dup2(stdin_pipe[0], STDIN_FILENO);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        close(stdin_pipe[0]);  close(stdin_pipe[1]);
        close(stdout_pipe[0]); close(stdout_pipe[1]);

        char abs_buf[PATH_MAX];
        std::string absPath = m_filePath;
        if (realpath(m_filePath.c_str(), abs_buf))
            absPath = abs_buf;

        // relative paths
        std::string dir = m_filePath.substr(0, m_filePath.find_last_of('/'));
        if (!dir.empty())
            chdir(dir.c_str());
        char* argv[] = { const_cast<char*>(m_iPath.c_str()),
                         const_cast<char*>(absPath.c_str()), nullptr };
        execve(m_iPath.c_str(), argv, m_env.data());

        std::cerr << ("CGI Handler child: Failed exec of interpreter");
        _exit(127);
    }

    // parent 
    close(stdin_pipe[0]);
    close(stdout_pipe[1]);
    fcntl(stdin_pipe[1], F_SETFL, O_NONBLOCK);
    fcntl(stdout_pipe[0], F_SETFL, O_NONBLOCK);

    m_pid = pid;

    // Setup exit handler
    g_pending[m_pid] = [this](int status){
        m_exitStatus = status;
        m_status_collected = true;
        finish();
    };

    const std::string& body = m_req.getBody();
    PollHandler& poll = PollHandler::getInstance();


    auto drain = [this, stdout_pipe](){ // readable && on_close
        char buf[4096];
        ssize_t n;
        while ((n = read(stdout_pipe[0], buf, sizeof(buf))) > 0)
            m_output.append(buf, static_cast<size_t>(n));

        if (n == 0) // EOF: child closed stdout (or exited)
        {
            m_output_drained = true;
            close(stdout_pipe[0]);
            PollHandler::getInstance().unsubscribe(stdout_pipe[0]);

            // Common case: the child already exited, reap it right now.
            pid_t reaped = waitpid(m_pid, &m_exitStatus, WNOHANG);
            if (reaped == m_pid)
            {
                g_pending.erase(m_pid);
                m_status_collected = true;
            }
            finish();
        }
    };

    poll.subscribe_read(stdout_pipe[0], drain, drain);

    poll.subscribe_write(stdin_pipe[1],
        [stdin_pipe](){ // close
            PollHandler::getInstance().unsubscribe(stdin_pipe[1]);
        },
        [body, stdin_pipe](){ // writeable
            size_t off = 0;
            while (off < body.size())
            {
                ssize_t n = write(stdin_pipe[1], body.data() + off, body.size() - off);
                if (n <= 0) break;
                off += static_cast<size_t>(n);
            }
            close(stdin_pipe[1]); // close to start script
            PollHandler::getInstance().unsubscribe(stdin_pipe[1]);
        }
    );
}

void CGIHandler::finish()
{
    if (m_done || !m_output_drained || !m_status_collected)
        return;
    m_done = true;
    m_onComplete();
}

CGIHandler::~CGIHandler()
{
    if (m_pid > 0)
        g_pending.erase(m_pid);
}

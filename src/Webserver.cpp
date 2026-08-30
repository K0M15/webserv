#include "Webserver.hpp"
#include "Defines.hpp"
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <iostream>
#include <csignal>

static Webserver* g_server = nullptr;

static void signalHandler(int sig)
{
    (void)sig;
    if (g_server)
        g_server->stop();
}

Webserver::Webserver(const std::string& config_path)
    : m_config(config_path), m_conn_manager(), m_running(false)
{
    g_server = this;
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    std::signal(SIGPIPE, SIG_IGN);

    setupListenSockets();
}

Webserver::~Webserver()
{
    stop();
    if (g_server == this)
        g_server = nullptr;
    for (const auto& pair : m_listen_fds)
    {
        PollHandler::getInstance().unsubscribe(pair.second);
        ::close(pair.second);
    }
    m_listen_fds.clear();
}

void Webserver::stop()
{
    m_running = false;
}

int Webserver::createListenSocket(const ListenDirective& ld)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
    {
        std::cerr << "socket() failed: error creating listening socket" << std::endl;
        return -1;
    }

    int opt = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        std::cerr << "setsockopt(SO_REUSEADDR) failed: error setting socket options" << std::endl;
        ::close(fd);
        return -1;
    }

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(ld.port));

    if (ld.address.empty() || ld.address == DEFAULT_LISTEN_ADDRESS)
        addr.sin_addr.s_addr = INADDR_ANY;
    else
    {
        struct addrinfo hints;
        struct addrinfo* res = nullptr;
        std::memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_NUMERICHOST;

        if (getaddrinfo(ld.address.c_str(), nullptr, &hints, &res) != 0 || !res)
        {
            std::cerr << "getaddrinfo() failed for address: " << ld.address << std::endl;
            ::close(fd);
            return -1;
        }
        struct sockaddr_in* ipv4 = reinterpret_cast<struct sockaddr_in*>(res->ai_addr);
        addr.sin_addr = ipv4->sin_addr;
        freeaddrinfo(res);
    }

    if (bind(fd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) < 0)
    {
        std::cerr << "bind() failed on " << ld.address << ":" << ld.port << "error binding socket" << std::endl;
        ::close(fd);
        return -1;
    }

    if (listen(fd, SOMAXCONN) < 0)
    {
        std::cerr << "listen() failed: error listening on socket" << std::endl;
        ::close(fd);
        return -1;
    }
    int flags = 0;
#if defined(REAPPLY_SET_FLAGS) && defined(SETFD_ALLOWED)
    flags = fcntl(fd, GETFD);
    if (fcntl(fd, F_SETFD, flags | FD_CLOEXEC) < 0)
#else
    (void) flags;
    if (fcntl(fd, F_SETFD, FD_CLOEXEC) < 0)
#endif
    {
        std::cerr << "fcntl(FD_CLOEXEC) failed: error setting listening socket file descriptor flags" << std::endl;
        ::close(fd);
        return -1;
    }
#ifdef REAPPLY_SET_FLAGS
    flags = fcntl(fd, F_GETFL);
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
#else
    if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0)
#endif
    {
        std::cerr << "fcntl(O_NONBLOCK) failed: error setting listening socket file descriptor flags" << std::endl;
        ::close(fd);
        return -1;
    }   

    std::cout << "Listening on " << ld.address << ":" << ld.port << std::endl;
    return fd;
}

void Webserver::setupListenSockets()
{
    for (auto& entry : m_config.getAllServers())
    {
        WebserverSettings* settings = entry.get();

        for (const auto& ld : settings->listen)
        {
            std::string key = (ld.address.empty() ? DEFAULT_LISTEN_ADDRESS : ld.address)
                            + ":" + std::to_string(ld.port);

            int fd = -1;
            auto it = m_listen_fds.find(key);
            if (it != m_listen_fds.end())
            {
                fd = it->second;
            }
            else
            {
                fd = createListenSocket(ld);
                if (fd < 0)
                    continue;
                m_listen_fds[key] = fd;
            }

            auto& vec = m_socket_settings[fd];
            if (ld.is_default)
            {
                if (!m_default_keys.insert(key).second)
                    throw HttpServerException("duplicate default_server for " + key);
                vec.insert(vec.begin(), settings);
            }
            else
            {
                vec.push_back(settings);
            }
        }
    }
}

void Webserver::run()
{
    if (m_listen_fds.empty())
    {
        std::cerr << "No listen sockets configured. Check your config file." << std::endl;
        return;
    }

    auto& poll = PollHandler::getInstance();
    m_running = true;

    for (const auto& pair : m_listen_fds)
    {
        int listen_fd = pair.second;

        poll.subscribe_read(listen_fd,
            nullptr,
            [this, listen_fd]() {
                auto it = m_socket_settings.find(listen_fd);
                if (it == m_socket_settings.end() || it->second.empty())
                    return;

                // use all blocks for this fd
                m_conn_manager.acceptConnection(listen_fd, it->second);
            }
        );
    }

    std::cout << "Server running. Press Ctrl+C to stop." << std::endl;

    while (m_running)
    {
        try
        {
            poll.checkFDs();
            m_conn_manager.checkTimeouts(DEFAULT_KEEP_ALIVE_TIMEOUT);
        }
        catch (const std::exception& e)
        {
            std::cerr << "Event loop error: " << e.what() << std::endl;
        }
    }

    std::cout << "\nServer shutting down." << std::endl;
}

#include "Webserver.hpp"
#include <sys/socket.h>
#include <arpa/inet.h>
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

    setupListenSockets();
}

Webserver::~Webserver()
{
    for (const auto& pair : m_listen_fds)
    {
        PollHandler::getInstance().unsubscribe(pair.second);
        ::close(pair.second);
    }
    m_listen_fds.clear();
    g_server = nullptr;
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

    if (ld.address.empty() || ld.address == "0.0.0.0")
        addr.sin_addr.s_addr = INADDR_ANY;
    else if (inet_pton(AF_INET, ld.address.c_str(), &addr.sin_addr) <= 0)
    {
        std::cerr << "inet_pton() failed for address: " << ld.address << std::endl;
        ::close(fd);
        return -1;
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

    if (fcntl(fd, F_SETFL, O_NONBLOCK | O_CLOEXEC ) < 0)
    {
        std::cerr << "fcntl(O_NONBLOCK | O_CLOEXEC) failed: error setting listening socket file descriptor flags" << std::endl;
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
            std::string key = (ld.address.empty() ? "0.0.0.0" : ld.address)
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
            m_conn_manager.checkTimeouts(60);
        }
        catch (const std::exception& e)
        {
            std::cerr << "Event loop error: " << e.what() << std::endl;
        }
    }

    std::cout << "\nServer shutting down." << std::endl;
}

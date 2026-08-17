#pragma once

#include <map>
#include <set>
#include <vector>
#include <string>
#include <netinet/in.h>
#include "ConfigReader.h"
#include "ConnectionManager.hpp"
#include "PollHandler.hpp"

class Webserver {
public:
    explicit Webserver(const std::string& config_path);
    ~Webserver();

    void run();
    void stop();

private:
    ConfigReader                    m_config;
    ConnectionManager               m_conn_manager;
    std::map<std::string, int>      m_listen_fds;
    std::map<int, std::vector<const WebserverSettings*>> m_socket_settings;
    std::set<std::string>           m_default_keys;
    volatile bool                   m_running;

    int  createListenSocket(const ListenDirective& ld);
    void setupListenSockets();
};

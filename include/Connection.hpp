#pragma once

#include <arpa/inet.h>
#include <string>
#include <ctime>
#include <Request.hpp>
#include <WebserverSettings.hpp>

enum ConnectionState {
    READING,
    PROCESSING,
    WRITING,
    CLOSED
};

struct Connection {
    int                 fd;
    sockaddr_in         addr;
    ConnectionState     state;

    std::string         read_buffer;
    bool                headers_complete;
    size_t              content_length;

    std::string         response_buffer;
    size_t              bytes_sent;

    bool                keep_alive;
    time_t              last_active;
    const WebserverSettings* settings;

    Connection(int fd, const sockaddr_in& a, const WebserverSettings* s);
    ~Connection();
};

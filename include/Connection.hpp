#pragma once

#include <arpa/inet.h>
#include <vector>
#include <string>
#include <ctime>
#include <memory>
#include <Request.hpp>
#include <WebserverSettings.hpp>

class CGIHandler;

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
    size_t              header_end;
    bool                chunked;
    bool                sent_100_continue;

    std::string         response_buffer;
    size_t              bytes_sent;

    bool                keep_alive;
    time_t              last_active;
    const WebserverSettings* settings;
    std::vector<const WebserverSettings*> candidates;

    std::unique_ptr<CGIHandler> cgi_handler;
    Connection(int fd, const sockaddr_in& a, const std::vector<const WebserverSettings*> candidates);
};

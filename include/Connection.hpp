#pragma once

#include <netinet/in.h>
#include <vector>
#include <string>
#include <ctime>
#include <memory>
#include "Request.hpp"
#include "WebserverSettings.hpp"
#include "CGIHandler.hpp"

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
    size_t              raw_body_length;
    size_t              header_end;
    bool                chunked;
    bool                is_head_request;
    bool                sent_100_continue;

    std::string         response_buffer;
    size_t              bytes_sent;

    bool                keep_alive;
    time_t              last_active;
    const WebserverSettings* settings;
    std::vector<const WebserverSettings*> candidates;
    std::unique_ptr<CGIHandler> cgi_handler;

    inline Connection(int fd, const sockaddr_in& a, const std::vector<const WebserverSettings*> candidates)
    :   fd(fd), addr(a), state(READING),
        headers_complete(false), content_length(0), raw_body_length(0),
        header_end(0), chunked(false), is_head_request(false), sent_100_continue(false),
        bytes_sent(0), keep_alive(true),
        last_active(std::time(nullptr)),
        settings(candidates.empty() ? nullptr : candidates.front()),
        candidates(candidates) {}
};

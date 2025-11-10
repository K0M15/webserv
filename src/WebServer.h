
#pragma once

#include <cstdint>
#include "defines.h"

class WebServer{
private:
    int    ip;
    int    port;
    int    sock;
    WebServer(void);
public:
    WebServer(int ip, uint16_t port);
    WebServer(const WebServer&    other);
    WebServer&  operator=(const WebServer&    other);
    ~WebServer(void);
    int serve(void);
    bool get_open(void);
};

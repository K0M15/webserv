#include "WebServer.h"
#include "unistd.h"
#include "sys/socket.h"
#include <netinet/in.h>
#include <exception>
#include <iostream>

WebServer::WebServer():WebServer(INADDR_LOOPBACK, 8080){}

WebServer::WebServer(int ip, uint16_t port){
    this->ip = ip;
    this->port = port;
    struct sockaddr_in addr = {
        .sin_family =   AF_INET,
        .sin_port   =   htons(this->port),
        .sin_addr   =   htonl(this->ip)
    };
    try
    {
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (bind(this->sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
            throw std::runtime_error("Failed binding socket");
        if (listen(sock, MAX_CONNECTIONS) < 0)
            throw std::runtime_error("Failed listening to socket");
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
}

WebServer::WebServer(const WebServer& other){
    this->ip = other.ip;
    this->port = other.port;
    this->sock = other.sock;
}

WebServer& WebServer::operator=(const WebServer& other){}

WebServer::serve()
{
    accept(sock, NULL, NULL)
}

bool WebServer::get_open(void)
{
    
}

WebServer::~WebServer()
{
    close(this->sock);
}
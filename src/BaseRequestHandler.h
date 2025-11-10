#pragma once

#include <string>
#include <map>
#include <WebServer.h>
#include <sstream>
#include <ctime>

enum    HTTPRequestMethod
{
    GET = 0,
    HEAD,       // Like GET without body
    POST,       // Submit something
    PUT,        // Replace all current representations of the target resource
    DELETE,     // Delete the target resource
    CONNECT,    // Connects. DO NOT SUPPORT!
    OPTIONS,    // Return allowed methods on requested ressource
    TRACE,      // Return 200 OK with 
    PATCH       // PATCH is diffenent than Put in that it replaces only partial
};

enum HTTPVersion
{
    zero_nine = 0,
    one_zero,
    one_one,
    two_zero
};

struct  Request{
    std::map<std::string, std::string>  headers;
    std::string                         path;
    int                                 ip;
    HTTPRequestMethod                   method;
    HTTPVersion                         version;
    std::streambuf                      rfile;
    std::streambuf                      wfile;
};

struct Response{
    std::map<std::string, std::string>  headers;
    std::string                         body;
    int                                 status;
    HTTPVersion                         version;
};

class BaseRequestHandler
{
public:
    BaseRequestHandler(Request req);
    BaseRequestHandler(const BaseRequestHandler &other);
    BaseRequestHandler& operator=(const BaseRequestHandler &other);
    ~BaseRequestHandler(void);
    void        closeConnection(void);
    void        handle(void);
    void        handle_one_request(void);
    int         send_error(int code);
    int         send_error(int code, std::string message);
    int         send_error(int code, std::string message, std::string explain);
    int         send_response(int code);
    int         send_response(int code, std::string message);
    int         send_header(std::pair<std::string, std::string> header_pair);
    int         send_response_only(int code, std::string message);
    int         end_headers(void);
    int         flush_headers(void);
    void        log_request(int code);
    void        log_request(int code, size_t size);
    void        log_error(void);
    void        log_message(std::string format, ...);
    std::string date_time_string(void);
    std::string date_time_string(std::time_t);
    virtual int do_GET();
    virtual int do_HEAD();
    virtual int do_POST();
    virtual int do_PUT();
    virtual int do_DELETE();
    virtual int do_CONNECT();
    virtual int do_OPTIONS();
    virtual int do_TRACE();
    virtual int do_PATCH();
private:
    BaseRequestHandler(void);
    Request     request;
    Response    response;
    WebServer   *server;
    int         clientAdress;
    std::iostream   rfile;
    std::iostream   wfile;
};

struct Address{
    std::string ip;
    int port;
};

class BaseRequestHandler
{
protected:
    Address _client;
    Address _server;
    std::istream _request;
    std::ostream _response;
    std::string server_version;
    std::string protocol_version;
    void send_response(int code, std::string message);
    void do_HEAD();
    void do_GET();
    void do_POST();
    void do_PUT();
    void do_DELETE();
    void do_CONNECT();
    void do_OPTIONS();
    void do_TRACE();
    void do_PATCH();
    void handle_request();
public:
    BaseRequestHandler();
    BaseRequestHandler(const BaseRequestHandler& other);
    BaseRequestHandler& operator=(const BaseRequestHandler& other);
    ~BaseRequestHandler();
};

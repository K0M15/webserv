
struct BaseServerConfig{
    std::string address;
    std::string host;
    int port;
}

template<typename RequestHandler>
class BaseServer{
private:
    BaseServer(){}
public:
    BaseServer(struct BaseServerConfig* config);
    BaseServer(const BaseServer& other);
    BaseServer& operator=(const BaseServer& other);
    ~BaseServer();
    virtual int fileno() const = 0;
    virtual void onRead() = 0;
    virtual void onWrite() = 0;
    virtual void onError() = 0;
    serve_forever();
}
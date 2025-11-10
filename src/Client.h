class Client{
private:
    int sock;
    void parseHTTP();
    void read();
public:
    Client(int fd);
    Client(const Client& other);
    Client& operator=(const Client& other);
    ~Client();
}
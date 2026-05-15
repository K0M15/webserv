#include <ctime>
#include <istream>

class Client{
private:
    int port;
    int fd;
    bool open;
    std::time_t created;
    std::time_t last_connection;
    int ip[4];
    Client(): port(0), open(false), created(0), last_connection(0) {}
public:
    Client(int port) : port(port), open(true), created(0), last_connection(0) {
        std::time(&created);
        last_connection = created;
    }
    Client(Client& other);
    Client& operator=(Client &other);
    ~Client(); // destructor should close connection
    std::istream& operator>>(std::streambuf& buffer);
    void write(const char * data);
    void close();
    int getPort() { return port; }
    int getFD() { return fd; }
    bool checkTimeout(std::time_t now) {
        if (now - last_connection >= 5000 )
            close();
    }
};

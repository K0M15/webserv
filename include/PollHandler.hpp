#pragma once
#include <functional>
#include <vector>
#include <poll.h>
#include <HttpServerException.hpp>

typedef std::function<void()> fvoid_t;

class PollHandler {
public:
    struct EventType {
        int fd;
        short event;
        fvoid_t on_readable;
        fvoid_t on_writeable;
        fvoid_t on_close; 
    };

    static PollHandler& getInstance();

private:
    unsigned int timeout;
    std::vector<struct EventType> events;

public:
    PollHandler();
    explicit PollHandler(int t);
    ~PollHandler();

public:
    PollHandler(const PollHandler& other);
    PollHandler& operator=(const PollHandler& other);

    unsigned int getTimeout() const;
    void setTimeout(unsigned int t);

    struct EventType* getEventByFD(int fd);
    void subscribe_read(int fd, fvoid_t on_close, fvoid_t on_readable);
    void subscribe_write(int fd, fvoid_t on_close, fvoid_t on_writeable);
    void subscribe(int fd, fvoid_t on_close, fvoid_t on_readable, fvoid_t on_writeable);
    void unsubscribe(int fd);
    void checkFDs();
};

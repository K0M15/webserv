#include <functional>
#include <map>
#include <vector>
#include <algorithm>
#include <poll.h>
#include <HttpServerException.hpp>

typedef std::function<void()> fvoid_t;

class PollHandler {
private:
    int timeout;
    struct EventType{
        int fd;
        short event;
        fvoid_t on_readable;
        fvoid_t on_writeable;
        fvoid_t on_close; // triggers on POLLERR, POLLRDHUP or POLLNVAL
    };
    std::vector<struct EventType> events;
public:
    PollHandler() : timeout(3000), events() {}
    PollHandler(int t) : timeout(t), events(){}
    PollHandler(PollHandler& other): timeout(other.timeout), events(other.events){}
    PollHandler& operator=(PollHandler& other){
        if (&other != this)
        {
            timeout = other.timeout;
            events = other.events;
        }
        return *this;
    }
    ~PollHandler() = default;

    struct EventType* getEventByFD(int fd){
        for(auto& e: events)
        {
            if (e.fd == fd){
                return &e;
            }
        }
        return nullptr;
    }
    void subscribe_read(int fd, fvoid_t on_close, fvoid_t on_readable){
        subscribe(fd, on_close, on_readable, nullptr);
    }
    void subscribe_write(int fd, fvoid_t on_close, fvoid_t on_writeable){
        subscribe(fd, on_close, nullptr, on_writeable);
    }
    void subscribe(int fd, fvoid_t on_close, fvoid_t on_readable, fvoid_t on_writeable){
        EventType d;
        d.event = ( on_close == nullptr ? 0 : POLLERR | on_readable == nullptr ? 0 : POLLIN | on_writeable == nullptr ? 0 : POLLOUT );
        if (d.event == 0) throw HttpServerException("No eventlistener is provided");
        d.fd = fd;
        d.on_close = on_close;
        d.on_readable = on_readable;
        d.on_writeable = on_writeable;
        auto e = getEventByFD(fd);
        if (e != nullptr){
            e->event = d.event;
            e->fd = d.fd;
            e->on_close = d.on_close;
            e->on_readable = d.on_readable;
            e->on_writeable = d.on_writeable;
            return;
        }
        events.push_back(d);
    }

    void unsubscribe(int fd) {
        events.erase(
            std::remove_if(events.begin(), events.end(), [fd](const struct EventType& e) {
                return e.fd == fd;
            }), 
            events.end()
        );
    };

    void checkFDs() {
        if (events.empty()) {
            return;
        }
        std::vector<struct pollfd> pollfds;
        pollfds.reserve(events.size());

        for (const auto& e : events) {
            struct pollfd pfd;
            pfd.fd = e.fd;
            pfd.events = e.event;
            pfd.revents = 0;
            pollfds.push_back(pfd);
        }
        int result = poll(pollfds.data(), pollfds.size(), timeout);
        if (result < 0) {
            throw HttpServerException("Error on poll");
        }
        
        if (result == 0) {
            return;
        }

        for (size_t i = 0; i < pollfds.size(); ++i) {
            short revents = pollfds[i].revents;
            if (revents == 0) continue;
            if (revents & (POLLERR | POLLHUP | POLLNVAL)) {
                if (events[i].on_close) events[i].on_close(); // what if element is removed?
                continue;
            }
            if (revents & (POLLIN | POLLPRI))
                if (events[i].on_readable) events[i].on_readable(); // what if element is removed?
            if (revents & POLLOUT)
                if (events[i].on_writeable) events[i].on_writeable();
        }
    }
};

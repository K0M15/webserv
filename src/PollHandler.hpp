#include <functional>
#include <map>
#include <vector>
#include <poll.h>
#include <HttpServerException.hpp>

class PollHandler {
private:
    struct EventType{
        int fd;
        short event;
        std::function<void()> cb;
    };
    std::vector<struct EventType> events;
    int timeout;
public:
    PollHandler() : timeout(300), events() {}
    PollHandler(int t) : timeout(t), events(){}
    PollHandler(PollHandler& other);
    PollHandler& operator=(PollHandler& other);
    ~PollHandler();

    void subscribe(int fd,  std::function<void()> handler);
    void unsubscribe(int fd);

    void checkFDs() {
        struct pollfd* check = (struct pollfd*)calloc(events.size(), sizeof(struct pollfd));
        int i = 0;
        for (std::vector<struct EventType>::iterator e = events.begin(); e != events.end(); e++)
        {
            check[i].events = e->event;
            check[i].fd = e->fd;
            i++;
        }
        int result = poll(check, i, timeout);
        if (result < 0)
            throw HttpServerException("Error on poll");
        // check each result, if revents has been set, then call cbs if appropriate event is active
    }
};

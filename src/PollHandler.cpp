#include "PollHandler.hpp"
#include <algorithm>

PollHandler& PollHandler::getInstance()
{
    static PollHandler instance;
    return instance;
}

PollHandler::PollHandler() : timeout(3000), events() {}

PollHandler::PollHandler(int t) : timeout(t), events() {}

PollHandler::PollHandler(const PollHandler& other) : timeout(other.timeout), events(other.events) {}

PollHandler& PollHandler::operator=(const PollHandler& other) {
    if (&other != this) {
        timeout = other.timeout;
        events = other.events;
    }
    return *this;
}

PollHandler::~PollHandler() {}

void PollHandler::setTimeout(unsigned int t) {
    timeout = t;
}

unsigned int PollHandler::getTimeout() const {
    return timeout;
}

PollHandler::EventType* PollHandler::getEventByFD(int fd) {
    for (auto& e : events) {
        if (e.fd == fd) {
            return &e;
        }
    }
    return nullptr;
}

void PollHandler::subscribe_read(int fd, fvoid_t on_close, fvoid_t on_readable) {
    subscribe(fd, on_close, on_readable, nullptr);
}

void PollHandler::subscribe_write(int fd, fvoid_t on_close, fvoid_t on_writeable) {
    subscribe(fd, on_close, nullptr, on_writeable);
}

void PollHandler::subscribe(int fd, fvoid_t on_close, fvoid_t on_readable, fvoid_t on_writeable) {
    auto e = getEventByFD(fd);
    if (e != nullptr) {
        if (on_close != nullptr) e->on_close = on_close;
        if (on_readable != nullptr) e->on_readable = on_readable;
        if (on_writeable != nullptr) e->on_writeable = on_writeable;

        e->event = 0;
        if (e->on_close)    e->event |= (POLLERR | POLLHUP | POLLNVAL);
        if (e->on_readable) e->event |= (POLLIN | POLLPRI);
        if (e->on_writeable) e->event |= POLLOUT;
        return;
    }
    short ev = 0;
    if (on_close != nullptr) ev |= (POLLERR | POLLHUP | POLLNVAL);
    if (on_readable != nullptr) ev |= (POLLIN | POLLPRI);
    if (on_writeable != nullptr) ev |= POLLOUT;
    if (ev == 0) throw HttpServerException("No eventlistener is provided");
    EventType d;
    d.fd = fd;
    d.event = ev;
    d.on_close = on_close;
    d.on_readable = on_readable;
    d.on_writeable = on_writeable;
    events.push_back(d);
}

void PollHandler::unsubscribe(int fd) {
    events.erase(
        std::remove_if(events.begin(), events.end(), [fd](const struct EventType& e) {
            return e.fd == fd;
        }),
        events.end()
    );
}

void PollHandler::checkFDs() {
    if (events.empty()) return;
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
    if (result < 0) throw HttpServerException("Error on poll");
    if (result == 0) return;
    for (size_t i = 0; i < pollfds.size(); ++i) {
        short revents = pollfds[i].revents;
        if (revents == 0) continue;
        int current_fd = pollfds[i].fd;
        auto* e = getEventByFD(current_fd);
        if (!e) continue;

        if (revents & (POLLERR | POLLHUP | POLLNVAL)) {
            if (e->on_close) {
                auto close_cb = e->on_close;
                close_cb();
            }
            continue;
        }
        if (revents & (POLLIN | POLLPRI)) {
            e = getEventByFD(current_fd);
            if (e && e->on_readable) e->on_readable();
        }
        if (revents & POLLOUT) {
            e = getEventByFD(current_fd);
            if (e && e->on_writeable) e->on_writeable();
        }
    }
}

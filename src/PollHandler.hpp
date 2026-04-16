


class PollHandler{
    // Singleton for poll handling.
private:
    struct PollHandlerData{
        int fd;
        short events;
        struct PollHandlerCallback* callbacks;
    };
    std::vector<PollHandlerData> _fds;
public:
    struct PollHandlerCallback{
        void *callback_read(int fd, short revents);
        void *callback_write(int fd, short revents);
        void *callback_error(int fd, short revents);
    };
    PollHandler()
    PollHandler(const PollHandler& other);
    PollHandler& operator=(const PollHandler& other);
    ~PollHandler()
    
    void addFD(
        int fd,
        short events,
        struct PollHandlerCallback* callbacks
    );
    void removeFD(int fd);
    void updateFD(int fd, short events, struct PollHandlerCallback* callbacks);
};
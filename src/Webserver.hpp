#include "PollHandler.hpp"

class Webserver
{
private:
    Webserver();
    int target_port;
    int timeout;
public:
    Webserver(const WebserverSettings& settings);
    ~Webserver();
    Webserver(const Webserver& other);
    Webserver& operator=(const Webserver& other);
    void listen(); // main loop
    PollHandler& getPollHandler() { return PollHandler::getInstance(); }
};

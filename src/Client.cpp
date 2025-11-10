#include "Client.h"
#include <sys/socket.h>

Client::Client(int fd)
{
    this->sock = fd;
}


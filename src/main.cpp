#include <sys/socket.h>
#include <arpa/inet.h>
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <string.h>

int main()
{
    int serverFD = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8088);

    if (bind(serverFD, (const struct sockaddr *)&address, sizeof(address)))
    {
        std::cerr << "Error binding on " << ntohs(address.sin_port) << ".\nError message: " << strerror(errno) <<"\n Stopping." << std::endl;
        return 1;

    }
    if (listen(serverFD, 10))
    {
        std::cerr << "Error listening on " << ntohs(address.sin_port) << ".\nError message: " << strerror(errno) <<"\n Stopping." << std::endl;
        close(serverFD);
        return 1;
    }

    socklen_t addrlen = sizeof(address);

    while (true)
    {
        int client_fd = accept(serverFD, (struct sockaddr *)&address, &addrlen);
        if (client_fd == -1)
            std::cerr << "Error accepting!" << std::endl;
        else
        {
            std::cout << "Accepted:\n";
            std::cout << "IP:'\t\t" << inet_ntoa(address.sin_addr) << "\n";
            std::cout << "Port:'\t\t" << address.sin_port << "\n";
            close (client_fd);
        }
        sleep(1);
    }
}
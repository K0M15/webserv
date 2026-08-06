#include "Webserver.hpp"
#include <iostream>
#include <algorithm>

/**
 * Included Additional parsing could be done to ensure the file contents are also valid.
 * However, this would be a bit overkill and is not required by the subject.
 * @author dabierma 
 */
static void doesFileHaveCorrectExtension(char **argv)
{
    std::string argument = argv[1];

    if (argument.size() < 5 || argument.substr(argument.size() - 5) != ".conf")
        throw std::runtime_error("Argument must be a valid .conf file");
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: ./webserv <config_file>" << std::endl;
        return 1;
    }
    try
    {
        doesFileHaveCorrectExtension(argv);
        Webserver server(argv[1]);
        server.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

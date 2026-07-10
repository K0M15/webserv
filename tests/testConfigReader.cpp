#include "ConfigReader.h"
#include "WebserverSettings.hpp"
#include "iostream"

int main(int argc, char** argv)
{
    if (argc != 2)
        return 1;
    std::string path = argv[1];
    std::cout << "Path: " << path << "\n";
    try
    {
        ConfigReader reader(path);
        std::cout << reader << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
}
#include "Webserver.hpp"
#include <iostream>
#include <algorithm>
#include "InMemoryDB.hpp"

/**
 * Included Additional parsing could be done to ensure the file contents are also valid.
 * However, this would be a bit overkill and is not required by the subject.
 * @author dabierma 
 */
static void doesFileHaveCorrectExtension(const char *argv)
{
    std::string argument = argv;

    if (argument.size() < 5 || argument.substr(argument.size() - 5) != ".conf")
        throw std::runtime_error("Argument must be a valid .conf file");
}

static void loginTempDebugTest()
{
    InMemoryDB<std::string, std::string> testDb;
    testDb.set("user:101", "Alice");
    if (auto val = testDb.get("user:101"); val)
    {
    std::cout << "InMemoryDB test: found " << *val << std::endl;
    }
    testDb.del("user:101");
    std::cout << "InMemoryDB test: exists after delete = " << testDb.exists("user:101") << std::endl;
}

int main(int argc, char **argv)
{
    std::string filepath = "config.conf";
    if (argc > 1){
        filepath = argv[1];
    }
    else{
        std::cout << "Using standard file name " << filepath << std::endl;
    }
    if (argc > 2){
        std::cerr << "Wrong amount of arguments given.";
        std::cout << "Usage: ./webserv [config_file]\n";
        std::cout << "When no 'config_file' argument is given, the file 'config.conf' is searched" << std::endl;
    }
#ifdef DEBUG
    loginTempDebugTest();
#else
    (void) loginTempDebugTest;
#endif
    try
    {
        doesFileHaveCorrectExtension(filepath.c_str());
        Webserver server(filepath);
        server.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

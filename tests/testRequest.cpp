#include "../src/Request.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

int main(int argc, char* argv[])
{
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <request_file>" << std::endl;
        return 1;
    }

    std::ifstream file(argv[1]);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << argv[1] << std::endl;
        return 1;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string rawRequest = buffer.str();

    try {
        Request req = Request::fromString(rawRequest);

        std::cout << "--- Request Validation ---" << std::endl;
        std::cout << "Method:  " << req.getMethod() << std::endl;
        std::cout << "URL:     " << req.getURL().str() << std::endl;
        std::cout << "Version: " << req.getVersion() << std::endl;
        
        std::cout << "Headers:" << std::endl;
        const std::map<std::string, std::string>& headers = req.getHeaders();
        for (std::map<std::string, std::string>::const_iterator it = headers.begin(); it != headers.end(); ++it) {
            std::cout << "  " << it->first << ": " << it->second << std::endl;
        }

        std::cout << "Body:" << std::endl;
        std::cout << req.getBody() << std::endl;
        std::cout << "--------------------------" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error parsing request: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

#include "Request.hpp"

Request& Request::operator=(const Request& other) {
    if (this != &other) {
        method = other.method;
        url = other.url;
        version = other.version;
        headers = other.headers;
        body = other.body;
    }
    return *this;
}

Request Request::fromString(const std::string& rawRequest)
{
    Request req;
    std::istringstream stream(rawRequest);
    std::string line;

    //parse request line
    if (std::getline(stream, line) && !line.empty())
    {
        if (line.back() == '\r')
            line.pop_back();
        std::stringstream ss(line);
        ss >> req.method >> req.url >> req.version;
    }
    //parse headers, last line is empty
    while (std::getline(stream, line) && line != "\r" && !line.empty())
    {
        if (line.back() == '\r')
            line.pop_back();
        auto pos = line.find(": ");
        if (pos != std::string::npos)
            req.headers[line.substr(0, pos)] = line.substr(pos + 2);
    }
    // check content length, then attach body
    if (req.headers.count("Content-Length"))
    {
        size_t len = std::stoul(req.headers["Content-Length"]);
        req.body.resize(len);
        stream.read(&req.body[0], len);
    }
    // Return built object
    return req;
}
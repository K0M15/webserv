
#include "Request.hpp"
#include <cstddef>

Request& Request::operator=(const Request& other) {
    if (this != &other) {
        method = other.method;
        url = other.url;
        version = other.version;
        headers = other.headers;
        query = other.query;
        body = other.body;
    }
    return *this;
}

Request Request::fromString(const std::string& rawRequest, size_t max_body_size)
{
    Request req;
    std::istringstream stream(rawRequest);
    std::string line;

    //parse request line
    if (!std::getline(stream, line))
        throw std::runtime_error("Empty request");
    if (!line.empty() && line.back() == '\r')   // back() auf leerem String = UB
        line.pop_back();
    if (line.empty())
        throw std::runtime_error("Empty request line");

    {
        std::stringstream ss(line);
        if (!(ss >> req.method >> req.url >> req.version))
            throw std::runtime_error("Malformed request line");

        std::string extra;
        if (ss >> extra)
            throw std::runtime_error("Too many tokens in request line");
    }
    // if (std::getline(stream, line) && !line.empty())
    // {
    //     if (line.back() == '\r')
    //         line.pop_back();
    //     std::stringstream ss(line);
    //     if (!(ss >> req.method >> req.url >> req.version))
    //         throw std::runtime_error("Malformed request line");
    // }
    //parse headers, last line is empty
    while (std::getline(stream, line) && line != "\r" && !line.empty())
    {
        if (line.back() == '\r')
            line.pop_back();
        auto pos = line.find(":");
        if (pos != std::string::npos)
        {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            // RFC Requirement: A server MUST reject any HTTP/1.1 request that contains whitespace before the colon with a 400 Bad Request status code.
            if (std::any_of(key.begin(), key.end(), [](unsigned char c){ return std::isspace(c); }))
                throw std::runtime_error("Header key is not allowed to contain space");
            while (!value.empty() && value.front() == ' ')
                    value.erase(0, 1);
            std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c)
                {
                    return std::tolower(c);
                });
            req.headers[key] = value;
        }
    }
    // check content length, then attach body
     if (auto it = req.headers.find("content-length"); it != req.headers.end())
    {
        const std::string& value = it->second;
        if (value.empty())
            throw std::runtime_error("Invalid Content-Length");
        unsigned long len{};
        auto result = std::from_chars(value.data(), value.data() + value.size(), len);
        if (result.ec != std::errc() || result.ptr != value.data() + value.size())
            throw std::runtime_error("Invalid Content-Length");
        if (len > max_body_size)
            throw PayloadTooLargeError("Content-Length exceeds max_body_size");

        req.body.resize(len);
        stream.read(req.body.data(), len);
        //Maybe 
        if (stream.gcount() != static_cast<std::streamsize>(len))
            throw std::runtime_error("Incomplete request body");
    }
    // Return built object
    return req;
}
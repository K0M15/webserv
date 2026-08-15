
#include "Request.hpp"
#include "Chunked.hpp"
#include "sys/socket.h"
#include <cctype>
#include <limits>
#include <sstream>

Request& Request::operator=(const Request& other) {
    if (this != &other) {
        method = other.method;
        url = other.url;
        version = other.version;
        headers = other.headers;
        query = other.query;
        body = other.body;
        chunked = other.chunked;
    }
    return *this;
}

static bool iequals(const std::string& a, const std::string& b)
{
    if (a.size() != b.size())
        return false;
    for (size_t i = 0; i < a.size(); ++i)
    {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i])))
            return false;
    }
    return true;
}

static bool isChunkedCoding(const std::string& value)
{
    size_t start = 0;
    while (true)
    {
        size_t comma = value.find(',', start);
        std::string coding = (comma == std::string::npos)
            ? value.substr(start) : value.substr(start, comma - start);
        size_t b = coding.find_first_not_of(" \t");
        size_t e = coding.find_last_not_of(" \t");
        if (b == std::string::npos)
            return false;
        if (!iequals(coding.substr(b, e - b + 1), "chunked"))
            return false;
        if (comma == std::string::npos)
            break;
        start = comma + 1;
    }
    return true;
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
        if (!(ss >> req.method >> req.url >> req.version))
            throw std::runtime_error("Malformed request line");
    }
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
    // Body framing: either Content-Length or Transfer-Encoding (chunked).
    // A request carrying both is treated as a smuggling attempt (RFC 9112 6.1).
    auto te_it = req.headers.find("transfer-encoding");
    auto cl_it = req.headers.find("content-length");
    if (te_it != req.headers.end())
    {
        if (cl_it != req.headers.end())
            throw std::runtime_error("Content-Length and Transfer-Encoding are mutually exclusive");
        if (!isChunkedCoding(te_it->second))
            throw std::runtime_error("Unsupported Transfer-Encoding");

        std::ostringstream oss;
        oss << stream.rdbuf();
        std::string framed = oss.str();

        std::string decoded;
        size_t consumed = 0;
        if (ChunkedBody::decode(framed, std::numeric_limits<size_t>::max(),
                                decoded, consumed) != ChunkResult::COMPLETE)
            throw std::runtime_error("Malformed chunked body");
        req.body = decoded;
        req.chunked = true;
    }
    else if (cl_it != req.headers.end())
    {
        const std::string& value = cl_it->second;
        unsigned long len{};
        auto result = std::from_chars(value.data(), value.data() + value.size(), len);
        if (result.ec != std::errc() || result.ptr != value.data() + value.size())
            throw std::runtime_error("Invalid Content-Length");

        req.body.resize(len);
        stream.read(req.body.data(), len);
        if (stream.gcount() != static_cast<std::streamsize>(len))
            throw std::runtime_error("Incomplete request body");
    }
    // Return built object
    return req;
}

#include "Request.hpp"
#include "Chunked.hpp"
#include "Defines.hpp"
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
        if (req.version != HTTP_VERSION)
            throw HTTPVersionNotSupportedException(std::string(HTTP_VERSION) + " required");
        // if (req.method != "GET" && req.method != "POST" 
        //     && req.method != "DELETE" && req.method != "PUT"
        //     && req.method != "PATCH" && req.method != )
        //     throw HTTPMethodNotAllowedException("Method not supported");
    }
    //parse headers, last line is empty
    while (std::getline(stream, line) && line != "\r" && !line.empty())
    {
        if (line.back() == '\r')
            line.pop_back();
        auto position = line.find(":");
        if (position != std::string::npos)
        {
            std::string key = line.substr(0, position);
            std::string value = line.substr(position + 1);
            // RFC Requirement: A server MUST reject any HTTP/1.1 request that contains whitespace before the colon with a 400 Bad Request status code.
            if (std::any_of(key.begin(), key.end(), [](unsigned char c){ return std::isspace(c); }))
                throw std::runtime_error("Header key is not allowed to contain space");
            // Remove leading whitespace from value
            while (!value.empty() && (value.front() == ' ' || value.front() == '\t'))
                value.erase(0, 1);
            // lower case keys
            std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c)
                {
                    return std::tolower(c);
                });
            // reject duplicate headers
            if (!req.headers[key].empty())
                throw std::runtime_error("Duplicate header " + key);
            // add to header map
            req.headers[key] = value;
        }
        else throw std::runtime_error("Malformed header line");
    }
    // validate host header
    {
        auto header_host = req.headers.find("host");
        if (header_host == req.headers.end() || header_host->second.empty())
            throw std::runtime_error("Host header is required");
    }
    // Body framing: either Content-Length or Transfer-Encoding (chunked).
    // A request carrying both is treated as a smuggling attempt (RFC 9112 6.1)
    {
        auto header_transfer_encoding = req.headers.find("transfer-encoding");
        auto header_content_length = req.headers.find("content-length");
        if (header_transfer_encoding != req.headers.end())
        {
            if (header_content_length != req.headers.end())
                throw std::runtime_error("Content-Length and Transfer-Encoding are mutually exclusive");
            if (!isChunkedCoding(header_transfer_encoding->second))
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
        else if (header_content_length != req.headers.end())
        {
            const std::string& value = header_content_length->second;
            unsigned long len{};
            auto result = std::from_chars(value.data(), value.data() + value.size(), len);
            if (result.ec != std::errc() || result.ptr != value.data() + value.size())
                throw std::runtime_error("Invalid Content-Length");

            req.body.resize(len);
            stream.read(req.body.data(), len);
            if (stream.gcount() != static_cast<std::streamsize>(len))
                throw std::runtime_error("Incomplete request body");
        }
        return req;
    }
}

const std::string& Request::getHeader(const std::string& key) const {
    // fromString() stores header keys lowercased, so lookups must
    // lowercase the key too, or e.g. getHeader("Content-Type") never matches.
    std::string lower_key = key;
    std::transform(lower_key.begin(), lower_key.end(), lower_key.begin(), [](unsigned char c)
        {
            return std::tolower(c);
        });
    std::map<std::string, std::string>::const_iterator it = headers.find(lower_key);
    if (it != headers.end()) {
        return it->second;
    }
    static const std::string empty;
    return empty;
}


/**
 * Go through cookie header and split at each delimiter ";"
 * HTTP specs basically require cookies to be joined with a semicolon Delimiter since browsers
 * will literally join multiple cookies with it. e.g. cookie: session_id=8675309; username=admin; etc
 * So we take each of these chunks and tokenize key pairs between the session_id, then the values
 * and return the pairs until we reach the end of the cookie.
 * For now if nothing is found im just passing an empty string.
 */
std::string Request::getCookie(const std::string& name) const
{
    std::string cookieHeader = getHeader("Cookie");
    size_t position = 0;

    while (position < cookieHeader.size())
    {
        size_t semicolonDelimiter = cookieHeader.find(';', position);

        std::string pair;
        if (semicolonDelimiter == std::string::npos)
            pair = cookieHeader.substr(position);
        else
            pair = cookieHeader.substr(position, semicolonDelimiter - position);

        size_t delimiterPosition = pair.find('=');
        if (delimiterPosition != std::string::npos)
        {
            std::string key = pair.substr(0, delimiterPosition);
            size_t start = key.find_first_not_of(" \t");
            if (start != std::string::npos)
                key = key.substr(start);
            if (key == name)
            {
                std::string value = pair.substr(delimiterPosition + 1);
                size_t end = value.find_last_not_of(" \t");
                if (end != std::string::npos)
                    value = value.substr(0, end + 1);
                return value;
            }
        }

        if (semicolonDelimiter == std::string::npos)
            break;
        position = semicolonDelimiter + 1;
    }

    return "";
}
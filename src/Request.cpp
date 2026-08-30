#include "Request.hpp"
#include "Connection.hpp"
#include "Chunked.hpp"
#include "Defines.hpp"
#include "PollHandler.hpp"
#include <sys/socket.h>
#include <unistd.h>
#include <cctype>
#include <limits>
#include <sstream>
#include <charconv>
#include <algorithm>
#include <cerrno>

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

    // parse request line
    if (std::getline(stream, line) && !line.empty())
    {
        if (line.back() == '\r')
            line.pop_back();
        std::stringstream ss(line);
        if (!(ss >> req.method >> req.url >> req.version))
            throw std::runtime_error("Malformed request line");
        if (req.version != HTTP_VERSION)
            throw HTTPVersionNotSupportedException(std::string(HTTP_VERSION) + " required");
    }
    // parse headers, last line is empty
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

static std::string headerFieldValue(const std::string &lower_haystack, const std::string &name)
{
    std::string needle = "\r\n" + name + ":";
    size_t pos = lower_haystack.find(needle);
    if (pos == std::string::npos)
        return "";
    pos += needle.size();
    size_t end = lower_haystack.find("\r\n", pos);
    if (end == std::string::npos)
        end = lower_haystack.size();
    std::string value = lower_haystack.substr(pos, end - pos);
    size_t b = value.find_first_not_of(" \t");
    size_t e = value.find_last_not_of(" \t");
    if (b == std::string::npos)
        return "";
    return value.substr(b, e - b + 1);
}

RequestReadState Request::isRequestComplete(Connection &conn)
{
    if (!conn.headers_complete)
    {
        size_t header_end = conn.read_buffer.find("\r\n\r\n");
        if (header_end == std::string::npos)
            return RequestReadState::INCOMPLETE;

        conn.headers_complete = true;
        conn.header_end = header_end;

        std::string header_part = conn.read_buffer.substr(0, header_end);
        std::transform(header_part.begin(), header_part.end(), header_part.begin(),
                       [](unsigned char c)
                       { return std::tolower(c); });

        std::string expect_value = headerFieldValue(header_part, "expect");
        std::string cl_value = headerFieldValue(header_part, "content-length");
        std::string te_value = headerFieldValue(header_part, "transfer-encoding");
        
        if (!te_value.empty() && !cl_value.empty())
            return RequestReadState::BAD_REQUEST;

        if (!te_value.empty())
        {
            if (!iequals(te_value, "chunked"))
                return RequestReadState::NOT_IMPLEMENTED;
            conn.chunked = true;
            conn.content_length = 0;
        }
        else
        {
            conn.chunked = false;
            if (!cl_value.empty())
            {
                unsigned long len{};
                auto r = std::from_chars(cl_value.data(), cl_value.data() + cl_value.size(), len);
                if (r.ec != std::errc() || r.ptr != cl_value.data() + cl_value.size())
                    return RequestReadState::BAD_REQUEST; // malformed content-length
                conn.content_length = static_cast<size_t>(len);
            }
            else
                conn.content_length = 0;
        }
        if (!expect_value.empty())
        {
            if (expect_value != "100-continue") // https://www.rfc-editor.org/info/rfc9110/#field.expect
                return RequestReadState::EXPECTATION_FAILED;
            size_t max_body_size = conn.settings ? conn.settings->max_body_size : DEFAULT_MAX_BODY_SIZE;
            if (conn.content_length > max_body_size)
            {
                return RequestReadState::PAYLOAD_TOO_LARGE;
            }
            // check if readbuffer is already big enough to contain data:
            if (conn.read_buffer.size() < conn.header_end + 4 + conn.content_length)
            {
                if (conn.fd >= 0 && !conn.sent_100_continue)
                {
                    // msg is big enough, so no msg is needed
                    conn.sent_100_continue = true;
                    auto prev = PollHandler::getInstance().getEventByFD(conn.fd);
                    fvoid_t on_read = prev ? prev->on_readable : nullptr;
                    fvoid_t on_close = prev ? prev->on_close : nullptr;

                    // construct temporary struct for switching values.
                    struct ContinueWriter {
                        std::string msg = "HTTP/1.1 100 Continue\r\n\r\n";
                        size_t bytes_sent = 0;
                        fvoid_t on_readable;    // old handlers, needed for resubbing
                        fvoid_t on_close;       // old handlers, needed for resubbing
                    };
                    auto state = std::make_shared<ContinueWriter>(); // expand lifetime
                    state->on_readable = on_read;
                    state->on_close = on_close;

                    PollHandler::getInstance().subscribe(conn.fd,
                        on_close ? on_close : fvoid_t([fd = conn.fd](){
                            ::close(fd);
                            PollHandler::getInstance().unsubscribe(fd);
                        }),
                        nullptr, // remove read callback
                        [fd = conn.fd, state, conn](){
                            if (state->bytes_sent < state->msg.size()) // check if message is sent
                            {
                                ssize_t n = ::write(fd, state->msg.data() + state->bytes_sent, state->msg.size() - state->bytes_sent);
                                if (n > 0)
                                    state->bytes_sent += static_cast<size_t>(n);
                                else if (n < 0) // Error Handling, close on error
                                {
                                    std::cerr << "Error writing 100-continue message" << std::endl;
                                    if (state->on_close)
                                        state->on_close();
                                    else
                                    {
                                        ::close(fd);
                                        PollHandler::getInstance().unsubscribe(fd);
                                    }
                                    return;
                                }
                            }
                            if (state->bytes_sent >= state->msg.size())
                            {
                                // if message is sent, re-register cb
                                if (state->on_readable)
                                    PollHandler::getInstance().subscribe_read(fd, state->on_close, state->on_readable);
                            }
                        }
                    );
                }
                return RequestReadState::INCOMPLETE;
            }
        }
    }

    size_t body_start = conn.header_end + 4;

    if (conn.chunked)
    {
        size_t max = conn.settings ? conn.settings->max_body_size : DEFAULT_MAX_BODY_SIZE;
        std::string framed = conn.read_buffer.substr(body_start);
        std::string decoded;
        size_t consumed = 0;
        switch (ChunkedBody::decode(framed, max, decoded, consumed))
        {
        case ChunkResult::INCOMPLETE:
            return RequestReadState::INCOMPLETE;
        case ChunkResult::MALFORMED:
            return RequestReadState::BAD_REQUEST;
        case ChunkResult::TOO_LARGE:
            return RequestReadState::PAYLOAD_TOO_LARGE;
        case ChunkResult::COMPLETE:
            if (conn.read_buffer.size() >= body_start + consumed)
            {
                conn.raw_body_length = consumed;
                return RequestReadState::COMPLETE;
            }
            return RequestReadState::INCOMPLETE;
        }
    }

    size_t max = conn.settings ? conn.settings->max_body_size : DEFAULT_MAX_BODY_SIZE;
    if (conn.content_length > max)
        return RequestReadState::PAYLOAD_TOO_LARGE;
    if (conn.read_buffer.size() >= body_start + conn.content_length)
    {
        conn.raw_body_length = conn.content_length;
        return RequestReadState::COMPLETE;
    }
    return RequestReadState::INCOMPLETE;
}
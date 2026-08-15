#pragma once

#include <cstddef>
#include <string>

enum class ChunkResult {
    COMPLETE,
    INCOMPLETE,
    MALFORMED,
    TOO_LARGE
};

class ChunkedBody
{
public:
    // raw:      bytes of the chunked body, starting right after the header
    //           terminator "\r\n\r\n" (trailers/pipelined data may follow).
    // max_size: maximum allowed decoded body size.
    // decoded:  receives the de-chunked body when the result is COMPLETE.
    // consumed: receives the number of raw bytes occupied by the chunked
    //           framing (chunk lines + trailers + final CRLF).
    static ChunkResult decode(const std::string& raw, size_t max_size,
                              std::string& decoded, size_t& consumed);
private:
    ChunkedBody();
    ChunkedBody(const ChunkedBody&);
    ChunkedBody& operator=(const ChunkedBody&);
};

#include "Chunked.hpp"

#include <cctype>

static bool hexValue(char c, unsigned int& out)
{
    if (c >= '0' && c <= '9') { out = static_cast<unsigned int>(c - '0'); return true; }
    if (c >= 'a' && c <= 'f') { out = static_cast<unsigned int>(10 + (c - 'a')); return true; }
    if (c >= 'A' && c <= 'F') { out = static_cast<unsigned int>(10 + (c - 'A')); return true; }
    return false;
}

ChunkResult ChunkedBody::decode(const std::string& raw, size_t max_size,
                                std::string& decoded, size_t& consumed)
{
    decoded.clear();
    consumed = 0;

    size_t i = 0;
    const size_t n = raw.size();

    while (true)
    {
        // read the chunk-size line
        size_t line_end = raw.find("\r\n", i);
        if (line_end == std::string::npos)
            return ChunkResult::INCOMPLETE;

        size_t chunk_size = 0;
        bool have_digit = false;
        for (size_t pos = i; pos < line_end; ++pos)
        {
            char c = raw[pos];
            if (c == ';')
                break; // chunk extensions are ignored
            unsigned int v;
            if (!hexValue(c, v))
                return ChunkResult::MALFORMED;
            have_digit = true;
            if (chunk_size > (max_size >> 4))
                return ChunkResult::TOO_LARGE;
            chunk_size = chunk_size * 16 + v;
        }
        if (!have_digit)
            return ChunkResult::MALFORMED;

        if (chunk_size == 0)
        {
            // last: skip header end
            size_t trailer = line_end + 2;
            while (true)
            {
                size_t tl_end = raw.find("\r\n", trailer);
                if (tl_end == std::string::npos)
                    return ChunkResult::INCOMPLETE;
                if (tl_end == trailer)
                {
                    consumed = tl_end + 2;
                    return ChunkResult::COMPLETE;
                }
                trailer = tl_end + 2;
            }
        }

        if (chunk_size > max_size || decoded.size() > max_size - chunk_size)
            return ChunkResult::TOO_LARGE;

        size_t data_start = line_end + 2;
        if (data_start + chunk_size + 2 > n)
            return ChunkResult::INCOMPLETE;
        if (raw[data_start + chunk_size] != '\r' || raw[data_start + chunk_size + 1] != '\n')
            return ChunkResult::MALFORMED;

        decoded.append(raw, data_start, chunk_size);
        i = data_start + chunk_size + 2;
    }
}

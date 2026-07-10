#include "HttpStatusReason.hpp"

const char* HttpStatusReason::reason(Code code) {
    switch (code) {
        // 1xx
        case Code::CONTINUE:            return "Continue";
        case Code::SWITCHING_PROTOCOLS: return "Switching Protocols";
        // 2xx
        case Code::OK:                  return "OK";
        case Code::CREATED:             return "Created";
        case Code::ACCEPTED:            return "Accepted";
        case Code::NO_CONTENT:          return "No Content";
        case Code::PARTIAL_CONTENT:     return "Partial Content";
        // 3xx
        case Code::MOVED_PERMANENTLY:   return "Moved Permanently";
        case Code::FOUND:               return "Found";
        case Code::NOT_MODIFIED:        return "Not Modified";
        case Code::TEMPORARY_REDIRECT:  return "Temporary Redirect";
        // 4xx
        case Code::BAD_REQUEST:            return "Bad Request";
        case Code::UNAUTHORIZED:           return "Unauthorized";
        case Code::FORBIDDEN:              return "Forbidden";
        case Code::NOT_FOUND:              return "Not Found";
        case Code::METHOD_NOT_ALLOWED:     return "Method Not Allowed";
        case Code::REQUEST_TIMEOUT:        return "Request Timeout";
        case Code::CONFLICT:               return "Conflict";
        case Code::GONE:                   return "Gone";
        case Code::LENGTH_REQUIRED:        return "Length Required";
        case Code::PAYLOAD_TOO_LARGE:      return "Payload Too Large";
        case Code::URI_TOO_LONG:           return "URI Too Long";
        case Code::UNSUPPORTED_MEDIA_TYPE: return "Unsupported Media Type";
        case Code::EXPECTATION_FAILED:     return "Expectation Failed";
        case Code::UPGRADE_REQUIRED:       return "Upgrade Required";
        case Code::TOO_MANY_REQUESTS:      return "Too Many Requests";
        // 5xx
        case Code::INTERNAL_SERVER_ERROR:      return "Internal Server Error";
        case Code::NOT_IMPLEMENTED:            return "Not Implemented";
        case Code::BAD_GATEWAY:                return "Bad Gateway";
        case Code::SERVICE_UNAVAILABLE:        return "Service Unavailable";
        case Code::GATEWAY_TIMEOUT:            return "Gateway Timeout";
        case Code::HTTP_VERSION_NOT_SUPPORTED: return "HTTP Version Not Supported";
        default: return "Unknown";
    }
}
const char* HttpStatusReason::reason(unsigned int code) {
    return reason(static_cast<Code>(code));
}
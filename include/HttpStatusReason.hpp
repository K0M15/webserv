#pragma once
#include <map>

class HttpStatusReason {
public:
    enum class Code : unsigned int {
        // 1xx Informational
        CONTINUE                     = 100,
        SWITCHING_PROTOCOLS          = 101,
        // 2xx Success
        OK                           = 200,
        CREATED                      = 201,
        ACCEPTED                     = 202,
        NO_CONTENT                   = 204,
        PARTIAL_CONTENT              = 206,
        // 3xx Redirection
        MOVED_PERMANENTLY            = 301,
        FOUND                        = 302,
        NOT_MODIFIED                 = 304,
        TEMPORARY_REDIRECT           = 307,
        // 4xx Client Error
        BAD_REQUEST                  = 400,
        UNAUTHORIZED                 = 401,
        FORBIDDEN                    = 403,
        NOT_FOUND                    = 404,
        METHOD_NOT_ALLOWED           = 405,
        REQUEST_TIMEOUT              = 408,
        CONFLICT                     = 409,
        GONE                         = 410,
        LENGTH_REQUIRED              = 411,
        PAYLOAD_TOO_LARGE            = 413,
        URI_TOO_LONG                 = 414,
        UNSUPPORTED_MEDIA_TYPE       = 415,
        EXPECTATION_FAILED           = 417,
        UPGRADE_REQUIRED             = 426,
        TOO_MANY_REQUESTS            = 429,
        // 5xx Server Error
        INTERNAL_SERVER_ERROR        = 500,
        NOT_IMPLEMENTED              = 501,
        BAD_GATEWAY                  = 502,
        SERVICE_UNAVAILABLE          = 503,
        GATEWAY_TIMEOUT              = 504,
        HTTP_VERSION_NOT_SUPPORTED   = 505
    };
    static const char* reason(Code code);
    static const char* reason(unsigned int code);
private:
    HttpStatusReason() = delete;
};

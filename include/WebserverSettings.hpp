#pragma once

#include <unordered_map>
#include <map>
#include <charconv>
#include <string>
#include <ostream>
#include <utility>
#include <optional>
#include <Request.hpp>
// #include <BaseResponse.hpp>

#ifndef DEFAULT_MAX_CGI_OUTPUT
# define DEFAULT_MAX_CGI_OUTPUT 2e6
#endif

#ifndef DEFAULT_MAX_BODY_SIZE
# define DEFAULT_MAX_BODY_SIZE 1e6
#endif

#ifndef DEFAULT_MAX_HEADER_SIZE
# define DEFAULT_MAX_HEADER_SIZE 1e6
#endif

typedef enum {
    GET = 0,
    HEAD,
    POST,
    PUT,
    PATCH,
    OPTIONS,
    DELETE,
} Method;

inline const char* method_name(Method m) {
    switch (m) {
        case GET:     return "GET";
        case HEAD:    return "HEAD";
        case POST:    return "POST";
        case PUT:     return "PUT";
        case PATCH:   return "PATCH";
        case OPTIONS: return "OPTIONS";
        case DELETE:  return "DELETE";
        default:      return "UNKNOWN";
    }
}

struct ListenDirective{
    std::string address;
    int         port;
    bool        is_default;
};

inline std::ostream& operator<<(std::ostream& os, const ListenDirective& ld) {
    os << ld.address << ":" << ld.port;
    if (ld.is_default) os << " default_server";
    return os;
}

enum class MissingContentTypePolicy {
    UNSET,
    REJECT,
    DEFAULT
};

struct LocationConfig{
    std::string     path;
    std::string     root;
    std::vector<Method>     methods;
    std::string     index;
    bool            dirindex;
    std::string     redirect;
    std::string     upload_dir;
    std::string     cgi_extension;
    size_t          max_cgi_output;
    MissingContentTypePolicy        missing_content_type_policy;
    std::string                     missing_content_type_default;
    std::unordered_map<unsigned int, std::string>
                                    error_page;
    std::unordered_map<std::string, std::string>
                                    cgi_ext_interpreter;
};

inline std::ostream& operator<<(std::ostream& os, const LocationConfig& loc) {
    os << "    location " << loc.path << " {\n";
    if (!loc.root.empty())  os << "      root:         " << loc.root << "\n";
    if (!loc.methods.empty()) {
        os << "      methods:      ";
        for (size_t i = 0; i < loc.methods.size(); ++i) {
            if (i) os << ", ";
            os << method_name(loc.methods[i]);
        }
        os << "\n";
    }
    if (!loc.index.empty())     os << "      index:        " << loc.index << "\n";
    if (loc.dirindex)           os << "      autoindex:    on\n";
    if (!loc.redirect.empty())  os << "      redirect:     " << loc.redirect << "\n";
    if (!loc.upload_dir.empty())os << "      upload_dir:   " << loc.upload_dir << "\n";
    if (!loc.cgi_extension.empty()) os << "      cgi_ext:      " << loc.cgi_extension << "\n";
    if (loc.missing_content_type_policy != MissingContentTypePolicy::UNSET) {
        os << "      missing_content_type: ";
        switch (loc.missing_content_type_policy) {
            case MissingContentTypePolicy::REJECT: os << "reject"; break;
            case MissingContentTypePolicy::DEFAULT:
                os << "default " << loc.missing_content_type_default;
                break;
            default: break;
        }
        os << "\n";
    }
    os << "    }\n";
    return os;
}

class WebserverSettings{
private:
    static WebserverSettings getDefaultSettings();
public:
    WebserverSettings():
        dirindex(false),
        missing_content_type_policy(MissingContentTypePolicy::REJECT),
        max_header_size(DEFAULT_MAX_HEADER_SIZE),
        max_body_size(DEFAULT_MAX_BODY_SIZE) {}
    ~WebserverSettings() = default;
    std::vector<ListenDirective>    listen;
    std::vector<std::string>        server_name;
    std::vector<Method>             methods;
    std::string                     root;
    std::string                     index;
    std::unordered_map<unsigned int, std::string>
                                    error_page;
    bool                            dirindex;
    MissingContentTypePolicy        missing_content_type_policy;
    std::string                     missing_content_type_default;
    std::unordered_map<std::string, LocationConfig> locations;
    size_t                          max_header_size;
    size_t                          max_body_size;
    std::string                     redirect;
    std::string                     upload_dir;
    std::unordered_map<std::string, std::string>
                                    cgi_ext_interpreter;
    size_t                          max_cgi_output;
    static WebserverSettings fromBlock(const std::string& block);
};


inline std::ostream& operator<<(std::ostream& os, const WebserverSettings& ws) {
    os << "  listen:        ";
    for (size_t i = 0; i < ws.listen.size(); ++i) {
        if (i) os << ", ";
        os << ws.listen[i];
    }
    os << "\n";
    os << "  server_name:   ";
    for (size_t i = 0; i < ws.server_name.size(); ++i) {
        if (i) os << " ";
        os << ws.server_name[i];
    }
    os << "\n";
    os << "  root:          " << ws.root << "\n";
    os << "  index:         " << ws.index << "\n";
    os << "  autoindex:     " << (ws.dirindex ? "on" : "off") << "\n";
    os << "  max_body_size: " << ws.max_body_size << " bytes\n";
    os << "  missing_content_type: ";
    switch (ws.missing_content_type_policy) {
        case MissingContentTypePolicy::UNSET:  os << "unset"; break;
        case MissingContentTypePolicy::REJECT: os << "reject"; break;
        case MissingContentTypePolicy::DEFAULT:
            os << "default " << ws.missing_content_type_default;
            break;
    }
    os << "\n";
    for (const auto& [path, loc] : ws.locations) {
        os << loc;
    }
    return os;
}

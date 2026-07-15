#pragma once

#include <map>
#include <string>
#include <ostream>
#include <utility>
#include <optional>
#include <Request.hpp>
// #include <BaseResponse.hpp>

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
    REJECT,
    DEFAULT
};

struct LocationConfig{
    std::string     path;
    std::optional<std::string> root;
    std::vector<Method>     methods;
    std::string     index;
    bool            dirindex;
    std::string     redirect;
    std::string     upload_dir;
    std::string     cgi_extension;
    std::optional<MissingContentTypePolicy> missing_content_type_policy;
    std::optional<std::string>              missing_content_type_default;
};

inline std::ostream& operator<<(std::ostream& os, const LocationConfig& loc) {
    os << "    location " << loc.path << " {\n";
    if (loc.root)       os << "      root:         " << *loc.root << "\n";
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
    if (loc.missing_content_type_policy.has_value()) {
        os << "      missing_content_type: ";
        switch (loc.missing_content_type_policy.value()) {
            case MissingContentTypePolicy::REJECT: os << "reject"; break;
            case MissingContentTypePolicy::DEFAULT:
                os << "default " << loc.missing_content_type_default.value_or("");
                break;
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
    WebserverSettings() = default;
    ~WebserverSettings() = default;
    std::vector<ListenDirective>    listen;
    std::vector<std::string>        server_name;
    std::string                     root;
    std::string                     index;
    bool                            dirindex;
    MissingContentTypePolicy        missing_content_type_policy;
    std::string                     missing_content_type_default;
    std::map<std::string, LocationConfig> locations;
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
    os << "  missing_content_type: ";
    switch (ws.missing_content_type_policy) {
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

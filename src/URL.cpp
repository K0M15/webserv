#include "URL.hpp"
#include <cctype>
#include <algorithm>

URL::URL() : value() {}

URL::URL(const std::string& s) : value(s) {
    if (!isValidURL(value)) {
        throw HttpServerException("URL String does not match any valid request-target pattern");
    }
}

const std::string URL::str() const {
    return value;
}

void URL::setURL(const std::string& s) {
    URL temp(s);
    this->value = s;
}

const std::string URL::getRawQuery() const {
    size_t separator = value.find('?');
    if (separator == std::string::npos)
        return "";
    return value.substr(separator + 1);
}

const std::string URL::getFileExt() const {
    std::string path = PathUtils::stripQuery(value);
    size_t dot = path.rfind('.');
    size_t slash = path.rfind('/');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
        return "";
    return path.substr(dot);
}

const std::unordered_map<std::string, std::string> URL::getQuery() const {
    std::unordered_map<std::string, std::string> result;
    std::string raw = getRawQuery();
    size_t pos = 0;
    while (pos < raw.size()) {
        size_t next_and = raw.find('&', pos);
        std::string curr_pair = (next_and == std::string::npos ? raw.substr(pos) : raw.substr(pos, next_and - pos));
        size_t curr_equ = curr_pair.find('=');
        if (curr_equ != std::string::npos)
            result[curr_pair.substr(0, curr_equ)] = curr_pair.substr(curr_equ + 1);
        else if (!curr_pair.empty())
            result[curr_pair] = "";
        if (next_and == std::string::npos)
            break;
        pos = next_and + 1;
    }
    return result;
}

static std::string decodeDotAndSlash(const std::string& input) {
    std::string result;
    result.reserve(input.size());
    for (size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '%' && i + 2 < input.size()) {
            char h1 = static_cast<char>(std::tolower(static_cast<unsigned char>(input[i + 1])));
            char h2 = static_cast<char>(std::tolower(static_cast<unsigned char>(input[i + 2])));
            if (h1 == '2' && h2 == 'e') {
                result += '.';
                i += 2;
                continue;
            }
            if (h1 == '2' && h2 == 'f') {
                result += '/';
                i += 2;
                continue;
            }
        }
        result += input[i];
    }
    return result;
}

bool URL::isValidURL(const std::string& url) {
    if (url.empty())
        return false;

    for (char c : url) {
        if (std::iscntrl(static_cast<unsigned char>(c)) || std::isspace(static_cast<unsigned char>(c)))
            return false;
    }

    if (url.find('#') != std::string::npos)
        return false;

    if (url == "*")
        return true;

    auto hasDotSegments = [](const std::string& raw_path) -> bool {
        std::string path = decodeDotAndSlash(raw_path);
        size_t start = 0;
        while (start < path.size()) {
            size_t end = path.find('/', start);
            if (end == std::string::npos)
                end = path.size();
            std::string seg = path.substr(start, end - start);
            if (seg == "." || seg == "..")
                return true;
            start = end + 1;
        }
        return false;
    };

    if (url[0] == '/') {
        std::string path_part = PathUtils::stripQuery(url);
        return !hasDotSegments(path_part);
    }

    size_t scheme_end = url.find("://");
    if (scheme_end == std::string::npos)
        return false;

    std::string scheme = url.substr(0, scheme_end);
    for (char& c : scheme)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    if (scheme != "http" && scheme != "https" && scheme != "ftp")
        return false;

    std::string rest = url.substr(scheme_end + 3);
    if (rest.empty())
        return false;

    size_t auth_end = rest.find_first_of("/?");
    std::string auth = (auth_end == std::string::npos) ? rest : rest.substr(0, auth_end);

    if (auth.length() < 2)
        return false;

    char first_auth = auth[0];
    if (first_auth == '/' || first_auth == '$' || first_auth == '.' || first_auth == '?')
        return false;

    if (auth_end != std::string::npos && rest[auth_end] == '/') {
        std::string path_and_more = rest.substr(auth_end);
        std::string path_part = PathUtils::stripQuery(path_and_more);
        if (hasDotSegments(path_part))
            return false;
    }

    return true;
}

std::istream& operator>>(std::istream& is, URL& url) {
    std::string temp;
    if (is >> temp)
        url.setURL(temp);
    return is;
}

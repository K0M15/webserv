#include "PathUtils.hpp"


std::string PathUtils::stripQuery(const std::string& url)
{
    std::string::size_type cut = url.find_first_of("?#");
    if (cut == std::string::npos)
        return url;
    return url.substr(0, cut);
}

bool PathUtils::isSafeRelative(const std::string& p)
{
    if (p.empty())
        return false;
    if (p[0] == '/')
        return false;
    if (p.find('\0') != std::string::npos)
        return false;

    std::string::size_type start = 0;
    while (start < p.size())
    {
        std::string::size_type end = p.find('/', start);
        if (end == std::string::npos)
            end = p.size();

        std::string seg = p.substr(start, end - start);
        if (seg == ".." || seg == ".")
            return false;
        if (seg.empty())
            return false;
        start = end + 1;
    }
    return true;
}


// build a safe system path 
//   base   = "/var/www/uploads"
//   url    = "/upload/bilder/a.png?v=2"
//   prefix = "/upload"
//   -> out = "/var/www/uploads/bilder/a.png"

PathUtils::ResolveResult PathUtils::resolveUnder(const std::string& base, const std::string& url_path,
                                                 const std::string& location_prefix, std::string& out, bool allow_nested)
{
    std::string path = stripQuery(url_path);

    if (!location_prefix.empty()
        && path.size() >= location_prefix.size()
        && path.compare(0, location_prefix.size(), location_prefix) == 0)
    {
        path.erase(0, location_prefix.size());
    }

    while (!path.empty() && path[0] == '/')
        path.erase(0, 1);

    while (!path.empty() && path[path.size() - 1] == '/')
        path.erase(path.size() - 1);

    if (path.empty())
    {
        out.clear();
        return RESOLVE_BAD_PATH;
    }

    if (!isSafeRelative(path))
    {
        out.clear();
        return RESOLVE_BAD_PATH;
    }
    if (!allow_nested && path.find('/') != std::string::npos)
    {
        out.clear();
        return RESOLVE_BAD_PATH;
    }

    std::string prefix = base;
    while (!prefix.empty() && prefix[prefix.size() - 1] == '/')
        prefix.erase(prefix.size() - 1);

    out = prefix + "/" + path;
    return RESOLVE_OK;
}

bool PathUtils::splitPathInfo(
    const std::string& url_path,
    const std::unordered_map<std::string, std::string>& interpreters,
    std::string& script_name,
    std::string& path_info,
    std::string& matched_ext)
{
    script_name.clear();
    path_info.clear();
    matched_ext.clear();

    if (interpreters.empty())
        return false;

    std::string path = stripQuery(url_path);
    if (path.empty())
        return false;

    size_t pos = 0;
    while (pos < path.size())
    {
        size_t next_slash = path.find('/', pos);
        std::string segment_prefix = (next_slash == std::string::npos)
                                         ? path
                                         : path.substr(0, next_slash);

        if (!segment_prefix.empty())
        {
            for (const auto& [ext, interp] : interpreters)
            {
                (void)interp;
                if (ext.empty())
                    continue;
                if (segment_prefix.size() >= ext.size() &&
                    segment_prefix.compare(segment_prefix.size() - ext.size(), ext.size(), ext) == 0)
                {
                    script_name = segment_prefix;
                    path_info = (next_slash == std::string::npos)
                                    ? ""
                                    : path.substr(next_slash);
                    matched_ext = ext;
                    return true;
                }
            }
        }

        if (next_slash == std::string::npos)
            break;
        pos = next_slash + 1;
    }
    return false;
}

std::string PathUtils::translatePath(const std::string& root, const std::string& path_info)
{
    if (path_info.empty())
        return "";
    if (root.empty())
        return path_info;

    std::string clean_root = root;
    while (!clean_root.empty() && clean_root.back() == '/')
        clean_root.pop_back();

    std::string clean_info = path_info;
    if (!clean_info.empty() && clean_info.front() == '/')
        return clean_root + clean_info;
    return clean_root + "/" + clean_info;
}

const char *PathUtils::mimeType(const std::string &filename)
{
    auto pos = filename.rfind('.');
    if (pos == std::string::npos)
        return "application/octet-stream";
    std::string ext = filename.substr(pos);
    if (ext == ".html")
        return "text/html";
    if (ext == ".htm")
        return "text/html";
    if (ext == ".css")
        return "text/css";
    if (ext == ".js")
        return "application/javascript";
    if (ext == ".png")
        return "image/png";
    if (ext == ".jpg" || ext == ".jpeg")
        return "image/jpeg";
    if (ext == ".gif")
        return "image/gif";
    if (ext == ".ico")
        return "image/x-icon";
    if (ext == ".txt")
        return "text/plain";
    if (ext == ".pdf")
        return "application/pdf";
    if (ext == ".json")
        return "application/json";
    if (ext == ".xml")
        return "application/xml";
    if (ext == ".svg")
        return "image/svg+xml";
    return "application/octet-stream";
}

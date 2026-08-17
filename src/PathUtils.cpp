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
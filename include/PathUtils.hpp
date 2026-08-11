#ifndef PATHUTILS_HPP
#define PATHUTILS_HPP

#include <string>

class PathUtils
{
public:
    enum ResolveResult
    {
        RESOLVE_OK,
        RESOLVE_BAD_PATH,
        RESOLVE_EMPTY
    };

    static std::string    stripQuery(const std::string& url);
    static bool           isSafeRelative(const std::string& p);
    static ResolveResult  resolveUnder(const std::string& base, const std::string& url_path,
                                        const std::string& location_prefix, std::string& out);
private:
    PathUtils();
    PathUtils(const PathUtils&);
    PathUtils& operator=(const PathUtils&);
};

#endif
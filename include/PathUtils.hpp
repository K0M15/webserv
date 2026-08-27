#ifndef PATHUTILS_HPP
#define PATHUTILS_HPP

#include <string>
#include <unordered_map>

class PathUtils
{
public:
    enum ResolveResult
    {
        RESOLVE_OK,
        RESOLVE_BAD_PATH,
    };

    static std::string    stripQuery(const std::string& url);
    static bool           isSafeRelative(const std::string& p);
    static ResolveResult  resolveUnder(const std::string& base, const std::string& url_path,
                                        const std::string& location_prefix, std::string& out, bool allow_nested = true);
    // Will return true, if a script is identified, setting filling script_name, path_info, matched_ext
    // If not found, it will reset these values
    static bool           splitPathInfo(const std::string& url_path,
                                        const std::unordered_map<std::string, std::string>& interpreters,
                                        std::string& script_name,
                                        std::string& path_info,
                                        std::string& matched_ext);
    // translates a path_info from splitPathInfo into the filesystem 
    static std::string    translatePath(const std::string& root, const std::string& path_info);
    static const char*    mimeType(const std::string &filename);
private:
    PathUtils();
    PathUtils(const PathUtils&);
    PathUtils& operator=(const PathUtils&);
};

#endif
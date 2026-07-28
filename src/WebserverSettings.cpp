#include "WebserverSettings.hpp"
#include <sstream>
#include <cstdlib>



// static std::string valueAfter(const std::string& line, const std::string& keyword)
// {
//     size_t pos = keyword.size();
//     while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t'))
//         ++pos;
//     std::string val = line.substr(pos);
//     if (!val.empty() && val.back() == ';')
//         val.pop_back();
//     return val;
// }

struct ConfigTarget{
    std::string*                root;
    std::string*                index;
    bool*                       dirindex;
    MissingContentTypePolicy*   mct_policy;
    std::string*                mct_default;
    std::unordered_map<unsigned int, std::string>*
                                error_page;
    std::vector<ListenDirective>*
                                interface;
    std::string*                server_name;
    unsigned long*              max_header_size;
    unsigned long*              max_body_size;
    std::vector<std::string>*   methods;
    std::string*                redirect;
    std::string*                upload_dir;
    std::unordered_map<std::string, std::string>*
                                cgi_ext_interpreter;
};
using Handler = std::function<void(const std::string& val, ConfigTarget target)>;
/* val is the value after the key in config and what should be placed in the target*/
#define PUT_INTO(x) [](const std::string& val, ConfigTarget t){x}
const std::unordered_map<std::string, Handler> entryParser = {
    {"root", PUT_INTO( *t.root = val; )},
    {"index", PUT_INTO( *t.index = val; )},
    {"dirindex", PUT_INTO( *t.dirindex = (val == "on" || val == "true");)},
    {"missing_content_type", PUT_INTO(
        if (val == "reject"){
            *t.mct_policy = MissingContentTypePolicy::REJECT;
        } else if (val.compare(0, 7, "default") == 0){
            *t.mct_policy = MissingContentTypePolicy::DEFAULT;
            std::string def = val.substr(8);
            if (def.empty()) throw std::runtime_error("missing_content_type default requires a media type");
            *t.mct_default = def;                
        } else {
            throw std::runtime_error("invalid missing_content_type: " + val);
        }
    )},
    {"error_page", PUT_INTO(
        std::istringstream ss(val);
        unsigned int code;
        std::string path;
        ss >> code >> path;
        if (path.empty())
            throw std::runtime_error("invalid error_page" + val);
        (*t.error_page)[code] = path;
    )},
    {"listen", PUT_INTO(
        if (t.interface == nullptr)
            throw std::runtime_error("Listen directive in location block");
        ListenDirective dir;
        dir.is_default = (val.find("default_server") != std::string::npos);
        size_t colon = val.rfind(':');
        if (colon != std::string::npos)
        {
            dir.address = val.substr(0, colon);
            dir.port = std::atoi(val.substr(colon + 1).c_str());
        }
        else
        {
            dir.address = "0.0.0.0";
            dir.port = std::atoi(val.c_str());
        }
        t.interface->push_back(dir);
    )},
    {"server_name", PUT_INTO(
        if(t.server_name == nullptr)
            throw std::runtime_error("server_name in location block");
        *t.server_name = val;
    )},
    {"max_header_size", PUT_INTO(
        if (t.max_header_size == nullptr)
            throw std::runtime_error("max_header_size inside location block");
        unsigned long mhs = std::stoul(val);
        if (mhs < 50 || mhs > 8192)
            throw std::runtime_error("max_header_size must be between 50 and 8192 bytes");
        *t.max_header_size = mhs;
    )},
    {"max_body_size", PUT_INTO(
        if (t.max_body_size == nullptr)
            throw std::runtime_error("max_body_size inside location block");
        unsigned long mbs = std::stoul(val);
        if (mbs < 50 || mbs > 8192)
            throw std::runtime_error("max_body_size must be between 50 and 8192 bytes");
        *t.max_header_size = mbs;
    )},
    {"methods", PUT_INTO(
        // consume each method and put into vector
    )},
    {"redirect", PUT_INTO(
        *t.redirect = val;
    )},
    {"cgi", PUT_INTO(
        // first = fileending, second = interpreter path
    )}
};

static void dispatch(const std::string& line, ConfigTarget target) {
    // Split "root /var/www;" -> keyword="root", value="/var/www"
    size_t space = line.find_first_of(" \t");
    std::string keyword = line.substr(0, space);
    std::string value;
    if (space != std::string::npos) {
        value = line.substr(space + 1);
        // strip trailing ';'
        if (!value.empty() && value.back() == ';')
            value.pop_back();
    }

    auto it = entryParser.find(keyword);
    if (it != entryParser.end())
        it->second(value, target);
    else
        std::cout << "[Warning] Unknown directive: " << line << std::endl;
}

std::string parseLocationPath(const std::string& line)
{
    size_t pos = 8;
    while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t'))
        ++pos;
    if (pos >= line.size())
        throw std::runtime_error("Malformed location: missing path and '{'");
    size_t brace = line.find('{', pos);
    if (brace == std::string::npos)
        throw std::runtime_error("Malformed location: missing '{'");
    std::string path = line.substr(pos, brace - pos);
    while (!path.empty() && (path.back() == ' ' || path.back() == '\t'))
        path.pop_back();
    if (path.empty())
        throw std::runtime_error("Malformed location: empty path");
    return path;
}

WebserverSettings WebserverSettings::fromBlock(const std::string& block)
{
    
    WebserverSettings settings;
    ConfigTarget starget{
        .root       = &settings.root,
        .index      = &settings.index,
        .dirindex   = &settings.dirindex,
        .mct_policy = &settings.missing_content_type_policy,
        .mct_default= &settings.missing_content_type_default,
        .error_page = &settings.error_page,
        .interface  = &settings.listen,
        .server_name = &settings.server_name,
        .max_header_size = &settings.max_header_size,
        .max_body_size = &settings.max_header_size,
        .methods = &settings.methods,
        .redirect = &settings.redirect,
        .upload_dir = &settings.upload_dir,
        .cgi_ext_interpreter = &settings.cgi_ext_interpreter,
    };
    LocationConfig current;
    bool in_loc = false;
    ConfigTarget loc_target;
    loc_target.server_name = nullptr;
    loc_target.interface = nullptr;
    loc_target.max_body_size = nullptr;
    loc_target.max_header_size = nullptr;

    std::istringstream stream(block);
    std::string line;
    while(std::getline(stream, line))
    {
        if (line.empty() || line[0] == '#') continue;
        if (!in_loc && line.compare(0, 8, "location") == 0) {
            in_loc = true;
            current = LocationConfig();
            std::string path = parseLocationPath(line);
            continue;
        }
        if (in_loc && line == "}") {
            settings.locations[current.path] = current;
            in_loc = false;
            continue;
        }

        ConfigTarget& target = in_loc ? loc_target : starget;
        dispatch(line, target);
    }
    return settings;
}

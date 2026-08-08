#include "WebserverSettings.hpp"
#include <sstream>
#include <cstdlib>
#include <algorithm>
#include <cctype>
#include <limits>

static std::string valueAfter(const std::string& line, const std::string& keyword)
{
    size_t pos = keyword.size();
    while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t'))
        ++pos;
    std::string val = line.substr(pos);
    if (!val.empty() && val.back() == ';')
        val.pop_back();
    return val;
}

WebserverSettings WebserverSettings::getDefaultSettings()
{
    WebserverSettings settings;
    settings.dirindex = false;
    settings.index = "index.html";
    settings.root = "";
    settings.missing_content_type_policy = MissingContentTypePolicy::REJECT;
    settings.max_cgi_output = DEFAULT_MAX_CGI_OUTPUT;
    settings.max_body_size = DEFAULT_MAX_BODY_SIZE;
    settings.max_header_size = DEFAULT_MAX_HEADER_SIZE;
    return settings;
}

struct ConfigTarget{
    std::string*                            root;
    std::string*                            index;
    bool*                                   dirindex;
    MissingContentTypePolicy*               mct_policy;
    std::string*                            mct_default;
    std::unordered_map<unsigned int, std::string>*
                                            error_page;
    std::vector<ListenDirective>*           interface;
    std::vector<std::string>*               server_name;
    unsigned long*                          max_header_size;
    unsigned long*                          max_body_size;
    std::vector<Method>*                    methods;
    std::string*                            redirect;
    std::string*                            upload_dir;
    std::string*                            cgi_extension;
    std::unordered_map<std::string, std::string>*
                                            cgi_ext_interpreter;
    size_t*                                 max_cgi_output;
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
            std::string def = valueAfter(val, "default");
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
        std::istringstream ns(val);
        std::string name;
        while (ns >> name)
            t.server_name->push_back(name);
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
        if (mbs < 50 || mbs > 100UL * 1024UL * 1024UL)
            throw std::runtime_error("max_body_size must be between 50 and 104857600 bytes");
        *t.max_body_size = mbs;
    )},
    {"methods", PUT_INTO(
        // consume each method and put into vector
        if (t.methods == nullptr)
            throw std::runtime_error("'methods' directive has no target");
        std::istringstream ms(val);
        std::string name;
        while (ms >> name) {
            if (name == "get")      t.methods->push_back(GET);
            else if (name == "head")    t.methods->push_back(HEAD);
            else if (name == "post")    t.methods->push_back(POST);
            else if (name == "put")     t.methods->push_back(PUT);
            else if (name == "patch")   t.methods->push_back(PATCH);
            else if (name == "options") t.methods->push_back(OPTIONS);
            else if (name == "delete")  t.methods->push_back(DELETE);
            else throw std::runtime_error("unknown method: " + name);
        }
        if (t.methods->empty())
            throw std::runtime_error("'methods' requires at least one method");
    )},
    {"redirect", PUT_INTO(
        *t.redirect = val;
    )},
    {"cgi", PUT_INTO(
        if (t.cgi_ext_interpreter == nullptr)
            throw std::runtime_error("'cgi' directive has no target");
        std::istringstream cs(val);
        std::string ext;
        std::string interp;
        cs >> ext >> interp;
        if (ext.empty() || interp.empty())
            throw std::runtime_error("'cgi' requires <extension> <interpreter-path>: " + val);
        (*t.cgi_ext_interpreter)[ext] = interp;
    )},
    {"cgi_extension", PUT_INTO(
        if (t.cgi_extension == nullptr)
            throw std::runtime_error("'cgi_extension' not allowed here");
        *t.cgi_extension = val;
    )},
    {"upload_dir", PUT_INTO(
        if (t.upload_dir == nullptr)
            throw std::runtime_error("'upload_dir' not allowed here");
        *t.upload_dir = val;
    )},
    {"max_cgi_output", PUT_INTO(
        *t.max_cgi_output = std::strtoul(val.c_str(), nullptr, 10);
        if (*t.max_cgi_output == std::numeric_limits<unsigned long>::max())
            *t.max_cgi_output = DEFAULT_MAX_CGI_OUTPUT;
    )}
};

static void dispatch(const std::string& line, ConfigTarget target) {
    size_t space = line.find_first_of(" \t");
    std::string keyword = line.substr(0, space);
    for (auto& c : keyword) c = std::tolower(c);

    std::string value = valueAfter(line, keyword);
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
    WebserverSettings settings = getDefaultSettings();
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
        .max_body_size = &settings.max_body_size,
        .methods = &settings.methods,
        .redirect = &settings.redirect,
        .upload_dir = &settings.upload_dir,
        .cgi_extension = nullptr,
        .cgi_ext_interpreter = &settings.cgi_ext_interpreter,
        .max_cgi_output = &settings.max_cgi_output,
    };
    LocationConfig current;
    bool in_loc = false;
    ConfigTarget loc_target{};
    loc_target.server_name = nullptr;
    loc_target.interface = nullptr;
    loc_target.max_header_size = nullptr;
    loc_target.max_body_size = nullptr;
    loc_target.cgi_extension = nullptr;

    std::istringstream stream(block);
    std::string line;
    while(std::getline(stream, line))
    {
        if (line.empty() || line[0] == '#') continue;

        // case-insensitive "location" check
        if (!in_loc && line.size() > 8) {
            std::string prefix = line.substr(0, 8);
            for (auto& c : prefix) c = std::tolower(c);
            if (prefix == "location") {
                in_loc = true;
                current = LocationConfig();
                current.path = parseLocationPath(line);
                loc_target.root = &current.root;
                loc_target.index = &current.index;
                loc_target.dirindex = &current.dirindex;
                loc_target.mct_policy = &current.missing_content_type_policy;
                loc_target.mct_default = &current.missing_content_type_default;
                loc_target.error_page = &current.error_page;
                loc_target.methods = &current.methods;
                loc_target.redirect = &current.redirect;
                loc_target.upload_dir = &current.upload_dir;
                loc_target.cgi_extension = &current.cgi_extension;
                loc_target.cgi_ext_interpreter = &current.cgi_ext_interpreter;
                loc_target.max_cgi_output = &current.max_cgi_output;
                continue;
            }
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

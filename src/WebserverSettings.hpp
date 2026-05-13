#include <map>
#include <string>
#include <ostream>
#include <utility>
#include <Request.hpp>
#include <BaseResponse.hpp>

class BaseHandler
{
private:

public:
    BaseHandler();
    virtual ~BaseHandler();
    void processRequest(Request& req, BaseResponse& res);
};

class StaticFileHandler : public BaseHandler
{
private:
public:
};

class CGIHandler : public BaseHandler
{

};

class WebserverSettings{
private:
    int addr;
    int port;
    std::map<std::string, std::string> locations; // this should be different handles we match for... a location object should create a handler, e.g. static files or cgi
    std::map<int, std::string> error_page;
    WebserverSettings() = default;
    ~WebserverSettings() = default;
public:
    static WebserverSettings fromFile(const char * location);
};

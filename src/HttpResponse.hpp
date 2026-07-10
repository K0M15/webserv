#include <string>
#include <map>
#include <ostream>
#include "HttpServerException.hpp"
#include "HttpStatusReason.hpp"


class HttpResponse : public BaseResponse
{
private:
    std::map<std::string, std::string> headers;
    std::string body;
    unsigned int target; //filedescriptor
    HttpStatusReason status;
    HttpResponse();
public:
    explicit HttpResponse(int target);
    HttpResponse(HttpResponse& other);
    HttpResponse& operator=(HttpResponse& other);
    ~HttpResponse();
    void addHeader(std::string& key, std::string& value) {
        if (headers[key].empty()){
            headers[key] = value;
            return;
        }
        throw HttpServerException("Header already exists");
    }

    unsigned int getStatus();
    void getStatus(unsigned int status);

    void removeHeader(std::string& key)
    {
        if (!headers[key].empty())
            headers[key] = std::string::empty();
    }

    const std::map<std::string, std::string>& getHeaders()
    {
        return this->headers;
    }
    
    void writeStartLine()
    {
        auto s = std::string("HTTP/1.1 ");
        s += std::to_string(status);
        write()
    }

    void writeHeaders()
    {
    }

    void write(const char* str);
};
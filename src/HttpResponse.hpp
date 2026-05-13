#include <string>
#include <map>
#include <HttpServerException.hpp>


class HttpResponse : public BaseResponse
{
private:
    std::map<std::string, std::string> headers;
    std::string body;
public:
    HttpResponse();
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

    void removeHeader(std::string& key)
    {
        if (!headers[key].empty())
            headers[key] = std::string::empty();
    }

    const std::map<std::string, std::string>& getHeaders()
    {
        return this->headers;
    }
    
};
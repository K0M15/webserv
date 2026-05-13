
#include <string>
#include <exception>

class BaseResponse
{
private:
    int fd;
    std::string wfile;
    void _write();
public:
    BaseResponse();
    BaseResponse(const BaseResponse& other);
    BaseResponse& operator=(const BaseResponse& other);
    ~BaseResponse();
    void write(const std::string& data) {
        // register poll handler on fd, if writeable, write data
    }
};
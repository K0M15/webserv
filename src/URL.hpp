
#include <string>
#include <regex>

class URL{
public:
    class NonValidURLException : public std::exception{
    private:
        std::string reason;
    public:
        explicit NonValidURLException(const std::string& s) : reason(s) {}
        const char* what() const noexcept override{
            return this->reason.c_str();
        }
    };
    URL() : value() {}
    explicit URL(const std::string& s) : value(std::move(s)){
        // allow absolute URI, absolute path (origin-form), or asterisk (*)
        const std::regex url_pattern(
        R"(^((https?|ftp)://[^\s/$.?#].[^\s]*|/[^\s]*|\*)$)", 
        std::regex::icase);
        if (!std::regex_match(value, url_pattern))
        {
            throw NonValidURLException("URL String does not match any valid request-target pattern");
        }
    }
    const std::string str() const { return value;}
    void setURL(const std::string& s){
        URL url(s); //checks data
        this->value = std::move(s); // replaces current value
    }
private:
    std::string value;
};

inline std::istream& operator>>(std::istream& is, URL& url){
    std::string temp;
    if (is >> temp)
        url.setURL(temp);
    return is;
}
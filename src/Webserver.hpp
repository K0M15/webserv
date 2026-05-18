class Webserver
{
private:
    Webserver();
public:
    Webserver(const WebserverSettings& settings);
    ~Webserver();
    Webserver(const Webserver& other);
    Webserver& operator=(cost Webserver& other);
    void listen(); // main loop
};

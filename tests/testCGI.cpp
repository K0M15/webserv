#include "CGIHandler.hpp"
#include "Connection.hpp"
#include "WebserverSettings.hpp"
#include "PollHandler.hpp"
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <string>
#include <sys/wait.h>

static int g_passed = 0;
static int g_failed = 0;

static void pass(const char* name) {
    std::cout << "[SUCCESS] " << name << std::endl;
    ++g_passed;
}

static void fail(const char* name, const char* msg) {
    std::cout << "[FAILURE] " << name;
    if (msg && msg[0]) std::cout << " - " << msg;
    std::cout << std::endl;
    ++g_failed;
}

static void check(const char* name, bool condition, const char* fail_msg = "") {
    if (condition)
        pass(name);
    else
        fail(name, fail_msg);
}

static bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

struct CGIResult {
    int status;
    std::string out;
};

static bool exitedCleanly(int status) {
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static CGIResult runCGI(const std::string& rawRequest, const std::string& scriptPath) {
    Request req = Request::fromString(rawRequest);

    WebserverSettings settings;
    settings.root = "tests";
    settings.server_name.push_back("localhost");
    settings.listen.push_back({"127.0.0.1", 8080, false});

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    std::vector<const WebserverSettings *> set;
    set.push_back(&settings);

    Connection conn(-1, addr, set);

    CGIHandler handler(scriptPath, "/usr/bin/python3", req, conn, [&handler](){
        std::cout << "CGI exit: " << handler.getExitStatus() << ", Message " << handler.getOutput() << std::endl;
    });

    PollHandler& poll = PollHandler::getInstance();
    poll.setTimeout(50);
    for (int i = 0; i < 100 && !handler.isDone(); ++i)
        poll.checkFDs();
    poll.setTimeout(3000);

    CGIResult r;
    r.status = handler.getExitStatus();
    r.out = handler.getOutput();
    return r;
}

int main(int argc, char* argv[]) {
    std::string script = "tests/echo_env.py";
    if (argc == 2)
        script = argv[1];

    // --------------- GET with query string + rich headers ---------------
    const std::string getRequest =
        "GET /echo_env.py?name=alain&lang=cpp HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "User-Agent: webserv-test/1.0\r\n"
        "Accept: text/html\r\n"
        "Accept-Language: en-US\r\n"
        "Cookie: session=abc123\r\n"
        "X-Custom-Header: custom-value\r\n"
        "\r\n";

    CGIResult get = runCGI(getRequest, script);
    std::cout << "=== GET /echo_env.py?name=alain&lang=cpp ===" << std::endl;
    std::cout << "exit status: " << get.status
              << " (clean exit: " << (exitedCleanly(get.status) ? "yes" : "no") << ")"
              << std::endl;
    std::cout << "--- CGI output ---" << std::endl;
    std::cout << get.out << std::endl;
    std::cout << "------------------" << std::endl;

    check("GET: cgi exited 0", exitedCleanly(get.status));
    check("GET: CGI header Content-Type present",
        contains(get.out, "Content-Type: text/plain"));
    check("GET: QUERY_STRING passed",
        contains(get.out, "QUERY_STRING=name=alain&lang=cpp"));
    check("GET: REQUEST_METHOD=GET", contains(get.out, "REQUEST_METHOD=GET"));
    check("GET: SERVER_PORT=8080", contains(get.out, "SERVER_PORT=8080"));
    check("GET: SERVER_NAME=localhost", contains(get.out, "SERVER_NAME=localhost"));
    check("GET: REMOTE_ADDR=127.0.0.1", contains(get.out, "REMOTE_ADDR=127.0.0.1"));
    check("GET: SCRIPT_NAME=/echo_env.py", contains(get.out, "SCRIPT_NAME=/echo_env.py"));
    check("GET: PATH_INFO empty", contains(get.out, "PATH_INFO=\n") || contains(get.out, "PATH_INFO=\r\n"));
    check("GET: PATH_TRANSLATED empty", contains(get.out, "PATH_TRANSLATED=\n") || contains(get.out, "PATH_TRANSLATED=\r\n"));
    check("GET: HTTP_USER_AGENT passed",
        contains(get.out, "HTTP_USER_AGENT=webserv-test/1.0"));
    check("GET: HTTP_COOKIE passed", contains(get.out, "HTTP_COOKIE=session=abc123"));
    check("GET: HTTP_X_CUSTOM_HEADER passed",
        contains(get.out, "HTTP_X_CUSTOM_HEADER=custom-value"));
    check("GET: REDIRECT_STATUS=200", contains(get.out, "REDIRECT_STATUS=200"));
    check("GET: no HTTP_CONTENT_TYPE (must stay CONTENT_TYPE)",
        !contains(get.out, "HTTP_CONTENT_TYPE"));

    // --------------- GET with PATH_INFO and PATH_TRANSLATED ---------------
    const std::string getPathInfoRequest =
        "GET /echo_env.py/extra/path/info?key=value HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "\r\n";

    CGIResult pathInfoRes = runCGI(getPathInfoRequest, script);
    std::cout << "=== GET /echo_env.py/extra/path/info?key=value ===" << std::endl;
    std::cout << "exit status: " << pathInfoRes.status
              << " (clean exit: " << (exitedCleanly(pathInfoRes.status) ? "yes" : "no") << ")"
              << std::endl;
    std::cout << "--- CGI output ---" << std::endl;
    std::cout << pathInfoRes.out << std::endl;
    std::cout << "------------------" << std::endl;

    check("PATH_INFO: cgi exited 0", exitedCleanly(pathInfoRes.status));
    check("PATH_INFO: SCRIPT_NAME=/echo_env.py", contains(pathInfoRes.out, "SCRIPT_NAME=/echo_env.py"));
    check("PATH_INFO: PATH_INFO=/extra/path/info", contains(pathInfoRes.out, "PATH_INFO=/extra/path/info"));
    check("PATH_INFO: PATH_TRANSLATED=tests/extra/path/info", contains(pathInfoRes.out, "PATH_TRANSLATED=tests/extra/path/info"));
    check("PATH_INFO: QUERY_STRING=key=value", contains(pathInfoRes.out, "QUERY_STRING=key=value"));

    // --------------- POST with a request body ---------------
    const std::string postRequest =
        "POST /echo_env.py?submit=1 HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "User-Agent: curl/8.0\r\n"
        "Content-Type: application/x-www-form-urlencoded\r\n"
        "Content-Length: 30\r\n"
        "\r\n"
        "name=alain&lang=cpp&action=run";

    CGIResult post = runCGI(postRequest, script);
    std::cout << "=== POST /echo_env.py?submit=1 ===" << std::endl;
    std::cout << "exit status: " << post.status
              << " (clean exit: " << (exitedCleanly(post.status) ? "yes" : "no") << ")"
              << std::endl;
    std::cout << "--- CGI output ---" << std::endl;
    std::cout << post.out << std::endl;
    std::cout << "------------------" << std::endl;

    check("POST: cgi exited 0", exitedCleanly(post.status));
    check("POST: REQUEST_METHOD=POST", contains(post.out, "REQUEST_METHOD=POST"));
    check("POST: QUERY_STRING=submit=1", contains(post.out, "QUERY_STRING=submit=1"));
    check("POST: CONTENT_TYPE from request",
        contains(post.out, "CONTENT_TYPE=application/x-www-form-urlencoded"));
    check("POST: CONTENT_LENGTH=30", contains(post.out, "CONTENT_LENGTH=30"));
    check("POST: body reached stdin",
        contains(post.out, "[stdin]") && contains(post.out, "name=alain&lang=cpp&action=run"));
    check("POST: no HTTP_CONTENT_TYPE", !contains(post.out, "HTTP_CONTENT_TYPE"));
    check("POST: no HTTP_CONTENT_LENGTH", !contains(post.out, "HTTP_CONTENT_LENGTH"));

    // --------------- max_cgi_output Cap Enforcement Test ---------------
    {
        Request req = Request::fromString("GET /echo_env.py HTTP/1.1\r\nHost: localhost:8080\r\n\r\n");
        WebserverSettings settings;
        settings.max_cgi_output = 50; // Cap at 50 bytes
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        std::vector<const WebserverSettings *> set{&settings};
        Connection conn(-1, addr, set);

        CGIHandler handler(script, "/usr/bin/python3", req, conn, [](){});
        PollHandler& poll = PollHandler::getInstance();
        poll.setTimeout(50);
        for (int i = 0; i < 50 && !handler.isDone(); ++i)
            poll.checkFDs();
        poll.setTimeout(3000);

        check("max_cgi_output: detected output exceeded cap", handler.isOutputExceeded());
        check("max_cgi_output: handler is marked done", handler.isDone());
    }

    // --------------- Process Execution Deadline (SIGKILL) Test ---------------
    {
        Request req = Request::fromString("GET /echo_env.py HTTP/1.1\r\nHost: localhost:8080\r\n\r\n");
        WebserverSettings settings;
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        std::vector<const WebserverSettings *> set{&settings};
        Connection conn(-1, addr, set);

        CGIHandler handler(script, "/usr/bin/python3", req, conn, [](){});
        bool timed_out = handler.checkTimeout(0); // Trigger deadline timeout immediately
        check("timeout: checkTimeout returned true", timed_out);
        check("timeout: handler marked as timed out", handler.isTimedOut());
        check("timeout: handler is marked done", handler.isDone());
    }

    std::cout << std::endl << g_passed << " passed, " << g_failed << " failed" << std::endl;
    return g_failed == 0 ? 0 : 1;
}

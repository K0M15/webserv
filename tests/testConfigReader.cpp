#include "ConfigReader.h"
#include "WebserverSettings.hpp"
#include <iostream>

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

// --------------- single server config ---------------

static void test_single_server() {
    ConfigReader reader("tests/sample_cfg/single_url.config");
    check("single: 1 server block", reader.data.size() == 1u);

    const auto& ws = reader.data.at("");
    check("single: 1 listen entry", ws.listen.size() == 1u);
    check("single: listen port 4000", ws.listen[0].port == 4000);
    check("single: root is /", ws.root == "/");
    check("single: 1 location", ws.locations.size() == 1u);
    check("single: location / exists", ws.locations.find("/") != ws.locations.end());
    check("single: location / root", ws.locations.at("/").root.value_or("") == "/var/www/");
}

// --------------- multi-server config ---------------

static void test_multi_server() {
    ConfigReader reader("tests/sample_cfg/multi_server.config");
    // Current behavior: all server blocks with empty host key
    // are merged into a single entry (last one wins)
    check("multi: 1 entry in map", reader.data.size() == 1u);

    const auto& ws = reader.data.at("");
    // Last server block's values win
    check("multi: last server listen port 5000", ws.listen[0].port == 5000);
    check("multi: last server root /var/www/site2", ws.root == "/var/www/site2");
    check("multi: last server index index.php", ws.index == "index.php");
    check("multi: last server missing_content_type DEFAULT",
        ws.missing_content_type_policy == MissingContentTypePolicy::DEFAULT);
    check("multi: last server missing_content_type default type",
        ws.missing_content_type_default == "application/json");

    check("multi: location /upload exists",
        ws.locations.find("/upload") != ws.locations.end());
    const auto& upload = ws.locations.at("/upload");
    check("multi: location /upload upload_dir",
        upload.upload_dir == "/tmp/uploads");
    check("multi: location /upload missing_content_type overridden",
        upload.missing_content_type_policy.has_value());
    check("multi: location /upload missing_content_type DEFAULT",
        upload.missing_content_type_policy.value() == MissingContentTypePolicy::DEFAULT);
    check("multi: location /upload default type octet-stream",
        upload.missing_content_type_default.value_or("") == "application/octet-stream");
    check("multi: location / exists",
        ws.locations.find("/") != ws.locations.end());
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    std::cout << "--- ConfigReader Tests ---\n" << std::endl;

    std::cout << "Single Server:" << std::endl;
    test_single_server();
    std::cout << std::endl;

    std::cout << "Multi Server:" << std::endl;
    test_multi_server();

    std::cout << "\n----------------------------------------" << std::endl;
    std::cout << "Results: " << g_passed << " passed, " << g_failed << " failed, "
              << (g_passed + g_failed) << " total" << std::endl;
    return g_failed > 0 ? 1 : 0;
}

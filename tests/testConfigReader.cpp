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
    check("single: location / root", ws.locations.at("/").root == "/var/www/");
}

// --------------- multi-server config ---------------

static void test_multi_server() {
    ConfigReader reader("tests/sample_cfg/multi_server.config");
    // Each server block is keyed by its server_name.
    check("multi: 2 server blocks", reader.data.size() == 2u);
    check("multi: example.com present", reader.data.find("example.com") != reader.data.end());
    check("multi: api.example.com present", reader.data.find("api.example.com") != reader.data.end());

    const auto& first = reader.data.at("example.com");
    check("multi: example.com listen port 4000", first.listen[0].port == 4000);
    check("multi: example.com root /var/www/site1", first.root == "/var/www/site1");
    check("multi: example.com index index.html", first.index == "index.html");
    check("multi: example.com missing_content_type REJECT",
        first.missing_content_type_policy == MissingContentTypePolicy::REJECT);
    check("multi: example.com location / exists",
        first.locations.find("/") != first.locations.end());
    check("multi: example.com location / root",
        first.locations.at("/").root == "/var/www/site1");

    const auto& second = reader.data.at("api.example.com");
    check("multi: api.example.com listen port 5000", second.listen[0].port == 5000);
    check("multi: api.example.com root /var/www/site2", second.root == "/var/www/site2");
    check("multi: api.example.com index index.php", second.index == "index.php");
    check("multi: api.example.com missing_content_type DEFAULT",
        second.missing_content_type_policy == MissingContentTypePolicy::DEFAULT);
    check("multi: api.example.com missing_content_type default type",
        second.missing_content_type_default == "application/json");

    check("multi: location /upload exists",
        second.locations.find("/upload") != second.locations.end());
    const auto& upload = second.locations.at("/upload");
    check("multi: location /upload upload_dir",
        upload.upload_dir == "/tmp/uploads");
    check("multi: location /upload missing_content_type DEFAULT",
        upload.missing_content_type_policy == MissingContentTypePolicy::DEFAULT);
    check("multi: location /upload default type octet-stream",
        upload.missing_content_type_default == "application/octet-stream");
    check("multi: api.example.com location / exists",
        second.locations.find("/") != second.locations.end());
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

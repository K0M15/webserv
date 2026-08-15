#include "ConfigReader.h"
#include "WebserverSettings.hpp"
#include <iostream>
#include <algorithm>

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

static bool hasName(const WebserverSettings& ws, const std::string& name) {
    return std::find(ws.server_name.begin(), ws.server_name.end(), name)
           != ws.server_name.end();
}

// --------------- single server config ---------------

static void test_single_server() {
    ConfigReader reader("tests/sample_cfg/single_url.config");
    check("single: 1 server block", reader.blocks.size() == 1u);

    const WebserverSettings* ws = reader.blocks[0].get();
    check("single: 1 listen entry", ws->listen.size() == 1u);
    check("single: listen port 4000", ws->listen[0].port == 4000);
    check("single: root is /", ws->root == "/");
    check("single: 1 location", ws->locations.size() == 1u);
    check("single: location / exists", ws->locations.find("/") != ws->locations.end());
    check("single: location / root", ws->locations.at("/").root == "/var/www/");
}

// --------------- multi-server config ---------------

static void test_multi_server() {
    ConfigReader reader("tests/sample_cfg/multi_server.config");
    check("multi: 2 server blocks kept", reader.blocks.size() == 2u);

    const WebserverSettings* site1 = reader.blocks[0].get();
    check("multi: block 1 listen port 4000", site1->listen[0].port == 4000);
    check("multi: block 1 root /var/www/site1", site1->root == "/var/www/site1");
    check("multi: block 1 index index.html", site1->index == "index.html");
    check("multi: block 1 server_name example.com", hasName(*site1, "example.com"));
    check("multi: block 1 missing_content_type REJECT",
        site1->missing_content_type_policy == MissingContentTypePolicy::REJECT);
    check("multi: block 1 location / root",
        site1->locations.at("/").root == "/var/www/site1");

    const WebserverSettings* site2 = reader.blocks[1].get();
    check("multi: block 2 listen port 5000", site2->listen[0].port == 5000);
    check("multi: block 2 root /var/www/site2", site2->root == "/var/www/site2");
    check("multi: block 2 index index.php", site2->index == "index.php");
    check("multi: block 2 server_name api.example.com", hasName(*site2, "api.example.com"));
    check("multi: block 2 missing_content_type DEFAULT",
        site2->missing_content_type_policy == MissingContentTypePolicy::DEFAULT);
    check("multi: block 2 missing_content_type default type",
        site2->missing_content_type_default == "application/json");

    check("multi: block 2 location /upload exists",
        site2->locations.find("/upload") != site2->locations.end());
    const auto& upload = site2->locations.at("/upload");
    check("multi: location /upload upload_dir",
        upload.upload_dir == "/tmp/uploads");
    check("multi: location /upload missing_content_type DEFAULT",
        upload.missing_content_type_policy == MissingContentTypePolicy::DEFAULT);
    check("multi: location /upload default type octet-stream",
        upload.missing_content_type_default == "application/octet-stream");
    check("multi: block 2 location / root",
        site2->locations.at("/").root == "/var/www/site2");

    // vhosts index maps every server_name to its owning block
    check("multi: vhosts example.com -> block 1",
        reader.vhosts.at("example.com") == site1);
    check("multi: vhosts api.example.com -> block 2",
        reader.vhosts.at("api.example.com") == site2);
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

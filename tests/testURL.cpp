#include "../src/URL.hpp"
#include <iostream>
#include <vector>
#include <string>

void testURL(const std::string& urlStr, bool expectedValid) {
    try {
        URL url(urlStr);
        if (expectedValid) {
            std::cout << "[SUCCESS] Valid URL accepted: " << urlStr << std::endl;
        } else {
            std::cout << "[FAILURE] Invalid URL accepted: " << urlStr << std::endl;
        }
    } catch (const URL::NonValidURLException& e) {
        if (!expectedValid) {
            std::cout << "[SUCCESS] Invalid URL correctly rejected: " << urlStr << " (" << e.what() << ")" << std::endl;
        } else {
            std::cout << "[FAILURE] Valid URL rejected: " << urlStr << " (" << e.what() << ")" << std::endl;
        }
    }
}

int main() {
    std::cout << "--- URL Validation Tests ---" << std::endl;

    std::vector<std::string> validURLs = {
        "http://google.com",
        "https://www.google.com",
        "ftp://example.com/file.txt",
        "http://localhost:8080",
        "https://127.0.0.1/index.html",
        "http://a.b",
        "/",
        "/index.html",
        "/path/to/resource?query=1",
        "*"
    };

    std::vector<std::string> invalidURLs = {
        "htt://google.com",
        "http:/google.com",
        "http:// google.com",
        "google.com",
        "http://",
        "http://.com",
        "http:///path",
        "https://???"
    };

    std::cout << "Testing Valid URLs:" << std::endl;
    for (size_t i = 0; i < validURLs.size(); ++i) {
        testURL(validURLs[i], true);
    }

    std::cout << "\nTesting Invalid URLs:" << std::endl;
    for (size_t i = 0; i < invalidURLs.size(); ++i) {
        testURL(invalidURLs[i], false);
    }

    return 0;
}
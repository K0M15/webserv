#pragma once
#include <iostream>
#include <string>
#include <unordered_map>
#include <shared_mutex>
#include <optional>
#include <mutex>

template <typename K, typename V>
class InMemoryDB {
private:
    std::unordered_map<K, V> table;
    mutable std::shared_mutex rw_lock;
public:
    // Insert or update a key-value pair (Write Lock)
    void set(const K& key, const V& value) {
        std::unique_lock<std::shared_mutex> lock(rw_lock);
        table[key] = value;
    }
    // Retrieve a value by key (Read Lock)
    std::optional<V> get(const K& key) const {
        std::shared_lock<std::shared_mutex> lock(rw_lock);
        auto it = table.find(key);
        if (it != table.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    // Delete a key-value pair (Write Lock)
    bool del(const K& key) {
        std::unique_lock<std::shared_mutex> lock(rw_lock);
        return table.erase(key) > 0;
    }
    // Check key existence
    bool exists(const K& key) const {
        std::shared_lock<std::shared_mutex> lock(rw_lock);
        return table.find(key) != table.end();
    }
};
// int main() {
//     InMemoryDB<std::string, std::string> db;
//     db.set("user:101", "Alice");
//     db.set("user:102", "Bob");
//     if (auto val = db.get("user:101"); val) {
//         std::cout << "Found: " << *val << "\n";
//     }
//     db.del("user:101");
//     if (!db.exists("user:101")) {
//         std::cout << "user:101 successfully removed.\n";
//     }
//     return 0;
// }
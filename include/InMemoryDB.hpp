#pragma once
#include <iostream>
#include <string>
#include <unordered_map>
#include <shared_mutex>
#include <optional>
#include <mutex>
#include <vector>

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
    // check which keys we already have
    std::vector<K> keys() const
    {
        std::shared_lock<std::shared_mutex> lock(rw_lock);
        std::vector<K> result;
        result.reserve(table.size());

        for (auto it = table.begin(); it != table.end(); ++it)
            result.push_back(it->first);

        return(result);
    }
};

#include "LruEviction.hpp"
#include <stdexcept>

namespace cache {
namespace storage {

LruList::Node LruList::insert(const std::string& key) {
    list_.push_front(key);
    return list_.begin();
}

void LruList::touch(Node node) {
    list_.splice(list_.begin(), list_, node);
}

void LruList::remove(Node node) {
    list_.erase(node);
}

const std::string& LruList::lru_key() const {
    if (list_.empty()) throw std::runtime_error("LRU list is empty");
    return list_.back();
}

std::string LruList::evict_lru() {
    if (list_.empty()) throw std::runtime_error("LRU list is empty");
    std::string key = std::move(list_.back());
    list_.pop_back();
    return key;
}

} // namespace storage
} // namespace cache

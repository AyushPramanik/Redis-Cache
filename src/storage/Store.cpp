#include "Store.hpp"
#include "util/TimeUtils.hpp"
#include <spdlog/spdlog.h>
#include <charconv>
#include <stdexcept>
#include <fnmatch.h>

namespace cache {
namespace storage {

Store::Store(size_t max_keys) : max_keys_(max_keys) {
    // Pre-reserve shard buckets to avoid rehash during initial load
    for (auto& shard : shards_) {
        shard.map.reserve(max_keys / SHARD_COUNT + 64);
    }
}

Store::Shard& Store::shard_for(const std::string& key) noexcept {
    size_t idx = std::hash<std::string>{}(key) % SHARD_COUNT;
    return shards_[idx];
}

const Store::Shard& Store::shard_for(const std::string& key) const noexcept {
    size_t idx = std::hash<std::string>{}(key) % SHARD_COUNT;
    return shards_[idx];
}

void Store::set_locked(Shard& shard, const std::string& key, std::string value, int64_t expires_at) {
    auto it = shard.map.find(key);
    if (it != shard.map.end()) {
        // Update in place: reuse the existing LRU node
        it->second.value       = std::move(value);
        it->second.expires_at_ms = expires_at;
        auto* node = static_cast<LruList::Node*>(it->second.lru_node);
        shard.lru.touch(*node);
    } else {
        Entry e;
        e.value        = std::move(value);
        e.expires_at_ms = expires_at;

        auto [map_it, _] = shard.map.emplace(key, std::move(e));
        auto lru_node = shard.lru.insert(key);

        // Store the iterator on the heap so Entry can hold a type-erased pointer.
        auto* node_ptr = new LruList::Node(lru_node);
        map_it->second.lru_node = node_ptr;
    }
}

void Store::set(const std::string& key, std::string value, int64_t ttl_ms) {
    maybe_evict();

    int64_t expires_at = (ttl_ms > 0)
        ? util::now_ms() + ttl_ms
        : -1;

    auto& shard = shard_for(key);
    std::unique_lock lock(shard.mutex);
    set_locked(shard, key, std::move(value), expires_at);
}

std::optional<std::string> Store::get(const std::string& key) {
    auto& shard = shard_for(key);

    // Optimistic shared lock for the read path
    {
        std::shared_lock lock(shard.mutex);
        auto it = shard.map.find(key);
        if (it == shard.map.end()) return std::nullopt;
        if (it->second.is_expired(util::now_ms())) {
            // Lazy deletion — fall through to exclusive path
        } else {
            // Touch LRU under shared lock isn't safe — upgrade needed
            // We do the touch under an exclusive lock below
            return it->second.value;
        }
    }

    // Upgrade to exclusive to delete or touch
    std::unique_lock lock(shard.mutex);
    auto it = shard.map.find(key);
    if (it == shard.map.end()) return std::nullopt;

    if (it->second.is_expired(util::now_ms())) {
        auto* node = static_cast<LruList::Node*>(it->second.lru_node);
        shard.lru.remove(*node);
        delete node;
        shard.map.erase(it);
        return std::nullopt;
    }

    // Touch LRU
    auto* node = static_cast<LruList::Node*>(it->second.lru_node);
    shard.lru.touch(*node);
    return it->second.value;
}

bool Store::del(const std::string& key) {
    auto& shard = shard_for(key);
    std::unique_lock lock(shard.mutex);
    auto it = shard.map.find(key);
    if (it == shard.map.end()) return false;

    auto* node = static_cast<LruList::Node*>(it->second.lru_node);
    shard.lru.remove(*node);
    delete node;
    shard.map.erase(it);
    return true;
}

bool Store::exists(const std::string& key) {
    auto& shard = shard_for(key);
    std::shared_lock lock(shard.mutex);
    auto it = shard.map.find(key);
    if (it == shard.map.end()) return false;
    return !it->second.is_expired(util::now_ms());
}

int64_t Store::ttl(const std::string& key) {
    auto& shard = shard_for(key);
    std::shared_lock lock(shard.mutex);
    auto it = shard.map.find(key);
    if (it == shard.map.end()) return -2;
    if (!it->second.has_expiry())  return -1;

    int64_t remaining = it->second.expires_at_ms - util::now_ms();
    return remaining > 0 ? remaining : -2;
}

bool Store::expire(const std::string& key, int64_t ttl_ms) {
    auto& shard = shard_for(key);
    std::unique_lock lock(shard.mutex);
    auto it = shard.map.find(key);
    if (it == shard.map.end()) return false;
    if (it->second.is_expired(util::now_ms())) return false;

    it->second.expires_at_ms = util::now_ms() + ttl_ms;
    return true;
}

std::optional<int64_t> Store::incr(const std::string& key) {
    auto& shard = shard_for(key);
    std::unique_lock lock(shard.mutex);

    auto it = shard.map.find(key);
    int64_t current = 0;

    if (it != shard.map.end()) {
        if (it->second.is_expired(util::now_ms())) {
            auto* node = static_cast<LruList::Node*>(it->second.lru_node);
            shard.lru.remove(*node);
            delete node;
            shard.map.erase(it);
        } else {
            auto [ptr, ec] = std::from_chars(
                it->second.value.data(),
                it->second.value.data() + it->second.value.size(),
                current);
            if (ec != std::errc{}) return std::nullopt; // not an integer
        }
    }

    ++current;
    std::string new_val = std::to_string(current);
    set_locked(shard, key, std::move(new_val), -1);
    return current;
}

std::vector<std::string> Store::keys(const std::string& pattern) {
    std::vector<std::string> result;
    int64_t now = util::now_ms();
    for (auto& shard : shards_) {
        std::shared_lock lock(shard.mutex);
        for (const auto& [k, v] : shard.map) {
            if (v.is_expired(now)) continue;
            if (pattern == "*" || fnmatch(pattern.c_str(), k.c_str(), 0) == 0) {
                result.push_back(k);
            }
        }
    }
    return result;
}

void Store::flush() {
    for (auto& shard : shards_) {
        std::unique_lock lock(shard.mutex);
        // Free all LRU node allocations
        for (auto& [k, e] : shard.map) {
            if (e.lru_node) {
                delete static_cast<LruList::Node*>(e.lru_node);
            }
        }
        shard.map.clear();
        // Rebuild empty LRU (list cleared implicitly — but nodes are gone)
        shard.lru = LruList{};
    }
}

void Store::sweep_expired() {
    int64_t now = util::now_ms();
    size_t removed = 0;
    for (auto& shard : shards_) {
        std::unique_lock lock(shard.mutex);
        for (auto it = shard.map.begin(); it != shard.map.end(); ) {
            if (it->second.is_expired(now)) {
                auto* node = static_cast<LruList::Node*>(it->second.lru_node);
                shard.lru.remove(*node);
                delete node;
                it = shard.map.erase(it);
                ++removed;
            } else {
                ++it;
            }
        }
    }
    if (removed > 0) {
        spdlog::debug("sweep_expired: removed {} expired keys", removed);
    }
}

void Store::for_each(const SnapshotCallback& cb) {
    int64_t now = util::now_ms();
    for (auto& shard : shards_) {
        std::shared_lock lock(shard.mutex);
        for (const auto& [k, v] : shard.map) {
            if (!v.is_expired(now)) {
                cb(k, v.value, v.expires_at_ms);
            }
        }
    }
}

size_t Store::size() const {
    size_t total = 0;
    for (const auto& shard : shards_) {
        std::shared_lock lock(shard.mutex);
        total += shard.map.size();
    }
    return total;
}

void Store::maybe_evict() {
    // Fast path: avoid the scan if we're comfortably under the limit.
    // Approximate total without locking every shard.
    size_t approx = size();
    if (approx < max_keys_) return;

    // Find the shard with the most entries and evict its LRU key.
    size_t max_sz = 0;
    size_t max_idx = 0;
    for (size_t i = 0; i < SHARD_COUNT; ++i) {
        std::shared_lock lock(shards_[i].mutex);
        if (shards_[i].map.size() > max_sz) {
            max_sz  = shards_[i].map.size();
            max_idx = i;
        }
    }

    auto& shard = shards_[max_idx];
    std::unique_lock lock(shard.mutex);
    if (shard.lru.empty()) return;

    // Copy the key before evict_lru() invalidates the list node
    std::string evict_key = shard.lru.evict_lru();
    auto it = shard.map.find(evict_key);
    if (it != shard.map.end()) {
        auto* node = static_cast<LruList::Node*>(it->second.lru_node);
        delete node;
        shard.map.erase(it);
    }

    spdlog::debug("evicted LRU key '{}' from shard {}", evict_key, max_idx);
}

} // namespace storage
} // namespace cache

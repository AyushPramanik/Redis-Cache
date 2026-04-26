#pragma once

#include "Entry.hpp"
#include "LruEviction.hpp"
#include <unordered_map>
#include <shared_mutex>
#include <optional>
#include <vector>
#include <functional>
#include <cstddef>
#include <cstdint>
#include <string>

namespace cache {
namespace storage {

// Sharded, thread-safe key-value store with LRU eviction and lazy TTL expiry.
//
// Sharding: SHARD_COUNT independent hash-map+mutex pairs reduce contention.
// Each shard owns its LRU list so eviction operates entirely within one lock.
//
// Eviction: when the total key count exceeds max_keys_, the shard with the
// most entries evicts its LRU entry. This is a coarse heuristic that avoids
// a cross-shard scan on every write.
class Store {
public:
    static constexpr size_t SHARD_COUNT = 16;

    explicit Store(size_t max_keys = 1'000'000);

    // --- Core operations ---

    void   set(const std::string& key, std::string value, int64_t ttl_ms = -1);
    std::optional<std::string> get(const std::string& key);
    bool   del(const std::string& key);
    bool   exists(const std::string& key);

    // Returns remaining TTL in milliseconds, -1 if no expiry, -2 if key missing.
    int64_t ttl(const std::string& key);

    // Sets or updates expiry. Returns false if key does not exist.
    bool expire(const std::string& key, int64_t ttl_ms);

    // Atomic increment (integer semantics). Returns new value or error string.
    // On type error returns std::nullopt.
    std::optional<int64_t> incr(const std::string& key);

    // Returns all keys matching a glob pattern (O(n) — for DEBUG use only).
    std::vector<std::string> keys(const std::string& pattern = "*");

    // Remove all keys from all shards.
    void flush();

    // Expiry background sweep — call periodically from a maintenance thread.
    void sweep_expired();

    // For persistence: iterate all entries in snapshot-safe order.
    // Callback receives (key, value, expires_at_ms).
    using SnapshotCallback = std::function<void(const std::string&, const std::string&, int64_t)>;
    void for_each(const SnapshotCallback& cb);

    [[nodiscard]] size_t size() const;

private:
    struct Shard {
        mutable std::shared_mutex                     mutex;
        std::unordered_map<std::string, Entry>        map;
        LruList                                       lru;
    };

    Shard& shard_for(const std::string& key) noexcept;
    const Shard& shard_for(const std::string& key) const noexcept;

    // Evicts one LRU entry from the shard with the most entries.
    void maybe_evict();

    // Internal set — shard lock must be held exclusively by caller.
    void set_locked(Shard& shard, const std::string& key, std::string value, int64_t expires_at);

    std::array<Shard, SHARD_COUNT> shards_;
    size_t max_keys_;
};

} // namespace storage
} // namespace cache

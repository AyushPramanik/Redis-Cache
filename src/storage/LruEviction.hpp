#pragma once

#include <list>
#include <string>
#include <cstddef>

namespace cache {
namespace storage {

// Doubly-linked list that tracks access order for LRU eviction.
// The store holds a pointer to each entry's list node so O(1) touch/remove
// is possible without scanning.
//
// Thread safety: callers must hold the shard lock.
class LruList {
public:
    using Node = std::list<std::string>::iterator;

    // Returns iterator to the newly inserted front node.
    Node insert(const std::string& key);

    // Move an existing node to the front (most-recently-used).
    void touch(Node node);

    // Remove an arbitrary node (called on deletion or eviction).
    void remove(Node node);

    // Return the least-recently-used key (back of list) without removing it.
    [[nodiscard]] const std::string& lru_key() const;

    // Pop and return the LRU key (for eviction).
    std::string evict_lru();

    [[nodiscard]] bool empty() const noexcept { return list_.empty(); }
    [[nodiscard]] size_t size() const noexcept { return list_.size(); }

private:
    std::list<std::string> list_; // front = MRU, back = LRU
};

} // namespace storage
} // namespace cache

#pragma once

#include <string>
#include <cstdint>
#include <list>

namespace cache {
namespace storage {

// A cached value with expiry metadata and an LRU list handle.
// Keeping the iterator inline avoids a second hash lookup during eviction.
struct Entry {
    std::string value;
    int64_t     expires_at_ms = -1; // -1 means no expiry

    // Pointer into the LRU key list — populated by the eviction layer.
    // Declared as void* to avoid circular inclusion; cast site knows the real type.
    void* lru_node = nullptr;

    [[nodiscard]] bool has_expiry() const noexcept { return expires_at_ms >= 0; }
    [[nodiscard]] bool is_expired(int64_t now_ms) const noexcept {
        return has_expiry() && now_ms >= expires_at_ms;
    }
};

} // namespace storage
} // namespace cache

#pragma once

#include "storage/Store.hpp"
#include <string>

namespace cache {
namespace persistence {

// RDB-style binary snapshot.
//
// File format:
//   MAGIC "CACHDB" (6 bytes)
//   VERSION u8 (1 byte, currently 0x01)
//   For each key:
//     KEY_LEN   u32 (4 bytes, big-endian)
//     KEY       bytes
//     VAL_LEN   u32
//     VAL       bytes
//     EXPIRE_MS i64 (-1 means no expiry)
//   EOF_MARKER  0xFF 0xFF 0xFF 0xFF
//
// The format is intentionally simple to keep the implementation readable.
// A production system would add CRC-32, compression, and versioned sections.
class Rdb {
public:
    explicit Rdb(const std::string& path);

    // Write a point-in-time snapshot. Atomic: writes to .tmp then renames.
    bool save(storage::Store& store);

    // Load snapshot into store. Returns number of keys loaded.
    size_t load(storage::Store& store);

private:
    std::string path_;
};

} // namespace persistence
} // namespace cache

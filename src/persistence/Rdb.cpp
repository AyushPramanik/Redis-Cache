#include "Rdb.hpp"
#include <spdlog/spdlog.h>
#include <fstream>
#include <cstring>
#include <cstdint>
#include <stdexcept>

namespace cache {
namespace persistence {

static constexpr char MAGIC[]   = "CACHDB";
static constexpr uint8_t VERSION = 0x01;
static constexpr uint32_t EOF_MARKER = 0xFFFFFFFF;

static void write_u32(std::ofstream& f, uint32_t v) {
    // Big-endian
    uint8_t b[4] = {
        static_cast<uint8_t>(v >> 24),
        static_cast<uint8_t>(v >> 16),
        static_cast<uint8_t>(v >>  8),
        static_cast<uint8_t>(v)
    };
    f.write(reinterpret_cast<char*>(b), 4);
}

static void write_i64(std::ofstream& f, int64_t v) {
    uint8_t b[8];
    uint64_t u = static_cast<uint64_t>(v);
    for (int i = 7; i >= 0; --i) {
        b[i] = static_cast<uint8_t>(u & 0xFF);
        u >>= 8;
    }
    f.write(reinterpret_cast<char*>(b), 8);
}

static uint32_t read_u32(std::ifstream& f) {
    uint8_t b[4];
    f.read(reinterpret_cast<char*>(b), 4);
    return (static_cast<uint32_t>(b[0]) << 24) |
           (static_cast<uint32_t>(b[1]) << 16) |
           (static_cast<uint32_t>(b[2]) <<  8) |
            static_cast<uint32_t>(b[3]);
}

static int64_t read_i64(std::ifstream& f) {
    uint8_t b[8];
    f.read(reinterpret_cast<char*>(b), 8);
    uint64_t u = 0;
    for (int i = 0; i < 8; ++i) {
        u = (u << 8) | b[i];
    }
    return static_cast<int64_t>(u);
}

Rdb::Rdb(const std::string& path) : path_(path) {}

bool Rdb::save(storage::Store& store) {
    std::string tmp_path = path_ + ".tmp";
    std::ofstream f(tmp_path, std::ios::binary | std::ios::trunc);
    if (!f) {
        spdlog::error("RDB: cannot open '{}' for writing", tmp_path);
        return false;
    }

    f.write(MAGIC, 6);
    f.put(static_cast<char>(VERSION));

    size_t count = 0;
    store.for_each([&](const std::string& key, const std::string& val, int64_t exp) {
        write_u32(f, static_cast<uint32_t>(key.size()));
        f.write(key.data(), static_cast<std::streamsize>(key.size()));
        write_u32(f, static_cast<uint32_t>(val.size()));
        f.write(val.data(), static_cast<std::streamsize>(val.size()));
        write_i64(f, exp);
        ++count;
    });

    write_u32(f, EOF_MARKER);
    f.close();

    if (!f) {
        spdlog::error("RDB: write error for '{}'", tmp_path);
        return false;
    }

    // Atomic rename
    if (std::rename(tmp_path.c_str(), path_.c_str()) != 0) {
        spdlog::error("RDB: rename failed for '{}'", tmp_path);
        return false;
    }

    spdlog::info("RDB: saved {} keys to '{}'", count, path_);
    return true;
}

size_t Rdb::load(storage::Store& store) {
    std::ifstream f(path_, std::ios::binary);
    if (!f) {
        spdlog::debug("RDB: no snapshot found at '{}'", path_);
        return 0;
    }

    char magic[6];
    f.read(magic, 6);
    if (std::memcmp(magic, MAGIC, 6) != 0) {
        spdlog::error("RDB: bad magic in '{}'", path_);
        return 0;
    }

    uint8_t ver = static_cast<uint8_t>(f.get());
    if (ver != VERSION) {
        spdlog::warn("RDB: version mismatch (got {}, expected {})", ver, VERSION);
    }

    size_t count = 0;
    int64_t now_ms_val = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    while (f) {
        uint32_t klen = read_u32(f);
        if (klen == EOF_MARKER) break;

        std::string key(klen, '\0');
        f.read(key.data(), klen);

        uint32_t vlen = read_u32(f);
        std::string val(vlen, '\0');
        f.read(val.data(), vlen);

        int64_t exp = read_i64(f);
        if (exp > 0 && exp <= now_ms_val) continue; // expired at rest

        int64_t ttl_ms = (exp > 0) ? exp - now_ms_val : -1;
        store.set(key, val, ttl_ms);
        ++count;
    }

    spdlog::info("RDB: loaded {} keys from '{}'", count, path_);
    return count;
}

} // namespace persistence
} // namespace cache

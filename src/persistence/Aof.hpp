#pragma once

#include "storage/Store.hpp"
#include "protocol/RespTypes.hpp"
#include <string>
#include <fstream>
#include <mutex>
#include <atomic>

namespace cache {
namespace persistence {

// Append-Only File persistence.
//
// Every write command is appended in RESP format so the log can be replayed
// on startup by re-parsing and re-executing each entry.
//
// Sync policy: we call fsync after every write for maximum durability.
// A production deployment might batch-sync every second (like Redis's "everysec")
// but that adds complexity without improving the demo value.
class Aof {
public:
    explicit Aof(const std::string& path);
    ~Aof();

    // Open the AOF file for appending.
    bool open();

    // Append a RESP command array to the log.
    // Thread-safe: multiple connections may call this concurrently.
    bool append(const protocol::RespValue& cmd);

    // Replay the AOF into the store. Returns number of commands replayed.
    size_t replay(storage::Store& store,
                  const std::function<void(const protocol::RespValue&, storage::Store&)>& executor);

    void close();

    [[nodiscard]] bool is_open() const noexcept { return file_.is_open(); }

private:
    std::string   path_;
    std::ofstream file_;
    std::mutex    mutex_;
};

} // namespace persistence
} // namespace cache

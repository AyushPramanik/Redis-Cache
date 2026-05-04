#pragma once

#include <string>
#include <cstdint>
#include <cstddef>

namespace cache {
namespace server {

struct Config {
    // Network
    std::string host        = "0.0.0.0";
    uint16_t    port        = 6399;
    int         backlog     = 1024;

    // Threading
    size_t      worker_threads = 4;

    // Storage
    size_t      max_keys    = 1'000'000;

    // Persistence
    bool        rdb_enabled = true;
    std::string rdb_path    = "dump.rdb";
    int         rdb_interval_secs = 300; // snapshot every 5 min

    bool        aof_enabled = false;
    std::string aof_path    = "appendonly.aof";

    // Metrics HTTP server
    bool        metrics_enabled = true;
    uint16_t    metrics_port    = 9999;

    // Logging
    std::string log_level = "info";
    std::string log_file  = "";   // empty = console only

    // Expiry sweep
    int         sweep_interval_ms = 1000;

    // Load from an INI-style config file. Silently ignores unknown keys.
    static Config from_file(const std::string& path);

    // Override individual fields from argc/argv (--key value pairs).
    void apply_args(int argc, char** argv);
};

} // namespace server
} // namespace cache

#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <array>
#include <chrono>

namespace cache {
namespace metrics {

// Lock-free metrics counters.
// Uses relaxed atomics for counters (approximation is fine for monitoring),
// and acquire/release for values that must be consistent snapshots.
class Metrics {
public:
    Metrics() = default;

    void record_command(int64_t latency_us);
    void record_cache_hit();
    void record_cache_miss();
    void record_connection_open();
    void record_connection_close();
    void set_memory_bytes(int64_t bytes);
    void set_key_count(int64_t n);

    // Snapshot for reporting — all reads are relaxed (best-effort)
    struct Snapshot {
        int64_t total_commands;
        int64_t total_hits;
        int64_t total_misses;
        int64_t active_connections;
        int64_t total_connections;
        int64_t memory_bytes;
        int64_t key_count;
        double  avg_latency_us;
        double  hit_rate;
        int64_t commands_per_sec;
    };

    Snapshot snapshot() const;

    // Prometheus-format text for the /metrics endpoint
    std::string prometheus_text() const;

private:
    // Exponentially-weighted moving average of latency (μs)
    void update_latency_ema(double sample_us);

    std::atomic<int64_t> total_commands_{0};
    std::atomic<int64_t> total_hits_{0};
    std::atomic<int64_t> total_misses_{0};
    std::atomic<int64_t> active_connections_{0};
    std::atomic<int64_t> total_connections_{0};
    std::atomic<int64_t> memory_bytes_{0};
    std::atomic<int64_t> key_count_{0};

    // EMA of latency stored as integer μs * 100 for precision without floats
    std::atomic<int64_t> latency_ema_x100_{0};

    // Commands per second — updated once per second by the server's maintenance thread
    std::atomic<int64_t> commands_per_sec_{0};
    mutable std::atomic<int64_t> last_cmd_snapshot_{0};
    mutable std::atomic<int64_t> last_snapshot_time_ms_{0};
};

} // namespace metrics
} // namespace cache

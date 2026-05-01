#include "Metrics.hpp"
#include "util/TimeUtils.hpp"
#include <fmt/format.h>
#include <cmath>

namespace cache {
namespace metrics {

void Metrics::record_command(int64_t latency_us) {
    total_commands_.fetch_add(1, std::memory_order_relaxed);
    update_latency_ema(static_cast<double>(latency_us));
}

void Metrics::record_cache_hit()  { total_hits_.fetch_add(1,   std::memory_order_relaxed); }
void Metrics::record_cache_miss() { total_misses_.fetch_add(1, std::memory_order_relaxed); }

void Metrics::record_connection_open() {
    active_connections_.fetch_add(1, std::memory_order_relaxed);
    total_connections_.fetch_add(1, std::memory_order_relaxed);
}

void Metrics::record_connection_close() {
    active_connections_.fetch_sub(1, std::memory_order_relaxed);
}

void Metrics::set_memory_bytes(int64_t bytes) {
    memory_bytes_.store(bytes, std::memory_order_relaxed);
}

void Metrics::set_key_count(int64_t n) {
    key_count_.store(n, std::memory_order_relaxed);
}

void Metrics::update_latency_ema(double sample_us) {
    // alpha = 0.05 — slow-moving EMA for stability under load
    static constexpr double alpha = 0.05;
    int64_t old = latency_ema_x100_.load(std::memory_order_relaxed);
    double  ema = static_cast<double>(old) / 100.0;
    ema = alpha * sample_us + (1.0 - alpha) * ema;
    latency_ema_x100_.store(static_cast<int64_t>(ema * 100.0), std::memory_order_relaxed);
}

Metrics::Snapshot Metrics::snapshot() const {
    Snapshot s;
    s.total_commands    = total_commands_.load(std::memory_order_relaxed);
    s.total_hits        = total_hits_.load(std::memory_order_relaxed);
    s.total_misses      = total_misses_.load(std::memory_order_relaxed);
    s.active_connections = active_connections_.load(std::memory_order_relaxed);
    s.total_connections = total_connections_.load(std::memory_order_relaxed);
    s.memory_bytes      = memory_bytes_.load(std::memory_order_relaxed);
    s.key_count         = key_count_.load(std::memory_order_relaxed);
    s.avg_latency_us    = static_cast<double>(latency_ema_x100_.load(std::memory_order_relaxed)) / 100.0;

    int64_t total = s.total_hits + s.total_misses;
    s.hit_rate = (total > 0) ? (static_cast<double>(s.total_hits) / static_cast<double>(total)) : 0.0;

    // Commands-per-second: computed from delta since last snapshot
    int64_t now_ms_val = util::now_ms();
    int64_t prev_time  = last_snapshot_time_ms_.load(std::memory_order_relaxed);
    int64_t prev_cmds  = last_cmd_snapshot_.load(std::memory_order_relaxed);
    int64_t dt_ms      = now_ms_val - prev_time;
    if (dt_ms > 100) {
        int64_t delta_cmds = s.total_commands - prev_cmds;
        s.commands_per_sec = (dt_ms > 0)
            ? (delta_cmds * 1000 / dt_ms)
            : 0;
        last_snapshot_time_ms_.store(now_ms_val, std::memory_order_relaxed);
        last_cmd_snapshot_.store(s.total_commands, std::memory_order_relaxed);
    } else {
        s.commands_per_sec = commands_per_sec_.load(std::memory_order_relaxed);
    }

    return s;
}

std::string Metrics::prometheus_text() const {
    auto s = snapshot();
    return fmt::format(
        "# HELP cache_commands_total Total commands executed\n"
        "# TYPE cache_commands_total counter\n"
        "cache_commands_total {}\n"
        "# HELP cache_hits_total Total cache hits\n"
        "# TYPE cache_hits_total counter\n"
        "cache_hits_total {}\n"
        "# HELP cache_misses_total Total cache misses\n"
        "# TYPE cache_misses_total counter\n"
        "cache_misses_total {}\n"
        "# HELP cache_active_connections Current active connections\n"
        "# TYPE cache_active_connections gauge\n"
        "cache_active_connections {}\n"
        "# HELP cache_key_count Current number of keys\n"
        "# TYPE cache_key_count gauge\n"
        "cache_key_count {}\n"
        "# HELP cache_memory_bytes Approximate memory usage\n"
        "# TYPE cache_memory_bytes gauge\n"
        "cache_memory_bytes {}\n"
        "# HELP cache_avg_latency_us Exponentially-weighted average command latency\n"
        "# TYPE cache_avg_latency_us gauge\n"
        "cache_avg_latency_us {:.2f}\n"
        "# HELP cache_hit_rate Cache hit rate (0-1)\n"
        "# TYPE cache_hit_rate gauge\n"
        "cache_hit_rate {:.4f}\n"
        "# HELP cache_commands_per_sec Commands per second\n"
        "# TYPE cache_commands_per_sec gauge\n"
        "cache_commands_per_sec {}\n",
        s.total_commands,
        s.total_hits,
        s.total_misses,
        s.active_connections,
        s.key_count,
        s.memory_bytes,
        s.avg_latency_us,
        s.hit_rate,
        s.commands_per_sec
    );
}

} // namespace metrics
} // namespace cache

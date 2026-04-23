#pragma once

#include <chrono>
#include <cstdint>

namespace cache::util {

// Returns milliseconds since Unix epoch
inline int64_t now_ms() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

// Returns microseconds since Unix epoch — used for latency tracking
inline int64_t now_us() {
    using namespace std::chrono;
    return duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
}

} // namespace cache::util

#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/pattern_formatter.h>
#include <memory>
#include <string>

namespace cache::util {

inline void init_logger(const std::string& log_level = "info",
                        const std::string& log_file  = "") {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");

    std::vector<spdlog::sink_ptr> sinks{console_sink};

    if (!log_file.empty()) {
        // 10 MB max, 3 rotated files
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            log_file, 10 * 1024 * 1024, 3);
        file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] %v");
        sinks.push_back(std::move(file_sink));
    }

    auto logger = std::make_shared<spdlog::logger>("cache", sinks.begin(), sinks.end());
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::from_str(log_level));
    spdlog::flush_on(spdlog::level::warn);
}

} // namespace cache::util

#include "Config.hpp"
#include <spdlog/spdlog.h>
#include <fstream>
#include <sstream>
#include <string>
#include <cctype>

namespace cache {
namespace server {

static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end   = s.find_last_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    return s.substr(start, end - start + 1);
}

Config Config::from_file(const std::string& path) {
    Config cfg;
    std::ifstream f(path);
    if (!f.is_open()) {
        spdlog::warn("Config file '{}' not found, using defaults", path);
        return cfg;
    }

    std::string line;
    while (std::getline(f, line)) {
        // Strip comments
        auto hash = line.find('#');
        if (hash != std::string::npos) line = line.substr(0, hash);
        line = trim(line);
        if (line.empty()) continue;

        auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = trim(line.substr(0, eq));
        std::string val = trim(line.substr(eq + 1));

        if (key == "host")              cfg.host = val;
        else if (key == "port")         cfg.port = static_cast<uint16_t>(std::stoi(val));
        else if (key == "backlog")      cfg.backlog = std::stoi(val);
        else if (key == "worker_threads") cfg.worker_threads = static_cast<size_t>(std::stoi(val));
        else if (key == "max_keys")     cfg.max_keys = static_cast<size_t>(std::stoull(val));
        else if (key == "rdb_enabled")  cfg.rdb_enabled = (val == "true" || val == "1");
        else if (key == "rdb_path")     cfg.rdb_path = val;
        else if (key == "rdb_interval_secs") cfg.rdb_interval_secs = std::stoi(val);
        else if (key == "aof_enabled")  cfg.aof_enabled = (val == "true" || val == "1");
        else if (key == "aof_path")     cfg.aof_path = val;
        else if (key == "metrics_enabled") cfg.metrics_enabled = (val == "true" || val == "1");
        else if (key == "metrics_port") cfg.metrics_port = static_cast<uint16_t>(std::stoi(val));
        else if (key == "log_level")    cfg.log_level = val;
        else if (key == "log_file")     cfg.log_file = val;
        else if (key == "sweep_interval_ms") cfg.sweep_interval_ms = std::stoi(val);
        else {
            spdlog::warn("Unknown config key: '{}'", key);
        }
    }

    return cfg;
}

void Config::apply_args(int argc, char** argv) {
    for (int i = 1; i + 1 < argc; i += 2) {
        std::string key(argv[i]);
        std::string val(argv[i + 1]);

        if (key == "--port")    port = static_cast<uint16_t>(std::stoi(val));
        else if (key == "--host")  host = val;
        else if (key == "--workers") worker_threads = static_cast<size_t>(std::stoi(val));
        else if (key == "--log-level") log_level = val;
        else if (key == "--max-keys")  max_keys = static_cast<size_t>(std::stoull(val));
    }
}

} // namespace server
} // namespace cache

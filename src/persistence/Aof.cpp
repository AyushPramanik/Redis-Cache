#include "Aof.hpp"
#include "protocol/RespParser.hpp"
#include "protocol/RespSerializer.hpp"
#include <spdlog/spdlog.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>

namespace cache {
namespace persistence {

Aof::Aof(const std::string& path) : path_(path) {}

Aof::~Aof() {
    close();
}

bool Aof::open() {
    file_.open(path_, std::ios::app | std::ios::binary);
    if (!file_) {
        spdlog::error("AOF: cannot open '{}': {}", path_, strerror(errno));
        return false;
    }
    spdlog::info("AOF: opened '{}'", path_);
    return true;
}

bool Aof::append(const protocol::RespValue& cmd) {
    if (cmd.type != protocol::RespType::Array) return false;

    // Serialize outside the lock — serialization is pure compute with no
    // shared state, so we only hold the mutex for the actual I/O call.
    std::string wire;
    wire.reserve(64);
    wire += "*" + std::to_string(cmd.array.size()) + "\r\n";
    for (const auto& arg : cmd.array) {
        wire += protocol::RespSerializer::bulk(arg.str);
    }

    std::unique_lock lock(mutex_);
    file_.write(wire.data(), static_cast<std::streamsize>(wire.size()));
    if (!file_) {
        spdlog::error("AOF: write error");
        return false;
    }
    file_.flush();
    return true;
}

size_t Aof::replay(
    storage::Store& store,
    const std::function<void(const protocol::RespValue&, storage::Store&)>& executor)
{
    std::ifstream f(path_, std::ios::binary);
    if (!f) {
        spdlog::debug("AOF: no log found at '{}'", path_);
        return 0;
    }

    // Read entire file into memory then parse
    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());

    protocol::RespParser parser;
    std::string_view view(content);
    size_t count = 0;

    while (!view.empty()) {
        auto res = parser.parse(view);
        if (res.status == protocol::ParseStatus::Incomplete) break;
        if (res.status == protocol::ParseStatus::Error) {
            spdlog::warn("AOF: parse error at offset {}", content.size() - view.size());
            break;
        }
        executor(res.value, store);
        view.remove_prefix(res.consumed);
        ++count;
    }

    spdlog::info("AOF: replayed {} commands from '{}'", count, path_);
    return count;
}

void Aof::close() {
    if (file_.is_open()) file_.close();
}

} // namespace persistence
} // namespace cache

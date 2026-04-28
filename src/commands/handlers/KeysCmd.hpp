#pragma once

#include "commands/Command.hpp"
#include "protocol/RespSerializer.hpp"
#include <span>

namespace cache {
namespace commands {

inline std::string handle_keys(const CommandContext& ctx) {
    std::string pattern = "*";
    if (ctx.req.array.size() >= 2) {
        pattern = ctx.req.array[1].str;
    }
    auto all_keys = ctx.store.keys(pattern);
    // RespSerializer::array expects span<const string>
    std::span<const std::string> view(all_keys);
    return protocol::RespSerializer::array(view);
}

inline std::string handle_dbsize(const CommandContext& ctx) {
    return protocol::RespSerializer::integer(static_cast<int64_t>(ctx.store.size()));
}

inline std::string handle_flushall(const CommandContext& ctx) {
    ctx.store.flush();
    return protocol::RespSerializer::ok();
}

} // namespace commands
} // namespace cache

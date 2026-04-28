#pragma once

#include "commands/Command.hpp"
#include "protocol/RespSerializer.hpp"

namespace cache {
namespace commands {

inline std::string handle_ttl(const CommandContext& ctx) {
    if (ctx.req.array.size() < 2) {
        return protocol::RespSerializer::error("wrong number of arguments for 'ttl' command");
    }
    int64_t ms = ctx.store.ttl(ctx.req.array[1].str);
    if (ms == -2) return protocol::RespSerializer::integer(-2); // key not found
    if (ms == -1) return protocol::RespSerializer::integer(-1); // no expiry
    return protocol::RespSerializer::integer(ms / 1000);
}

inline std::string handle_pttl(const CommandContext& ctx) {
    if (ctx.req.array.size() < 2) {
        return protocol::RespSerializer::error("wrong number of arguments for 'pttl' command");
    }
    int64_t ms = ctx.store.ttl(ctx.req.array[1].str);
    return protocol::RespSerializer::integer(ms);
}

} // namespace commands
} // namespace cache

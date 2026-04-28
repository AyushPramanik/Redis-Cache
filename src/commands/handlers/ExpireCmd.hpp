#pragma once

#include "commands/Command.hpp"
#include "protocol/RespSerializer.hpp"
#include <charconv>

namespace cache {
namespace commands {

inline std::string handle_expire(const CommandContext& ctx) {
    if (ctx.req.array.size() < 3) {
        return protocol::RespSerializer::error("wrong number of arguments for 'expire' command");
    }
    const std::string& key = ctx.req.array[1].str;
    int64_t secs = 0;
    std::from_chars(ctx.req.array[2].str.data(),
                    ctx.req.array[2].str.data() + ctx.req.array[2].str.size(), secs);
    bool ok = ctx.store.expire(key, secs * 1000);
    return protocol::RespSerializer::integer(ok ? 1 : 0);
}

inline std::string handle_pexpire(const CommandContext& ctx) {
    if (ctx.req.array.size() < 3) {
        return protocol::RespSerializer::error("wrong number of arguments for 'pexpire' command");
    }
    const std::string& key = ctx.req.array[1].str;
    int64_t ms = 0;
    std::from_chars(ctx.req.array[2].str.data(),
                    ctx.req.array[2].str.data() + ctx.req.array[2].str.size(), ms);
    bool ok = ctx.store.expire(key, ms);
    return protocol::RespSerializer::integer(ok ? 1 : 0);
}

} // namespace commands
} // namespace cache

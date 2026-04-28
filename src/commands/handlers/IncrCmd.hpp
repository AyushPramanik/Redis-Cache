#pragma once

#include "commands/Command.hpp"
#include "protocol/RespSerializer.hpp"
#include <charconv>

namespace cache {
namespace commands {

inline std::string handle_incr(const CommandContext& ctx) {
    if (ctx.req.array.size() < 2) {
        return protocol::RespSerializer::error("wrong number of arguments for 'incr' command");
    }
    auto result = ctx.store.incr(ctx.req.array[1].str);
    if (!result) {
        return protocol::RespSerializer::error("value is not an integer or out of range");
    }
    return protocol::RespSerializer::integer(*result);
}

inline std::string handle_incrby(const CommandContext& ctx) {
    if (ctx.req.array.size() < 3) {
        return protocol::RespSerializer::error("wrong number of arguments for 'incrby' command");
    }
    const std::string& key = ctx.req.array[1].str;
    int64_t delta = 0;
    auto [ptr, ec] = std::from_chars(
        ctx.req.array[2].str.data(),
        ctx.req.array[2].str.data() + ctx.req.array[2].str.size(),
        delta);
    if (ec != std::errc{}) {
        return protocol::RespSerializer::error("value is not an integer or out of range");
    }

    // Retrieve current, add delta, store back
    auto cur_str = ctx.store.get(key);
    int64_t cur = 0;
    if (cur_str) {
        auto [p2, ec2] = std::from_chars(cur_str->data(), cur_str->data() + cur_str->size(), cur);
        if (ec2 != std::errc{}) {
            return protocol::RespSerializer::error("value is not an integer or out of range");
        }
    }
    int64_t next = cur + delta;
    ctx.store.set(key, std::to_string(next));
    return protocol::RespSerializer::integer(next);
}

} // namespace commands
} // namespace cache

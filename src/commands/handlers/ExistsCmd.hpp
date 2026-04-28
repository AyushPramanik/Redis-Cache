#pragma once

#include "commands/Command.hpp"
#include "protocol/RespSerializer.hpp"

namespace cache {
namespace commands {

inline std::string handle_exists(const CommandContext& ctx) {
    if (ctx.req.array.size() < 2) {
        return protocol::RespSerializer::error("wrong number of arguments for 'exists' command");
    }
    int64_t count = 0;
    for (size_t i = 1; i < ctx.req.array.size(); ++i) {
        if (ctx.store.exists(ctx.req.array[i].str)) ++count;
    }
    return protocol::RespSerializer::integer(count);
}

} // namespace commands
} // namespace cache

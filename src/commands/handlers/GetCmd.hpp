#pragma once

#include "commands/Command.hpp"
#include "protocol/RespSerializer.hpp"

namespace cache {
namespace commands {

inline std::string handle_get(const CommandContext& ctx) {
    if (ctx.req.array.size() < 2) {
        return protocol::RespSerializer::error("wrong number of arguments for 'get' command");
    }
    auto val = ctx.store.get(ctx.req.array[1].str);
    if (!val) return protocol::RespSerializer::null_bulk();
    return protocol::RespSerializer::bulk(*val);
}

} // namespace commands
} // namespace cache

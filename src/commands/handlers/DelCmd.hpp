#pragma once

#include "commands/Command.hpp"
#include "protocol/RespSerializer.hpp"

namespace cache {
namespace commands {

inline std::string handle_del(const CommandContext& ctx) {
    if (ctx.req.array.size() < 2) {
        return protocol::RespSerializer::error("wrong number of arguments for 'del' command");
    }
    int64_t deleted = 0;
    for (size_t i = 1; i < ctx.req.array.size(); ++i) {
        if (ctx.store.del(ctx.req.array[i].str)) ++deleted;
    }
    return protocol::RespSerializer::integer(deleted);
}

} // namespace commands
} // namespace cache

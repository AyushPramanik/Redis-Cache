#pragma once

#include "commands/Command.hpp"
#include "protocol/RespSerializer.hpp"

namespace cache {
namespace commands {

inline std::string handle_ping(const CommandContext& ctx) {
    // PING [message]
    if (ctx.req.array.size() >= 2 && ctx.req.array[1].type == protocol::RespType::BulkString) {
        return protocol::RespSerializer::bulk(ctx.req.array[1].str);
    }
    return protocol::RespSerializer::simple("PONG");
}

} // namespace commands
} // namespace cache

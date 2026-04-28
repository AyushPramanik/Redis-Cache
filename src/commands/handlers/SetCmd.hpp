#pragma once

#include "commands/Command.hpp"
#include "protocol/RespSerializer.hpp"
#include <string>
#include <cctype>
#include <charconv>

namespace cache {
namespace commands {

// SET key value [EX seconds] [PX milliseconds] [NX] [XX]
inline std::string handle_set(const CommandContext& ctx) {
    const auto& args = ctx.req.array;
    if (args.size() < 3) {
        return protocol::RespSerializer::error("wrong number of arguments for 'set' command");
    }

    const std::string& key   = args[1].str;
    const std::string& value = args[2].str;
    int64_t ttl_ms = -1;
    bool nx = false, xx = false;

    for (size_t i = 3; i < args.size(); ++i) {
        std::string opt = args[i].str;
        for (char& c : opt) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

        if (opt == "EX" && i + 1 < args.size()) {
            int64_t secs = 0;
            std::from_chars(args[++i].str.data(), args[i].str.data() + args[i].str.size(), secs);
            ttl_ms = secs * 1000;
        } else if (opt == "PX" && i + 1 < args.size()) {
            std::from_chars(args[++i].str.data(), args[i].str.data() + args[i].str.size(), ttl_ms);
        } else if (opt == "NX") {
            nx = true;
        } else if (opt == "XX") {
            xx = true;
        }
    }

    if (nx && ctx.store.exists(key)) return protocol::RespSerializer::null_bulk();
    if (xx && !ctx.store.exists(key)) return protocol::RespSerializer::null_bulk();

    ctx.store.set(key, value, ttl_ms);
    return protocol::RespSerializer::ok();
}

} // namespace commands
} // namespace cache

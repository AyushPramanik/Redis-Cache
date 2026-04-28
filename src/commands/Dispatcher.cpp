#include "Dispatcher.hpp"
#include "handlers/PingCmd.hpp"
#include "handlers/SetCmd.hpp"
#include "handlers/GetCmd.hpp"
#include "handlers/DelCmd.hpp"
#include "handlers/ExistsCmd.hpp"
#include "handlers/ExpireCmd.hpp"
#include "handlers/TtlCmd.hpp"
#include "handlers/IncrCmd.hpp"
#include "handlers/KeysCmd.hpp"
#include "protocol/RespSerializer.hpp"
#include <spdlog/spdlog.h>
#include <cctype>
#include <algorithm>

namespace cache {
namespace commands {

Dispatcher::Dispatcher() {
    register_handler("ping",     handle_ping);
    register_handler("set",      handle_set);
    register_handler("get",      handle_get);
    register_handler("del",      handle_del);
    register_handler("unlink",   handle_del); // alias
    register_handler("exists",   handle_exists);
    register_handler("expire",   handle_expire);
    register_handler("pexpire",  handle_pexpire);
    register_handler("ttl",      handle_ttl);
    register_handler("pttl",     handle_pttl);
    register_handler("incr",     handle_incr);
    register_handler("incrby",   handle_incrby);
    register_handler("keys",     handle_keys);
    register_handler("dbsize",   handle_dbsize);
    register_handler("flushall", handle_flushall);
    register_handler("flushdb",  handle_flushall);
}

void Dispatcher::register_handler(const std::string& name, CommandHandler handler) {
    handlers_.emplace(name, std::move(handler));
}

std::string Dispatcher::dispatch(const protocol::RespValue& req, storage::Store& store) const {
    if (req.type != protocol::RespType::Array || req.array.empty()) {
        return protocol::RespSerializer::error("invalid request format");
    }

    std::string cmd = req.array[0].str;
    std::transform(cmd.begin(), cmd.end(), cmd.begin(),
        [](unsigned char c) { return std::tolower(c); });

    auto it = handlers_.find(cmd);
    if (it == handlers_.end()) {
        spdlog::debug("unknown command: {}", cmd);
        return protocol::RespSerializer::error("unknown command '" + cmd + "'");
    }

    CommandContext ctx{store, req};
    return it->second(ctx);
}

} // namespace commands
} // namespace cache

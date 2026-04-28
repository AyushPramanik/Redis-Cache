#pragma once

#include "protocol/RespTypes.hpp"
#include "storage/Store.hpp"
#include <string>
#include <functional>

namespace cache {
namespace commands {

// Context passed to every command handler.
// Keeps the handler interface thin — add fields here rather than growing handler signatures.
struct CommandContext {
    storage::Store&             store;
    const protocol::RespValue&  req;   // the parsed RESP array from the client
};

// A command handler writes its response into the returned string (RESP wire format).
using CommandHandler = std::function<std::string(const CommandContext&)>;

} // namespace commands
} // namespace cache

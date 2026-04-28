#pragma once

#include "Command.hpp"
#include "storage/Store.hpp"
#include <unordered_map>
#include <string>

namespace cache {
namespace commands {

// Routes parsed RESP commands to registered handler functions.
// Registration happens once at startup — the map is immutable at runtime,
// so reads require no locking.
class Dispatcher {
public:
    Dispatcher();

    // Execute a parsed command array. Returns a RESP-encoded response string.
    std::string dispatch(const protocol::RespValue& req, storage::Store& store) const;

    void register_handler(const std::string& name, CommandHandler handler);

private:
    std::unordered_map<std::string, CommandHandler> handlers_;
};

} // namespace commands
} // namespace cache

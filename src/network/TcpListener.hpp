#pragma once

#include <cstdint>
#include <string>

namespace cache {
namespace network {

// Creates and binds the server TCP socket.
// Returns the listening fd on success (caller owns it).
int create_tcp_listener(const std::string& host, uint16_t port, int backlog = 1024);

} // namespace network
} // namespace cache

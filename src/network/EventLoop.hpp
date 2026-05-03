#pragma once

#include "Connection.hpp"
#include <unordered_map>
#include <memory>
#include <functional>
#include <cstdint>

// Platform-specific I/O multiplexing includes
#ifdef PLATFORM_LINUX
  #include <sys/epoll.h>
#elif defined(PLATFORM_MACOS)
  #include <sys/event.h>
  #include <sys/time.h>
#endif

namespace cache {
namespace network {

// Edge-triggered I/O multiplexer (epoll on Linux, kqueue on macOS).
// Manages a collection of Connections and drives their read/write callbacks.
//
// Design notes:
//   - All Connection objects are owned here via shared_ptr.
//   - Connections marked closed are reaped on the next iteration.
//   - Caller registers an on_accept callback; EventLoop calls it for each
//     new connection so the server layer can inject dependencies.
class EventLoop {
public:
    using AcceptCallback = std::function<std::shared_ptr<Connection>(int fd)>;

    EventLoop();
    ~EventLoop();

    // Non-copyable/movable
    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;

    // Add the listening socket. New connections are constructed via accept_cb.
    void add_listener(int listen_fd, AcceptCallback accept_cb);

    // Register an existing connection for read events.
    void add_connection(std::shared_ptr<Connection> conn);

    // Remove (and close) a connection by fd.
    void remove_connection(int fd);

    // Run the event loop until stop() is called.
    void run();

    // Signal the loop to exit on next iteration.
    void stop();

private:
    void poll_once();
    void handle_accept();
    void handle_readable(int fd);
    void handle_writable(int fd);
    void update_write_interest(int fd, bool want_write);
    void reap_closed();

    int     kq_or_epoll_fd_{-1};  // epoll fd (Linux) or kqueue fd (macOS)
    int     listen_fd_{-1};
    bool    running_{false};

    AcceptCallback accept_cb_;
    std::unordered_map<int, std::shared_ptr<Connection>> connections_;

    static constexpr int MAX_EVENTS = 256;
};

} // namespace network
} // namespace cache

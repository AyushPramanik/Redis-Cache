#pragma once

#include "Buffer.hpp"
#include "protocol/RespParser.hpp"
#include "protocol/RespSerializer.hpp"
#include "commands/Dispatcher.hpp"
#include "storage/Store.hpp"
#include "metrics/Metrics.hpp"
#include <atomic>
#include <mutex>
#include <string>
#include <memory>

namespace cache {
namespace network {

// Represents a single connected TCP client.
// Owned by the EventLoop — one Connection per file descriptor.
//
// Thread model:
//   - EventLoop thread calls on_readable() when data arrives.
//   - Worker thread calls on_readable_complete() to process parsed commands.
//   - Write buffer is mutex-protected so workers can write responses concurrently.
class Connection {
public:
    Connection(int fd,
               commands::Dispatcher& dispatcher,
               storage::Store&       store,
               metrics::Metrics&     metrics);
    ~Connection();

    // Non-copyable, non-movable (owned via shared_ptr by EventLoop)
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    // Called by event loop when socket is readable. Reads available data.
    // Returns false if the connection should be closed.
    bool on_readable();

    // Called by event loop when socket is writable. Flushes the write buffer.
    // Returns false if the connection should be closed.
    bool on_writable();

    // Whether the write buffer has pending data (event loop uses this to
    // decide whether to register EPOLLOUT / EVFILT_WRITE).
    [[nodiscard]] bool has_pending_write() const noexcept;

    [[nodiscard]] int fd() const noexcept { return fd_; }
    [[nodiscard]] bool is_closed() const noexcept { return closed_; }

private:
    // Parse all complete commands from in_buf_ and execute them.
    void process_commands();

    // Append resp to the write buffer; called from worker context.
    void write_response(const std::string& resp);

    int                      fd_;
    bool                     closed_{false};

    Buffer                   in_buf_;
    Buffer                   out_buf_;
    mutable std::mutex       write_mutex_;

    protocol::RespParser     parser_;
    commands::Dispatcher&    dispatcher_;
    storage::Store&          store_;
    metrics::Metrics&        metrics_;
};

} // namespace network
} // namespace cache

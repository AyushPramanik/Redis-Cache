#include "Connection.hpp"
#include "util/TimeUtils.hpp"
#include <spdlog/spdlog.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>

namespace cache {
namespace network {

Connection::Connection(int fd,
                       commands::Dispatcher& dispatcher,
                       storage::Store&       store,
                       metrics::Metrics&     metrics)
    : fd_(fd)
    , dispatcher_(dispatcher)
    , store_(store)
    , metrics_(metrics)
{}

Connection::~Connection() {
    if (fd_ >= 0) {
        ::close(fd_);
    }
}

bool Connection::on_readable() {
    while (true) {
        in_buf_.ensure_writable(4096);

        ssize_t n = ::recv(fd_, in_buf_.write_ptr(),
                           in_buf_.writable_size(), MSG_DONTWAIT);
        if (n > 0) {
            in_buf_.commit_write(static_cast<size_t>(n));
            continue;
        }
        if (n == 0) {
            // Peer closed connection cleanly
            closed_ = true;
            return false;
        }
        // n < 0
        if (errno == EAGAIN || errno == EWOULDBLOCK) break; // all data read
        if (errno == EINTR) continue;
        spdlog::warn("recv error on fd {}: {}", fd_, strerror(errno));
        closed_ = true;
        return false;
    }

    process_commands();
    return !closed_;
}

void Connection::process_commands() {
    // Process all complete commands in the read buffer.
    // Because multiple commands may have arrived in a single recv() call
    // (pipelining), we loop until the buffer is drained or a partial command
    // is reached. Responses are coalesced in out_buf_ and flushed together
    // by the next EPOLLOUT / EVFILT_WRITE event — this amortizes send() calls
    // and keeps throughput high under pipelined workloads.
    auto view = in_buf_.readable();
    std::string batch_resp;

    while (!view.empty()) {
        int64_t t0 = util::now_us();
        auto result = parser_.parse(view);

        if (result.status == protocol::ParseStatus::Incomplete) break;
        if (result.status == protocol::ParseStatus::Error) {
            write_response(protocol::RespSerializer::error("protocol error"));
            closed_ = true;
            return;
        }

        in_buf_.consume(result.consumed);
        view = in_buf_.readable();

        batch_resp += dispatcher_.dispatch(result.value, store_);

        int64_t latency_us = util::now_us() - t0;
        metrics_.record_command(latency_us);
    }

    if (!batch_resp.empty()) {
        write_response(batch_resp);
    }
}

bool Connection::on_writable() {
    std::unique_lock lock(write_mutex_);

    while (!out_buf_.empty()) {
        auto data = out_buf_.readable();
        ssize_t n = ::send(fd_, data.data(), data.size(), MSG_NOSIGNAL | MSG_DONTWAIT);
        if (n > 0) {
            out_buf_.consume(static_cast<size_t>(n));
            continue;
        }
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) {
            spdlog::warn("send error on fd {}: {}", fd_, strerror(errno));
            closed_ = true;
            return false;
        }
    }
    return true;
}

void Connection::write_response(const std::string& resp) {
    std::unique_lock lock(write_mutex_);
    out_buf_.append(resp);
}

bool Connection::has_pending_write() const noexcept {
    std::unique_lock lock(write_mutex_);
    return !out_buf_.empty();
}

} // namespace network
} // namespace cache

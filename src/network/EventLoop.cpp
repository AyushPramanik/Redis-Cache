#include "EventLoop.hpp"
#include <spdlog/spdlog.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>
#include <stdexcept>

namespace cache {
namespace network {

// ---- platform helpers ------------------------------------------------

static void set_nonblocking(int fd) {
    int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0) throw std::runtime_error("fcntl F_GETFL failed");
    if (::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
        throw std::runtime_error("fcntl F_SETFL O_NONBLOCK failed");
}

// ---- EventLoop -------------------------------------------------------

#ifdef PLATFORM_LINUX

EventLoop::EventLoop() {
    kq_or_epoll_fd_ = ::epoll_create1(EPOLL_CLOEXEC);
    if (kq_or_epoll_fd_ < 0) throw std::runtime_error("epoll_create1 failed");
}

EventLoop::~EventLoop() {
    if (kq_or_epoll_fd_ >= 0) ::close(kq_or_epoll_fd_);
}

void EventLoop::add_listener(int listen_fd, AcceptCallback accept_cb) {
    listen_fd_ = listen_fd;
    accept_cb_ = std::move(accept_cb);
    set_nonblocking(listen_fd_);

    epoll_event ev{};
    ev.events  = EPOLLIN | EPOLLET;
    ev.data.fd = listen_fd_;
    if (::epoll_ctl(kq_or_epoll_fd_, EPOLL_CTL_ADD, listen_fd_, &ev) < 0)
        throw std::runtime_error("epoll_ctl ADD listener failed");
}

void EventLoop::add_connection(std::shared_ptr<Connection> conn) {
    int fd = conn->fd();
    set_nonblocking(fd);

    epoll_event ev{};
    ev.events  = EPOLLIN | EPOLLET;
    ev.data.fd = fd;
    ::epoll_ctl(kq_or_epoll_fd_, EPOLL_CTL_ADD, fd, &ev);

    connections_.emplace(fd, std::move(conn));
}

void EventLoop::remove_connection(int fd) {
    ::epoll_ctl(kq_or_epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
    connections_.erase(fd);
}

void EventLoop::update_write_interest(int fd, bool want_write) {
    epoll_event ev{};
    ev.data.fd = fd;
    ev.events  = EPOLLIN | EPOLLET | (want_write ? EPOLLOUT : 0);
    ::epoll_ctl(kq_or_epoll_fd_, EPOLL_CTL_MOD, fd, &ev);
}

void EventLoop::poll_once() {
    epoll_event events[MAX_EVENTS];
    int n = ::epoll_wait(kq_or_epoll_fd_, events, MAX_EVENTS, 50 /*ms timeout*/);

    for (int i = 0; i < n; ++i) {
        int fd     = events[i].data.fd;
        uint32_t e = events[i].events;

        if (fd == listen_fd_) {
            handle_accept();
            continue;
        }

        if (e & (EPOLLERR | EPOLLHUP)) {
            remove_connection(fd);
            continue;
        }
        if (e & EPOLLIN)  handle_readable(fd);
        if (e & EPOLLOUT) handle_writable(fd);
    }
}

#elif defined(PLATFORM_MACOS)

EventLoop::EventLoop() {
    kq_or_epoll_fd_ = ::kqueue();
    if (kq_or_epoll_fd_ < 0) throw std::runtime_error("kqueue() failed");
}

EventLoop::~EventLoop() {
    if (kq_or_epoll_fd_ >= 0) ::close(kq_or_epoll_fd_);
}

void EventLoop::add_listener(int listen_fd, AcceptCallback accept_cb) {
    listen_fd_ = listen_fd;
    accept_cb_ = std::move(accept_cb);
    set_nonblocking(listen_fd_);

    struct kevent ev;
    EV_SET(&ev, listen_fd_, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, nullptr);
    ::kevent(kq_or_epoll_fd_, &ev, 1, nullptr, 0, nullptr);
}

void EventLoop::add_connection(std::shared_ptr<Connection> conn) {
    int fd = conn->fd();
    set_nonblocking(fd);

    struct kevent ev;
    EV_SET(&ev, fd, EVFILT_READ, EV_ADD | EV_ENABLE | EV_CLEAR, 0, 0, nullptr);
    ::kevent(kq_or_epoll_fd_, &ev, 1, nullptr, 0, nullptr);

    connections_.emplace(fd, std::move(conn));
}

void EventLoop::remove_connection(int fd) {
    struct kevent ev;
    EV_SET(&ev, fd, EVFILT_READ,  EV_DELETE, 0, 0, nullptr);
    ::kevent(kq_or_epoll_fd_, &ev, 1, nullptr, 0, nullptr);
    EV_SET(&ev, fd, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);
    ::kevent(kq_or_epoll_fd_, &ev, 1, nullptr, 0, nullptr);
    connections_.erase(fd);
}

void EventLoop::update_write_interest(int fd, bool want_write) {
    struct kevent ev;
    uint16_t flags = want_write ? (EV_ADD | EV_ENABLE | EV_CLEAR) : EV_DELETE;
    EV_SET(&ev, fd, EVFILT_WRITE, flags, 0, 0, nullptr);
    ::kevent(kq_or_epoll_fd_, &ev, 1, nullptr, 0, nullptr);
}

void EventLoop::poll_once() {
    struct kevent events[MAX_EVENTS];
    struct timespec timeout{0, 50'000'000}; // 50 ms

    int n = ::kevent(kq_or_epoll_fd_, nullptr, 0, events, MAX_EVENTS, &timeout);
    for (int i = 0; i < n; ++i) {
        int fd = static_cast<int>(events[i].ident);

        if (events[i].flags & EV_ERROR) {
            remove_connection(fd);
            continue;
        }
        if (fd == listen_fd_) {
            handle_accept();
            continue;
        }
        if (events[i].filter == EVFILT_READ)  handle_readable(fd);
        if (events[i].filter == EVFILT_WRITE) handle_writable(fd);
    }
}

#endif // PLATFORM_MACOS

// ---- platform-independent methods ------------------------------------

void EventLoop::run() {
    running_ = true;
    spdlog::info("EventLoop started");
    while (running_) {
        poll_once();
        reap_closed();
    }
    spdlog::info("EventLoop stopped");
}

void EventLoop::stop() {
    running_ = false;
}

void EventLoop::handle_accept() {
    while (true) {
        sockaddr_in addr{};
        socklen_t   len = sizeof(addr);
        int cfd = ::accept(listen_fd_, reinterpret_cast<sockaddr*>(&addr), &len);
        if (cfd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            if (errno == EINTR) continue;
            spdlog::error("accept() error: {}", strerror(errno));
            break;
        }

        char ip[INET_ADDRSTRLEN];
        ::inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));
        spdlog::debug("new connection from {}:{} fd={}", ip, ntohs(addr.sin_port), cfd);

        auto conn = accept_cb_(cfd);
        if (conn) add_connection(std::move(conn));
        else ::close(cfd);
    }
}

void EventLoop::handle_readable(int fd) {
    auto it = connections_.find(fd);
    if (it == connections_.end()) return;

    auto& conn = it->second;
    bool ok = conn->on_readable();

    if (!ok || conn->is_closed()) {
        remove_connection(fd);
        return;
    }

    // If the connection has data to send, register for write events
    if (conn->has_pending_write()) {
        update_write_interest(fd, true);
    }
}

void EventLoop::handle_writable(int fd) {
    auto it = connections_.find(fd);
    if (it == connections_.end()) return;

    auto& conn = it->second;
    bool ok = conn->on_writable();

    if (!ok || conn->is_closed()) {
        remove_connection(fd);
        return;
    }

    // De-register write interest if nothing left to send
    if (!conn->has_pending_write()) {
        update_write_interest(fd, false);
    }
}

void EventLoop::reap_closed() {
    std::vector<int> to_remove;
    for (const auto& [fd, conn] : connections_) {
        if (conn->is_closed()) to_remove.push_back(fd);
    }
    for (int fd : to_remove) remove_connection(fd);
}

} // namespace network
} // namespace cache

#include "TcpListener.hpp"
#include <spdlog/spdlog.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <stdexcept>

namespace cache {
namespace network {

int create_tcp_listener(const std::string& host, uint16_t port, int backlog) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) throw std::runtime_error(std::string("socket() failed: ") + strerror(errno));

    // Avoid TIME_WAIT on restart
    int yes = 1;
    ::setsockopt(fd, SOL_SOCKET,  SO_REUSEADDR, &yes, sizeof(yes));

    // Disable Nagle for lower latency on small Redis-like messages
    ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY,  &yes, sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);

    if (host == "0.0.0.0" || host.empty()) {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
            ::close(fd);
            throw std::runtime_error("Invalid bind address: " + host);
        }
    }

    if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(fd);
        throw std::runtime_error(std::string("bind() failed: ") + strerror(errno));
    }

    if (::listen(fd, backlog) < 0) {
        ::close(fd);
        throw std::runtime_error(std::string("listen() failed: ") + strerror(errno));
    }

    spdlog::info("TCP listener bound to {}:{}", host.empty() ? "0.0.0.0" : host, port);
    return fd;
}

} // namespace network
} // namespace cache

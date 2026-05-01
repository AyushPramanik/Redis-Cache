#include "HttpServer.hpp"
#include <spdlog/spdlog.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <fmt/format.h>
#include <cerrno>
#include <cstring>
#include <sstream>

namespace cache {
namespace metrics {

HttpServer::HttpServer(uint16_t port, Metrics& metrics, storage::Store& store)
    : port_(port), metrics_(metrics), store_(store) {}

HttpServer::~HttpServer() {
    stop();
}

void HttpServer::start() {
    server_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        spdlog::error("HttpServer: socket() failed: {}", strerror(errno));
        return;
    }

    int opt = 1;
    ::setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(port_);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (::bind(server_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        spdlog::error("HttpServer: bind() on port {} failed: {}", port_, strerror(errno));
        ::close(server_fd_);
        server_fd_ = -1;
        return;
    }

    ::listen(server_fd_, 8);
    running_ = true;
    thread_  = std::thread([this] { serve_loop(); });
    spdlog::info("Metrics HTTP server listening on :{}", port_);
}

void HttpServer::stop() {
    running_ = false;
    if (server_fd_ >= 0) {
        ::shutdown(server_fd_, SHUT_RDWR);
        ::close(server_fd_);
        server_fd_ = -1;
    }
    if (thread_.joinable()) thread_.join();
}

void HttpServer::serve_loop() {
    while (running_) {
        sockaddr_in client_addr{};
        socklen_t   client_len = sizeof(client_addr);
        int cfd = ::accept(server_fd_,
                           reinterpret_cast<sockaddr*>(&client_addr),
                           &client_len);
        if (cfd < 0) {
            if (running_) spdlog::debug("HttpServer: accept() error: {}", strerror(errno));
            break;
        }
        handle_client(cfd);
        ::close(cfd);
    }
}

void HttpServer::handle_client(int client_fd) {
    char buf[2048] = {};
    ssize_t n = ::recv(client_fd, buf, sizeof(buf) - 1, 0);
    if (n <= 0) return;

    std::string req(buf, static_cast<size_t>(n));
    std::string method, path;
    std::istringstream ss(req);
    ss >> method >> path;

    std::string body = route(method, path);

    bool is_metrics = (path == "/metrics");
    std::string content_type = is_metrics
        ? "text/plain; version=0.0.4; charset=utf-8"
        : "application/json";

    std::string response = fmt::format(
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: {}\r\n"
        "Content-Length: {}\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Connection: close\r\n"
        "\r\n"
        "{}",
        content_type, body.size(), body);

    ::send(client_fd, response.data(), response.size(), MSG_NOSIGNAL);
}

std::string HttpServer::route(const std::string& /*method*/, const std::string& path) {
    if (path == "/metrics") {
        return metrics_.prometheus_text();
    }
    if (path == "/health") {
        return R"({"status":"ok"})";
    }
    if (path == "/stats") {
        auto s = metrics_.snapshot();
        return fmt::format(
            R"({{)"
            R"("total_commands":{},"total_hits":{},"total_misses":{},"active_connections":{},"key_count":{},"memory_bytes":{},"avg_latency_us":{:.2f},"hit_rate":{:.4f},"commands_per_sec":{})"
            R"(}})",
            s.total_commands, s.total_hits, s.total_misses,
            s.active_connections, s.key_count, s.memory_bytes,
            s.avg_latency_us, s.hit_rate, s.commands_per_sec);
    }
    return R"({"error":"not found"})";
}

} // namespace metrics
} // namespace cache

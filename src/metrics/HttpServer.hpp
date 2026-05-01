#pragma once

#include "Metrics.hpp"
#include "storage/Store.hpp"
#include <string>
#include <thread>
#include <atomic>
#include <functional>

namespace cache {
namespace metrics {

// Minimal single-threaded HTTP/1.1 server for /metrics and /health endpoints.
// Runs on a dedicated thread — intentionally simple, not a general HTTP stack.
// The metrics endpoint returns Prometheus exposition format.
class HttpServer {
public:
    HttpServer(uint16_t port, Metrics& metrics, storage::Store& store);
    ~HttpServer();

    void start();
    void stop();

private:
    void serve_loop();
    void handle_client(int client_fd);

    // Returns the HTTP response body for a given path
    std::string route(const std::string& method, const std::string& path);

    uint16_t        port_;
    Metrics&        metrics_;
    storage::Store& store_;
    int             server_fd_{-1};
    std::thread     thread_;
    std::atomic<bool> running_{false};
};

} // namespace metrics
} // namespace cache

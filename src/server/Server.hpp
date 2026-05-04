#pragma once

#include "Config.hpp"
#include "storage/Store.hpp"
#include "commands/Dispatcher.hpp"
#include "network/EventLoop.hpp"
#include "network/TcpListener.hpp"
#include "persistence/Rdb.hpp"
#include "persistence/Aof.hpp"
#include "metrics/Metrics.hpp"
#include "metrics/HttpServer.hpp"
#include "concurrency/ThreadPool.hpp"
#include <thread>
#include <atomic>
#include <csignal>

namespace cache {
namespace server {

// Top-level coordinator.
// Owns all subsystems and wires them together:
//   Config → Store + Dispatcher + EventLoop + ThreadPool + Persistence + Metrics
class Server {
public:
    explicit Server(Config cfg);
    ~Server();

    // Block until stop() is called (e.g., from a signal handler).
    void run();

    // Signal the server to shut down gracefully.
    void stop();

    // Expose for signal handler access
    static Server* instance;

private:
    void maintenance_loop(); // expiry sweep + periodic RDB + metrics refresh

    Config                    cfg_;
    storage::Store            store_;
    commands::Dispatcher      dispatcher_;
    network::EventLoop        event_loop_;
    metrics::Metrics          metrics_;
    metrics::HttpServer       http_server_;
    persistence::Rdb          rdb_;
    persistence::Aof          aof_;
    concurrency::ThreadPool   thread_pool_;

    int                       listen_fd_{-1};
    std::thread               maintenance_thread_;
    std::atomic<bool>         running_{false};
};

} // namespace server
} // namespace cache

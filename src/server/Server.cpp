#include "Server.hpp"
#include "util/Logger.hpp"
#include "util/TimeUtils.hpp"
#include <spdlog/spdlog.h>
#include <csignal>
#include <unistd.h>
#include <chrono>
#include <thread>

namespace cache {
namespace server {

Server* Server::instance = nullptr;

Server::Server(Config cfg)
    : cfg_(std::move(cfg))
    , store_(cfg_.max_keys)
    , dispatcher_()
    , event_loop_()
    , metrics_()
    , http_server_(cfg_.metrics_port, metrics_, store_)
    , rdb_(cfg_.rdb_path)
    , aof_(cfg_.aof_path)
    , thread_pool_(cfg_.worker_threads)
{
    util::init_logger(cfg_.log_level, cfg_.log_file);
    spdlog::info("cacheserver starting (port={}, workers={}, max_keys={})",
                 cfg_.port, cfg_.worker_threads, cfg_.max_keys);
    instance = this;
}

Server::~Server() {
    stop();
    instance = nullptr;
}

void Server::run() {
    // Load persisted data
    if (cfg_.rdb_enabled) rdb_.load(store_);
    if (cfg_.aof_enabled) {
        aof_.open();
        aof_.replay(store_, [this](const protocol::RespValue& cmd, storage::Store& s) {
            dispatcher_.dispatch(cmd, s);
        });
    }

    // Start metrics HTTP server
    if (cfg_.metrics_enabled) http_server_.start();

    // Create TCP listener
    listen_fd_ = network::create_tcp_listener(cfg_.host, cfg_.port, cfg_.backlog);

    // Wire accept callback
    event_loop_.add_listener(listen_fd_, [this](int fd) -> std::shared_ptr<network::Connection> {
        metrics_.record_connection_open();
        auto conn = std::make_shared<network::Connection>(fd, dispatcher_, store_, metrics_);
        spdlog::debug("accepted connection fd={}", fd);
        return conn;
    });

    // Start maintenance thread (expiry sweep + RDB snapshots)
    running_ = true;
    maintenance_thread_ = std::thread([this] { maintenance_loop(); });

    spdlog::info("ready to accept connections on {}:{}", cfg_.host, cfg_.port);

    // Block in the event loop
    event_loop_.run();

    // Cleanup
    running_ = false;
    if (maintenance_thread_.joinable()) maintenance_thread_.join();

    // Final snapshot on shutdown
    if (cfg_.rdb_enabled) rdb_.save(store_);
    if (cfg_.aof_enabled) aof_.close();
    if (cfg_.metrics_enabled) http_server_.stop();

    if (listen_fd_ >= 0) {
        ::close(listen_fd_);
        listen_fd_ = -1;
    }

    spdlog::info("cacheserver stopped cleanly");
}

void Server::stop() {
    event_loop_.stop();
    running_ = false;
}

void Server::maintenance_loop() {
    int64_t last_rdb_save  = util::now_ms();
    int64_t rdb_interval_ms = static_cast<int64_t>(cfg_.rdb_interval_secs) * 1000;

    while (running_) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(cfg_.sweep_interval_ms));

        store_.sweep_expired();
        metrics_.set_key_count(static_cast<int64_t>(store_.size()));

        int64_t now = util::now_ms();
        if (cfg_.rdb_enabled && (now - last_rdb_save) >= rdb_interval_ms) {
            rdb_.save(store_);
            last_rdb_save = now;
        }
    }
}

} // namespace server
} // namespace cache

#include "server/Server.hpp"
#include "server/Config.hpp"
#include <spdlog/spdlog.h>
#include <csignal>
#include <cstdlib>
#include <stdexcept>

static void signal_handler(int sig) {
    spdlog::info("signal {} received, shutting down...", sig);
    if (cache::server::Server::instance) {
        cache::server::Server::instance->stop();
    }
}

int main(int argc, char** argv) {
    // Default config path; override with --config <path>
    std::string config_path;
    for (int i = 1; i < argc - 1; ++i) {
        if (std::string(argv[i]) == "--config") {
            config_path = argv[i + 1];
            break;
        }
    }

    auto cfg = config_path.empty()
        ? cache::server::Config{}
        : cache::server::Config::from_file(config_path);

    cfg.apply_args(argc, argv);

    // Register signal handlers for graceful shutdown
    std::signal(SIGINT,  signal_handler);
    std::signal(SIGTERM, signal_handler);
    std::signal(SIGPIPE, SIG_IGN); // ignore broken-pipe from disconnected clients

    try {
        cache::server::Server server(std::move(cfg));
        server.run();
    } catch (const std::exception& e) {
        spdlog::critical("fatal: {}", e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

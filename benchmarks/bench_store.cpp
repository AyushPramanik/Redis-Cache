// Standalone microbenchmark for the storage engine.
// Run with: ./bench_store [num_ops]
// Reports: throughput (ops/sec) and approximate latency percentiles.

#include "storage/Store.hpp"
#include "util/TimeUtils.hpp"
#include <iostream>
#include <vector>
#include <numeric>
#include <algorithm>
#include <string>
#include <thread>
#include <random>

using cache::storage::Store;

struct BenchResult {
    double   ops_per_sec;
    int64_t  p50_us;
    int64_t  p99_us;
    int64_t  p999_us;
};

BenchResult bench_set(Store& store, int num_ops) {
    std::vector<int64_t> latencies;
    latencies.reserve(static_cast<size_t>(num_ops));

    int64_t start = cache::util::now_us();
    for (int i = 0; i < num_ops; ++i) {
        int64_t t0 = cache::util::now_us();
        store.set("bench_key_" + std::to_string(i), "value");
        latencies.push_back(cache::util::now_us() - t0);
    }
    int64_t elapsed_us = cache::util::now_us() - start;

    std::sort(latencies.begin(), latencies.end());
    return {
        static_cast<double>(num_ops) * 1e6 / static_cast<double>(elapsed_us),
        latencies[static_cast<size_t>(num_ops) * 50  / 100],
        latencies[static_cast<size_t>(num_ops) * 99  / 100],
        latencies[static_cast<size_t>(num_ops) * 999 / 1000],
    };
}

BenchResult bench_get(Store& store, int num_ops) {
    // Pre-populate
    for (int i = 0; i < num_ops; ++i)
        store.set("g_" + std::to_string(i), "v");

    std::vector<int64_t> latencies;
    latencies.reserve(static_cast<size_t>(num_ops));
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, num_ops - 1);

    int64_t start = cache::util::now_us();
    for (int i = 0; i < num_ops; ++i) {
        int64_t t0 = cache::util::now_us();
        store.get("g_" + std::to_string(dist(rng)));
        latencies.push_back(cache::util::now_us() - t0);
    }
    int64_t elapsed_us = cache::util::now_us() - start;

    std::sort(latencies.begin(), latencies.end());
    return {
        static_cast<double>(num_ops) * 1e6 / static_cast<double>(elapsed_us),
        latencies[static_cast<size_t>(num_ops) * 50  / 100],
        latencies[static_cast<size_t>(num_ops) * 99  / 100],
        latencies[static_cast<size_t>(num_ops) * 999 / 1000],
    };
}

static void print_result(const std::string& name, const BenchResult& r) {
    std::cout << "[" << name << "]\n"
              << "  throughput : " << static_cast<int>(r.ops_per_sec) << " ops/sec\n"
              << "  p50 latency: " << r.p50_us  << " μs\n"
              << "  p99 latency: " << r.p99_us  << " μs\n"
              << "  p999 latency: " << r.p999_us << " μs\n\n";
}

int main(int argc, char** argv) {
    int num_ops = (argc > 1) ? std::stoi(argv[1]) : 100'000;
    std::cout << "Running store benchmark with " << num_ops << " ops each...\n\n";

    // Single-threaded SET
    {
        Store store;
        auto r = bench_set(store, num_ops);
        print_result("SET (single-thread)", r);
    }

    // Single-threaded GET
    {
        Store store;
        auto r = bench_get(store, num_ops);
        print_result("GET (single-thread, random)", r);
    }

    // Concurrent SET/GET with 8 threads
    {
        Store store;
        constexpr int N_THREADS = 8;
        int ops_per_thread = num_ops / N_THREADS;
        std::vector<std::thread> threads;
        std::vector<BenchResult> results(N_THREADS);

        for (int t = 0; t < N_THREADS; ++t) {
            threads.emplace_back([&, t] {
                for (int i = 0; i < ops_per_thread; ++i)
                    store.set("mt_" + std::to_string(t) + "_" + std::to_string(i), "v");
                // Measure GET half
                std::vector<int64_t> lat;
                lat.reserve(static_cast<size_t>(ops_per_thread));
                int64_t s = cache::util::now_us();
                for (int i = 0; i < ops_per_thread; ++i) {
                    int64_t t0 = cache::util::now_us();
                    store.get("mt_" + std::to_string(t) + "_" + std::to_string(i));
                    lat.push_back(cache::util::now_us() - t0);
                }
                int64_t el = cache::util::now_us() - s;
                std::sort(lat.begin(), lat.end());
                results[static_cast<size_t>(t)] = {
                    static_cast<double>(ops_per_thread) * 1e6 / static_cast<double>(el),
                    lat[static_cast<size_t>(ops_per_thread) * 50  / 100],
                    lat[static_cast<size_t>(ops_per_thread) * 99  / 100],
                    lat[static_cast<size_t>(ops_per_thread) * 999 / 1000],
                };
            });
        }
        for (auto& th : threads) th.join();

        double total_tput = 0;
        for (auto& r : results) total_tput += r.ops_per_sec;
        BenchResult agg = results[0];
        agg.ops_per_sec = total_tput;
        print_result("GET (8 threads concurrent)", agg);
    }

    return 0;
}

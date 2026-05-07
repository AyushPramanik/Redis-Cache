#include <catch2/catch_test_macros.hpp>
#include "storage/Store.hpp"
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

using namespace cache::storage;

TEST_CASE("Store - basic set/get", "[store]") {
    Store s;
    s.set("key", "value");
    auto v = s.get("key");
    REQUIRE(v.has_value());
    REQUIRE(*v == "value");
}

TEST_CASE("Store - missing key returns nullopt", "[store]") {
    Store s;
    auto v = s.get("no_such_key");
    REQUIRE(!v.has_value());
}

TEST_CASE("Store - delete", "[store]") {
    Store s;
    s.set("x", "1");
    REQUIRE(s.del("x") == true);
    REQUIRE(s.del("x") == false);
    REQUIRE(!s.get("x").has_value());
}

TEST_CASE("Store - exists", "[store]") {
    Store s;
    s.set("k", "v");
    REQUIRE(s.exists("k") == true);
    REQUIRE(s.exists("nope") == false);
}

TEST_CASE("Store - TTL expiry", "[store]") {
    Store s;
    s.set("temp", "bye", 50); // 50ms TTL
    REQUIRE(s.get("temp").has_value());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    REQUIRE(!s.get("temp").has_value()); // expired
}

TEST_CASE("Store - TTL returns -1 for no expiry", "[store]") {
    Store s;
    s.set("k", "v");
    REQUIRE(s.ttl("k") == -1);
}

TEST_CASE("Store - TTL returns -2 for missing key", "[store]") {
    Store s;
    REQUIRE(s.ttl("missing") == -2);
}

TEST_CASE("Store - expire updates TTL", "[store]") {
    Store s;
    s.set("k", "v");
    REQUIRE(s.expire("k", 10000));
    auto t = s.ttl("k");
    REQUIRE(t > 0);
    REQUIRE(t <= 10000);
}

TEST_CASE("Store - incr on new key starts at 1", "[store]") {
    Store s;
    auto r = s.incr("counter");
    REQUIRE(r.has_value());
    REQUIRE(*r == 1);
}

TEST_CASE("Store - incr increments existing integer", "[store]") {
    Store s;
    s.set("n", "10");
    auto r = s.incr("n");
    REQUIRE(r.has_value());
    REQUIRE(*r == 11);
}

TEST_CASE("Store - incr on non-integer returns nullopt", "[store]") {
    Store s;
    s.set("bad", "not_a_number");
    auto r = s.incr("bad");
    REQUIRE(!r.has_value());
}

TEST_CASE("Store - flush removes all keys", "[store]") {
    Store s;
    for (int i = 0; i < 100; ++i) s.set("k" + std::to_string(i), "v");
    s.flush();
    REQUIRE(s.size() == 0);
}

TEST_CASE("Store - concurrent set/get is safe", "[store][concurrency]") {
    Store s;
    constexpr int N = 8;
    constexpr int OPS = 1000;

    std::vector<std::thread> threads;
    std::atomic<int> errors{0};

    for (int t = 0; t < N; ++t) {
        threads.emplace_back([&s, t, &errors] {
            for (int i = 0; i < OPS; ++i) {
                std::string key = "k" + std::to_string(t) + "_" + std::to_string(i);
                s.set(key, "val");
                auto v = s.get(key);
                if (v && *v != "val") ++errors;
            }
        });
    }
    for (auto& th : threads) th.join();
    REQUIRE(errors == 0);
}

TEST_CASE("Store - sweep_expired removes expired keys", "[store]") {
    Store s;
    s.set("short", "gone", 10); // 10ms
    s.set("long",  "here", 60000);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    s.sweep_expired();
    REQUIRE(!s.exists("short"));
    REQUIRE(s.exists("long"));
}

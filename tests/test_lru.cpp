#include <catch2/catch_test_macros.hpp>
#include "storage/LruEviction.hpp"

using namespace cache::storage;

TEST_CASE("LruList - insert and lru_key", "[lru]") {
    LruList lru;
    lru.insert("a");
    lru.insert("b");
    lru.insert("c");
    // c was inserted last → MRU, a is LRU
    REQUIRE(lru.lru_key() == "a");
}

TEST_CASE("LruList - touch moves to MRU", "[lru]") {
    LruList lru;
    auto n1 = lru.insert("a");
    lru.insert("b");
    lru.insert("c");
    lru.touch(n1); // a is now MRU
    REQUIRE(lru.lru_key() == "b");
}

TEST_CASE("LruList - evict_lru returns and removes LRU", "[lru]") {
    LruList lru;
    lru.insert("x");
    lru.insert("y");
    lru.insert("z");
    auto evicted = lru.evict_lru();
    REQUIRE(evicted == "x");
    REQUIRE(lru.size() == 2);
}

TEST_CASE("LruList - remove arbitrary node", "[lru]") {
    LruList lru;
    lru.insert("a");
    auto nb = lru.insert("b");
    lru.insert("c");
    lru.remove(nb);
    REQUIRE(lru.size() == 2);
    REQUIRE(lru.lru_key() == "a");
}

TEST_CASE("LruList - empty check", "[lru]") {
    LruList lru;
    REQUIRE(lru.empty());
    lru.insert("k");
    REQUIRE(!lru.empty());
    lru.evict_lru();
    REQUIRE(lru.empty());
}

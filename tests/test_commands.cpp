#include <catch2/catch_test_macros.hpp>
#include "commands/Dispatcher.hpp"
#include "protocol/RespParser.hpp"
#include "protocol/RespSerializer.hpp"
#include "storage/Store.hpp"

using namespace cache;

// Helper: build a command request value from argc/argv style
static protocol::RespValue make_cmd(std::initializer_list<std::string> args) {
    protocol::RespValue req;
    req.type = protocol::RespType::Array;
    for (const auto& a : args) {
        protocol::RespValue v;
        v.type = protocol::RespType::BulkString;
        v.str  = a;
        req.array.push_back(std::move(v));
    }
    return req;
}

static std::string run(commands::Dispatcher& d, storage::Store& s,
                       std::initializer_list<std::string> args) {
    return d.dispatch(make_cmd(args), s);
}

TEST_CASE("Dispatcher - PING", "[commands]") {
    commands::Dispatcher d;
    storage::Store s;
    REQUIRE(run(d, s, {"PING"}) == "+PONG\r\n");
}

TEST_CASE("Dispatcher - PING with message", "[commands]") {
    commands::Dispatcher d;
    storage::Store s;
    REQUIRE(run(d, s, {"PING", "hello"}) == "$5\r\nhello\r\n");
}

TEST_CASE("Dispatcher - SET and GET", "[commands]") {
    commands::Dispatcher d;
    storage::Store s;
    REQUIRE(run(d, s, {"SET", "mykey", "myval"}) == "+OK\r\n");
    REQUIRE(run(d, s, {"GET", "mykey"}) == "$5\r\nmyval\r\n");
}

TEST_CASE("Dispatcher - GET missing key", "[commands]") {
    commands::Dispatcher d;
    storage::Store s;
    REQUIRE(run(d, s, {"GET", "missing"}) == "$-1\r\n");
}

TEST_CASE("Dispatcher - DEL", "[commands]") {
    commands::Dispatcher d;
    storage::Store s;
    run(d, s, {"SET", "k", "v"});
    REQUIRE(run(d, s, {"DEL", "k"}) == ":1\r\n");
    REQUIRE(run(d, s, {"DEL", "k"}) == ":0\r\n");
}

TEST_CASE("Dispatcher - EXISTS", "[commands]") {
    commands::Dispatcher d;
    storage::Store s;
    run(d, s, {"SET", "a", "1"});
    REQUIRE(run(d, s, {"EXISTS", "a"}) == ":1\r\n");
    REQUIRE(run(d, s, {"EXISTS", "b"}) == ":0\r\n");
}

TEST_CASE("Dispatcher - INCR", "[commands]") {
    commands::Dispatcher d;
    storage::Store s;
    REQUIRE(run(d, s, {"INCR", "ctr"}) == ":1\r\n");
    REQUIRE(run(d, s, {"INCR", "ctr"}) == ":2\r\n");
    REQUIRE(run(d, s, {"INCR", "ctr"}) == ":3\r\n");
}

TEST_CASE("Dispatcher - INCR on non-integer", "[commands]") {
    commands::Dispatcher d;
    storage::Store s;
    run(d, s, {"SET", "bad", "abc"});
    auto r = run(d, s, {"INCR", "bad"});
    REQUIRE(r.substr(0, 1) == "-"); // should be an error
}

TEST_CASE("Dispatcher - SET with EX", "[commands]") {
    commands::Dispatcher d;
    storage::Store s;
    run(d, s, {"SET", "exp_key", "val", "EX", "10"});
    auto ttl_resp = run(d, s, {"TTL", "exp_key"});
    // Should be :10\r\n or :9\r\n depending on timing
    REQUIRE(ttl_resp.substr(0, 1) == ":");
    int64_t ttl = std::stoll(ttl_resp.substr(1, ttl_resp.size() - 3));
    REQUIRE(ttl > 0);
    REQUIRE(ttl <= 10);
}

TEST_CASE("Dispatcher - DBSIZE", "[commands]") {
    commands::Dispatcher d;
    storage::Store s;
    run(d, s, {"SET", "a", "1"});
    run(d, s, {"SET", "b", "2"});
    REQUIRE(run(d, s, {"DBSIZE"}) == ":2\r\n");
}

TEST_CASE("Dispatcher - FLUSHALL", "[commands]") {
    commands::Dispatcher d;
    storage::Store s;
    run(d, s, {"SET", "a", "1"});
    run(d, s, {"FLUSHALL"});
    REQUIRE(run(d, s, {"DBSIZE"}) == ":0\r\n");
}

TEST_CASE("Dispatcher - unknown command", "[commands]") {
    commands::Dispatcher d;
    storage::Store s;
    auto r = run(d, s, {"UNKNOWNCMD"});
    REQUIRE(r.substr(0, 1) == "-");
}

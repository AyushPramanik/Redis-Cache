#include <catch2/catch_test_macros.hpp>
#include "protocol/RespParser.hpp"
#include "protocol/RespTypes.hpp"
#include <string>

using namespace cache::protocol;

TEST_CASE("RespParser - simple string", "[parser]") {
    RespParser p;
    auto res = p.parse("+OK\r\n");
    REQUIRE(res.status == ParseStatus::Ok);
    REQUIRE(res.value.type == RespType::SimpleString);
    REQUIRE(res.value.str == "OK");
    REQUIRE(res.consumed == 5);
}

TEST_CASE("RespParser - error", "[parser]") {
    RespParser p;
    auto res = p.parse("-ERR unknown command\r\n");
    REQUIRE(res.status == ParseStatus::Ok);
    REQUIRE(res.value.type == RespType::Error);
    REQUIRE(res.value.str == "ERR unknown command");
}

TEST_CASE("RespParser - integer", "[parser]") {
    RespParser p;
    auto res = p.parse(":42\r\n");
    REQUIRE(res.status == ParseStatus::Ok);
    REQUIRE(res.value.type == RespType::Integer);
    REQUIRE(res.value.integer == 42);
}

TEST_CASE("RespParser - negative integer", "[parser]") {
    RespParser p;
    auto res = p.parse(":-1\r\n");
    REQUIRE(res.status == ParseStatus::Ok);
    REQUIRE(res.value.integer == -1);
}

TEST_CASE("RespParser - bulk string", "[parser]") {
    RespParser p;
    auto res = p.parse("$6\r\nfoobar\r\n");
    REQUIRE(res.status == ParseStatus::Ok);
    REQUIRE(res.value.type == RespType::BulkString);
    REQUIRE(res.value.str == "foobar");
    REQUIRE(res.consumed == 12);
}

TEST_CASE("RespParser - null bulk string", "[parser]") {
    RespParser p;
    auto res = p.parse("$-1\r\n");
    REQUIRE(res.status == ParseStatus::Ok);
    REQUIRE(res.value.type == RespType::BulkString);
    REQUIRE(res.value.is_null == true);
}

TEST_CASE("RespParser - array", "[parser]") {
    RespParser p;
    // *2\r\n$3\r\nSET\r\n$5\r\nhello\r\n
    auto res = p.parse("*2\r\n$3\r\nSET\r\n$5\r\nhello\r\n");
    REQUIRE(res.status == ParseStatus::Ok);
    REQUIRE(res.value.type == RespType::Array);
    REQUIRE(res.value.array.size() == 2);
    REQUIRE(res.value.array[0].str == "SET");
    REQUIRE(res.value.array[1].str == "hello");
}

TEST_CASE("RespParser - full SET command", "[parser]") {
    RespParser p;
    std::string cmd = "*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\nbar\r\n";
    auto res = p.parse(cmd);
    REQUIRE(res.status == ParseStatus::Ok);
    REQUIRE(res.value.array.size() == 3);
    REQUIRE(res.value.array[0].str == "SET");
    REQUIRE(res.value.array[1].str == "foo");
    REQUIRE(res.value.array[2].str == "bar");
    REQUIRE(res.consumed == cmd.size());
}

TEST_CASE("RespParser - incomplete read", "[parser]") {
    RespParser p;
    // Partial bulk string
    auto res = p.parse("$6\r\nfoo");
    REQUIRE(res.status == ParseStatus::Incomplete);
    REQUIRE(res.consumed == 0);
}

TEST_CASE("RespParser - invalid type byte", "[parser]") {
    RespParser p;
    auto res = p.parse("X invalid\r\n");
    REQUIRE(res.status == ParseStatus::Error);
}

TEST_CASE("RespParser - multiple commands in stream", "[parser]") {
    RespParser p;
    std::string buf = "+PONG\r\n:1000\r\n";
    auto r1 = p.parse(buf);
    REQUIRE(r1.status == ParseStatus::Ok);
    REQUIRE(r1.value.str == "PONG");

    auto r2 = p.parse(std::string_view(buf).substr(r1.consumed));
    REQUIRE(r2.status == ParseStatus::Ok);
    REQUIRE(r2.value.integer == 1000);
}

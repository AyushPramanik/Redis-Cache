#pragma once

#include <string>
#include <vector>
#include <variant>
#include <optional>
#include <cstdint>

namespace cache {
namespace protocol {

enum class RespType : uint8_t {
    SimpleString,
    Error,
    Integer,
    BulkString,
    Array,
    Null,
};

// Unified RESP value used throughout the parser and serializer.
// Array elements are stored inline — depth is typically <=2 for Redis traffic.
struct RespValue {
    RespType type  = RespType::Null;
    std::string   str;       // SimpleString, Error, BulkString
    int64_t       integer = 0; // Integer
    std::vector<RespValue> array; // Array
    bool          is_null = false; // $-1 or *-1
};

} // namespace protocol
} // namespace cache

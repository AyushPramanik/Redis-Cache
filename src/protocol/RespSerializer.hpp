#pragma once

#include "RespTypes.hpp"
#include <string>
#include <span>
#include <cstdint>

namespace cache {
namespace protocol {

// Encodes RESP values into wire-format byte strings.
// All methods return owned strings — callers write them into their send buffers.
class RespSerializer {
public:
    static std::string ok();
    static std::string error(std::string_view msg);
    static std::string null_bulk();
    static std::string bulk(std::string_view s);
    static std::string integer(int64_t n);
    static std::string simple(std::string_view s);
    static std::string array(std::span<const std::string> elements);
    static std::string nil_array();
};

} // namespace protocol
} // namespace cache

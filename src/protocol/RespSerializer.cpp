#include "RespSerializer.hpp"
#include <fmt/format.h>

namespace cache {
namespace protocol {

std::string RespSerializer::ok() {
    return "+OK\r\n";
}

std::string RespSerializer::error(std::string_view msg) {
    return fmt::format("-ERR {}\r\n", msg);
}

std::string RespSerializer::null_bulk() {
    return "$-1\r\n";
}

std::string RespSerializer::bulk(std::string_view s) {
    return fmt::format("${}\r\n{}\r\n", s.size(), s);
}

std::string RespSerializer::integer(int64_t n) {
    return fmt::format(":{}\r\n", n);
}

std::string RespSerializer::simple(std::string_view s) {
    return fmt::format("+{}\r\n", s);
}

std::string RespSerializer::array(std::span<const std::string> elements) {
    std::string out = fmt::format("*{}\r\n", elements.size());
    for (const auto& e : elements) {
        out += bulk(e);
    }
    return out;
}

std::string RespSerializer::nil_array() {
    return "*-1\r\n";
}

} // namespace protocol
} // namespace cache

#include "RespParser.hpp"
#include <charconv>
#include <stdexcept>

namespace cache {
namespace protocol {

std::optional<size_t> RespParser::find_crlf(std::string_view buf) {
    auto pos = buf.find("\r\n");
    if (pos == std::string_view::npos) return std::nullopt;
    return pos;
}

std::optional<int64_t> RespParser::parse_length(std::string_view line) {
    int64_t n = 0;
    auto [ptr, ec] = std::from_chars(line.data(), line.data() + line.size(), n);
    if (ec != std::errc{} || ptr != line.data() + line.size()) return std::nullopt;
    return n;
}

ParseResult RespParser::parse(std::string_view buf) const {
    if (buf.empty()) return {ParseStatus::Incomplete, {}, 0};

    switch (buf[0]) {
        case '+': return parse_simple_string(buf.substr(1));
        case '-': return parse_error(buf.substr(1));
        case ':': return parse_integer(buf.substr(1));
        case '$': return parse_bulk_string(buf.substr(1));
        case '*': return parse_array(buf.substr(1));
        default:
            return {ParseStatus::Error, {}, 0};
    }
}

ParseResult RespParser::parse_simple_string(std::string_view buf) const {
    auto crlf = find_crlf(buf);
    if (!crlf) return {ParseStatus::Incomplete, {}, 0};

    RespValue v;
    v.type = RespType::SimpleString;
    v.str  = std::string(buf.substr(0, *crlf));
    // +1 for the leading type byte
    return {ParseStatus::Ok, std::move(v), 1 + *crlf + 2};
}

ParseResult RespParser::parse_error(std::string_view buf) const {
    auto crlf = find_crlf(buf);
    if (!crlf) return {ParseStatus::Incomplete, {}, 0};

    RespValue v;
    v.type = RespType::Error;
    v.str  = std::string(buf.substr(0, *crlf));
    return {ParseStatus::Ok, std::move(v), 1 + *crlf + 2};
}

ParseResult RespParser::parse_integer(std::string_view buf) const {
    auto crlf = find_crlf(buf);
    if (!crlf) return {ParseStatus::Incomplete, {}, 0};

    auto len = parse_length(buf.substr(0, *crlf));
    if (!len) return {ParseStatus::Error, {}, 0};

    RespValue v;
    v.type    = RespType::Integer;
    v.integer = *len;
    return {ParseStatus::Ok, std::move(v), 1 + *crlf + 2};
}

ParseResult RespParser::parse_bulk_string(std::string_view buf) const {
    auto crlf = find_crlf(buf);
    if (!crlf) return {ParseStatus::Incomplete, {}, 0};

    auto len = parse_length(buf.substr(0, *crlf));
    if (!len) return {ParseStatus::Error, {}, 0};

    if (*len == -1) {
        // Null bulk string
        RespValue v;
        v.type    = RespType::BulkString;
        v.is_null = true;
        return {ParseStatus::Ok, std::move(v), 1 + *crlf + 2};
    }

    // Need: 1 (type) + crlf+2 (first line) + len + 2 (trailing \r\n)
    size_t header_size   = 1 + *crlf + 2;
    size_t content_size  = static_cast<size_t>(*len) + 2;
    if (buf.size() < *crlf + 2 + content_size) {
        return {ParseStatus::Incomplete, {}, 0};
    }

    RespValue v;
    v.type = RespType::BulkString;
    v.str  = std::string(buf.substr(*crlf + 2, static_cast<size_t>(*len)));
    return {ParseStatus::Ok, std::move(v), header_size + static_cast<size_t>(*len) + 2};
}

ParseResult RespParser::parse_array(std::string_view buf) const {
    auto crlf = find_crlf(buf);
    if (!crlf) return {ParseStatus::Incomplete, {}, 0};

    auto count = parse_length(buf.substr(0, *crlf));
    if (!count) return {ParseStatus::Error, {}, 0};

    if (*count == -1) {
        RespValue v;
        v.type    = RespType::Array;
        v.is_null = true;
        return {ParseStatus::Ok, std::move(v), 1 + *crlf + 2};
    }

    RespValue arr;
    arr.type = RespType::Array;
    arr.array.reserve(static_cast<size_t>(*count));

    // Offset from buf start (after the leading '*' byte)
    size_t offset = *crlf + 2;

    for (int64_t i = 0; i < *count; ++i) {
        if (offset >= buf.size()) return {ParseStatus::Incomplete, {}, 0};

        // Each element starts with its type byte; adjust view to include it
        auto sub = buf.substr(offset);
        auto res = parse(sub);
        if (res.status == ParseStatus::Incomplete) return {ParseStatus::Incomplete, {}, 0};
        if (res.status == ParseStatus::Error)      return {ParseStatus::Error, {}, 0};

        arr.array.push_back(std::move(res.value));
        offset += res.consumed;
    }

    return {ParseStatus::Ok, std::move(arr), 1 + offset};
}

} // namespace protocol
} // namespace cache

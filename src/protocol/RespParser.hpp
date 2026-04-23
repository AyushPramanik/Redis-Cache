#pragma once

#include "RespTypes.hpp"
#include <string_view>
#include <optional>
#include <cstddef>

namespace cache {
namespace protocol {

enum class ParseStatus {
    Ok,          // full value parsed
    Incomplete,  // need more data
    Error,       // protocol violation
};

struct ParseResult {
    ParseStatus    status;
    RespValue      value;
    size_t         consumed; // bytes consumed from input
};

// Stateless incremental RESP2 parser.
// Feed it a buffer; it returns how many bytes were consumed and the parsed value.
// Designed to work with partial network reads — call again with more data if Incomplete.
class RespParser {
public:
    ParseResult parse(std::string_view buf) const;

private:
    ParseResult parse_simple_string(std::string_view buf) const;
    ParseResult parse_error(std::string_view buf) const;
    ParseResult parse_integer(std::string_view buf) const;
    ParseResult parse_bulk_string(std::string_view buf) const;
    ParseResult parse_array(std::string_view buf) const;

    // Finds \r\n in buf, returns index or npos
    static std::optional<size_t> find_crlf(std::string_view buf);
    static std::optional<int64_t> parse_length(std::string_view line);
};

} // namespace protocol
} // namespace cache

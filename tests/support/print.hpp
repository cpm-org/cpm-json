// Shared by the tests: failed checks show values as JSON, and parse errors can be inspected without try blocks.
#pragma once

#include <cpm/json.hpp>
#include <cpm/test.hpp>

#include <format>
#include <optional>
#include <string>
#include <string_view>

template <>
struct cpm::test::Printer<cpm::json::Value> {
    static std::string print(const cpm::json::Value& value) {
        try {
            return cpm::json::write(value);
        } catch (const cpm::json::WriteError&) {
            return "(a value with a string that isn't UTF-8)";
        }
    }
};

template <>
struct cpm::test::Printer<cpm::json::Number> {
    static std::string print(const cpm::json::Number& number) { return cpm::json::write(cpm::json::Value(number)); }
};

template <>
struct cpm::test::Printer<cpm::json::Location> {
    static std::string print(const cpm::json::Location& where) {
        return std::format("offset {}, line {}, column {}", where.offset, where.line, where.column);
    }
};

namespace support {

// The error that parsing `text` throws, or nullopt when it parses.
inline std::optional<cpm::json::ParseError> parse_error(std::string_view text, const cpm::json::ParseOptions& options = {}) {
    cpm::json::ParseResult result = cpm::json::try_parse(text, options);
    if (const cpm::json::ParseError* error = result.error()) return *error;
    return std::nullopt;
}

// The message of the exception `f` throws, or "" when it throws nothing.
template <class F>
std::string message_of(F&& f) {
    try {
        f();
    } catch (const std::exception& error) {
        return error.what();
    }
    return "";
}

}  // namespace support

// The exceptions cpm-json throws for bad data, and where in a text a problem is (docs/spec.md §7).
#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>

namespace cpm::json {

// A place in a text: `offset` in bytes from the start, and `line` and `column` from 1. Columns count code points, so
// a tab or an "é" is one column.
struct Location {
    std::size_t offset = 0;
    std::size_t line = 1;
    std::size_t column = 1;

    friend bool operator==(const Location&, const Location&) = default;
};

// The base of every exception cpm-json throws for bad data.
class Error : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Text that isn't JSON, or JSONC when the options allow it (spec §4.4).
class ParseError : public Error {
public:
    enum class Code {
        unexpected_end,        // the text ended inside a value
        unexpected_character,  // something that can't come where it is
        invalid_number,
        number_out_of_range,   // too large for a double
        invalid_escape,        // in a string, including a lone surrogate
        invalid_utf8,
        control_character,     // a character below U+0020 in a string, unescaped
        duplicate_key,
        too_deep,              // nested deeper than ParseOptions::max_depth
        trailing_content,      // more than whitespace after the value
        unterminated_comment,
    };

    // what() is "line L, column C: <reason>".
    ParseError(Code code, Location location, std::string reason);

    Code code() const noexcept { return code_; }
    const Location& location() const noexcept { return location_; }
    const std::string& reason() const noexcept { return reason_; }

private:
    Code code_;
    Location location_;
    std::string reason_;
};

// A value that isn't of the kind asked for, or a conversion that doesn't fit (spec §7, §9.3).
class TypeError : public Error {
public:
    explicit TypeError(std::string reason);

    // `inner`, seen from one level up: `token` is the array index or member key that leads to the value that failed.
    // Conversions of containers use it to extend the path as the error passes up.
    TypeError(const TypeError& inner, std::string_view token);

    // The JSON Pointer of the value that failed, from the value being converted; "" for that value itself.
    const std::string& path() const noexcept { return path_; }
    const std::string& reason() const noexcept { return reason_; }

private:
    TypeError(std::string path, std::string reason);

    std::string path_;
    std::string reason_;
};

// A member or element that isn't there (spec §7).
class LookupError : public Error {
public:
    using Error::Error;
};

// A JSON Pointer that isn't well formed (spec §8).
class PointerError : public Error {
public:
    using Error::Error;
};

// Text that can't be written (spec §5).
class WriteError : public Error {
public:
    using Error::Error;
};

}  // namespace cpm::json

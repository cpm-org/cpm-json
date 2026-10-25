// Numbers as JSON text, both ways (docs/spec.md §3.2, §5.2). The one part that differs by platform.
#pragma once

#include <cpm/json/number.hpp>

#include <optional>
#include <string>
#include <string_view>

namespace cpm::json::detail {

// The Number of a token that matches the JSON number grammar: integer tokens that fit in 64 bits exactly, "-0" as -0.0,
// and the rest as the nearest double. nullopt when the magnitude is too large for a double.
std::optional<Number> read_number(std::string_view token);

// Appends the JSON text of `number`: integers exactly, doubles in the shortest form that reads back as the same double,
// with ".0" added when that form would read back as an integer.
void append_number(std::string& out, const Number& number);

}  // namespace cpm::json::detail

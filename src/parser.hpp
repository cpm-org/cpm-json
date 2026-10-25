// The JSON grammar: text to events (docs/spec.md §4).
#pragma once

#include <cpm/json/handler.hpp>
#include <cpm/json/read.hpp>

#include <string_view>

namespace cpm::json::detail {

// Reads `text` as one JSON value and delivers it to `handler` as events. Throws ParseError; exceptions from the
// handler pass through.
void parse_events(std::string_view text, Handler& handler, const ParseOptions& options);

}  // namespace cpm::json::detail

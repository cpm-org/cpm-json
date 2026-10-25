// Walking a document as events (docs/spec.md §6).
#pragma once

#include <cpm/json/handler.hpp>
#include <cpm/json/value.hpp>

namespace cpm::json {

// Delivers `value` to `handler` as events: object members in their own order, or by key when `sort_keys` is set.
void emit(const Value& value, Handler& handler, bool sort_keys = false);

}  // namespace cpm::json

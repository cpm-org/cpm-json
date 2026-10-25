// JSON Pointer, RFC 6901: paths such as /servers/0/name (docs/spec.md §8).
#pragma once

#include <cpm/json/value.hpp>

#include <initializer_list>
#include <string>
#include <string_view>

namespace cpm::json {

// The value `pointer` names in `root`, or nullptr when it names nothing: a missing member, an index out of range or
// "-", or a step into a value that isn't an array or object. "" is the root. Throws PointerError when the pointer isn't
// well formed: it doesn't start with '/', or a '~' isn't followed by 0 or 1.
const Value* find(const Value& root, std::string_view pointer);
Value* find(Value& root, std::string_view pointer);

// A token as it appears in a pointer: '~' as ~0 and '/' as ~1.
std::string escape_pointer_token(std::string_view token);

// The pointer to the path `tokens`: make_pointer({"a/b", "0"}) is "/a~1b/0".
std::string make_pointer(std::initializer_list<std::string_view> tokens);

}  // namespace cpm::json

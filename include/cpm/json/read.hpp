// Reading JSON and JSONC text (docs/spec.md §4).
#pragma once

#include <cpm/json/error.hpp>
#include <cpm/json/handler.hpp>
#include <cpm/json/value.hpp>

#include <cstddef>
#include <string_view>
#include <variant>

namespace cpm::json {

// What happens to a key that an object already has.
enum class DuplicateKeys { error, keep_first, keep_last };

struct ParseOptions {
    bool comments = false;         // `// to the end of the line` and `/* ... */` (they don't nest), as whitespace
    bool trailing_commas = false;  // one comma after the last element or member
    std::size_t max_depth = 512;   // nesting of arrays and objects; deeper is the error too_deep
    // For documents. With a handler every member is reported, so only `error` changes anything there.
    DuplicateKeys duplicate_keys = DuplicateKeys::error;
};

// JSON with comments and trailing commas, as in cpm.jsonc and many editors' settings.
inline constexpr ParseOptions jsonc{.comments = true, .trailing_commas = true};

// The outcome of try_parse: a value, or the error that stopped it.
class ParseResult {
public:
    explicit ParseResult(Value value) noexcept;
    explicit ParseResult(ParseError error) noexcept;

    // True when the text parsed.
    explicit operator bool() const noexcept;

    // The value; throws the ParseError when the text didn't parse.
    const Value& value() const&;
    Value& value() &;
    Value value() &&;

    // The error, or nullptr when the text parsed.
    const ParseError* error() const noexcept;

private:
    std::variant<Value, ParseError> result_;
};

// The value of `text`. Throws ParseError when it isn't JSON.
Value parse(std::string_view text, const ParseOptions& options = {});

// Like parse, but returns the error instead of throwing it.
ParseResult try_parse(std::string_view text, const ParseOptions& options = {});

// Delivers the value of `text` to `handler` as it is read, so an error can come after some events. Exceptions the
// handler throws pass through unchanged.
void parse(std::string_view text, Handler& handler, const ParseOptions& options = {});

}  // namespace cpm::json

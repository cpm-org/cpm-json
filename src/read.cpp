#include <cpm/json/builder.hpp>
#include <cpm/json/read.hpp>

#include <utility>

#include "parser.hpp"

namespace cpm::json {

ParseResult::ParseResult(Value value) noexcept : result_(std::in_place_type<Value>, std::move(value)) {}
ParseResult::ParseResult(ParseError error) noexcept : result_(std::in_place_type<ParseError>, std::move(error)) {}

ParseResult::operator bool() const noexcept { return result_.index() == 0; }

const Value& ParseResult::value() const& {
    if (const ParseError* failure = error()) throw *failure;
    return std::get<Value>(result_);
}

Value& ParseResult::value() & {
    if (const ParseError* failure = error()) throw *failure;
    return std::get<Value>(result_);
}

Value ParseResult::value() && {
    if (const ParseError* failure = error()) throw *failure;
    return std::move(std::get<Value>(result_));
}

const ParseError* ParseResult::error() const noexcept { return std::get_if<ParseError>(&result_); }

void parse(std::string_view text, Handler& handler, const ParseOptions& options) {
    detail::parse_events(text, handler, options);
}

Value parse(std::string_view text, const ParseOptions& options) {
    // The parser reports repeated keys itself under DuplicateKeys::error, with their location.
    DocumentBuilder builder(options.duplicate_keys == DuplicateKeys::keep_first ? DuplicateKeys::keep_first
                                                                                : DuplicateKeys::keep_last);
    detail::parse_events(text, builder, options);
    return builder.take();
}

ParseResult try_parse(std::string_view text, const ParseOptions& options) {
    try {
        return ParseResult(parse(text, options));
    } catch (ParseError& error) {
        return ParseResult(std::move(error));
    }
}

}  // namespace cpm::json

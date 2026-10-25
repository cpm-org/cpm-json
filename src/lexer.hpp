// The tokens of JSON text, with whitespace and (when allowed) comments skipped (docs/spec.md §4.3).
#pragma once

#include <cpm/json/error.hpp>

#include <cstddef>
#include <string>
#include <string_view>

namespace cpm::json::detail {

enum class TokenType {
    begin_array, end_array, begin_object, end_object, colon, comma,
    string, number, literal_true, literal_false, literal_null,
    end,      // the end of the text
    invalid,  // a character no token starts with; the parser says what it expected instead
};

struct Token {
    TokenType type = TokenType::end;
    std::size_t offset = 0;  // where the token starts
    std::string_view text;   // numbers: the token; strings: what is between the quotes, escapes undecoded
    bool escaped = false;    // strings: whether `text` has escapes, so it needs decode_string
};

class Lexer {
public:
    // A leading UTF-8 byte order mark is skipped.
    Lexer(std::string_view text, bool comments) noexcept;

    // The next token. Throws ParseError for malformed strings, numbers, literals and comments, and invalid UTF-8.
    Token next();

    // Throws a ParseError at `offset`.
    [[noreturn]] void fail(ParseError::Code code, std::size_t offset, const std::string& reason) const;

    // How an error names what is at `offset`: 'x', U+00E9, or "the end of the text".
    std::string describe(std::size_t offset) const;

private:
    void skip_space();
    void skip_comment(std::size_t start);
    Token scan_string(std::size_t start);
    Token scan_number(std::size_t start);
    Token scan_literal(std::size_t start, std::string_view word, TokenType type);
    // The length of the well-formed UTF-8 sequence at `offset`; throws invalid_utf8 when there isn't one.
    std::size_t checked_sequence(std::size_t offset) const;

    std::string_view text_;
    std::size_t pos_ = 0;
    bool comments_;
};

}  // namespace cpm::json::detail

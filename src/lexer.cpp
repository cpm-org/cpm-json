#include "lexer.hpp"

#include <cstdint>
#include <format>

#include "escape.hpp"
#include "location.hpp"
#include "utf8.hpp"

namespace cpm::json::detail {

namespace {

using Code = ParseError::Code;

bool is_digit(char c) noexcept { return c >= '0' && c <= '9'; }

bool is_hex_digit(char c) noexcept { return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }

bool is_printable_ascii(char c) noexcept { return c > ' ' && c < '\x7f'; }

}  // namespace

Lexer::Lexer(std::string_view text, bool comments) noexcept : text_(text), comments_(comments) {
    if (text_.starts_with("\xEF\xBB\xBF")) pos_ = 3;
}

void Lexer::fail(Code code, std::size_t offset, const std::string& reason) const {
    throw ParseError(code, locate(text_, offset), reason);
}

std::string Lexer::describe(std::size_t offset) const {
    if (offset >= text_.size()) return "the end of the text";
    if (is_printable_ascii(text_[offset])) return std::format("'{}'", text_[offset]);
    const std::size_t length = utf8::sequence_length(text_, offset);
    const char32_t code_point =
        length != 0 ? utf8::decode(text_, offset, length) : static_cast<unsigned char>(text_[offset]);
    return std::format("U+{:04X}", static_cast<std::uint32_t>(code_point));
}

std::size_t Lexer::checked_sequence(std::size_t offset) const {
    const std::size_t length = utf8::sequence_length(text_, offset);
    if (length == 0) {
        fail(Code::invalid_utf8, offset,
             std::format("invalid UTF-8: byte 0x{:02x}", static_cast<unsigned char>(text_[offset])));
    }
    return length;
}

Token Lexer::next() {
    skip_space();
    const std::size_t start = pos_;
    if (start == text_.size()) return {TokenType::end, start, {}, false};
    auto single = [&](TokenType type) {
        ++pos_;
        return Token{type, start, text_.substr(start, 1), false};
    };
    switch (text_[start]) {
        case '[': return single(TokenType::begin_array);
        case ']': return single(TokenType::end_array);
        case '{': return single(TokenType::begin_object);
        case '}': return single(TokenType::end_object);
        case ':': return single(TokenType::colon);
        case ',': return single(TokenType::comma);
        case '"': return scan_string(start);
        case 't': return scan_literal(start, "true", TokenType::literal_true);
        case 'f': return scan_literal(start, "false", TokenType::literal_false);
        case 'n': return scan_literal(start, "null", TokenType::literal_null);
        default: break;
    }
    if (text_[start] == '-' || is_digit(text_[start])) return scan_number(start);
    checked_sequence(start);  // invalid UTF-8 is an error wherever it is
    return {TokenType::invalid, start, {}, false};
}

void Lexer::skip_space() {
    while (pos_ < text_.size()) {
        const char c = text_[pos_];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            ++pos_;
        } else if (c == '/' && comments_) {
            skip_comment(pos_);
        } else {
            return;
        }
    }
}

void Lexer::skip_comment(std::size_t start) {
    std::size_t pos = start + 1;
    if (pos == text_.size()) fail(Code::unexpected_end, pos, "expected '/' or '*' after '/', found the end of the text");
    if (text_[pos] == '/') {
        for (++pos; pos < text_.size() && text_[pos] != '\n' && text_[pos] != '\r';) {
            pos += static_cast<unsigned char>(text_[pos]) < 0x80 ? 1 : checked_sequence(pos);
        }
        pos_ = pos;
        return;
    }
    if (text_[pos] != '*') {
        fail(Code::unexpected_character, start,
             std::format("a comment starts with // or /*, but '/' is followed by {}", describe(pos)));
    }
    for (++pos; pos < text_.size();) {
        if (text_[pos] == '*' && pos + 1 < text_.size() && text_[pos + 1] == '/') {
            pos_ = pos + 2;
            return;
        }
        pos += static_cast<unsigned char>(text_[pos]) < 0x80 ? 1 : checked_sequence(pos);
    }
    fail(Code::unterminated_comment, start, "the comment is never closed: expected '*/'");
}

Token Lexer::scan_string(std::size_t start) {
    bool escaped = false;
    std::size_t pos = start + 1;
    for (;;) {
        if (pos >= text_.size()) {
            const Location opening = locate(text_, start);
            fail(Code::unexpected_end, text_.size(),
                 std::format("expected '\"' to close the string that starts at line {}, column {}", opening.line,
                             opening.column));
        }
        const auto byte = static_cast<unsigned char>(text_[pos]);
        if (byte == '"') break;
        if (byte == '\\') {
            escaped = true;
            if (pos + 1 == text_.size()) fail(Code::unexpected_end, pos + 1, "expected an escape after '\\', found the end of the text");
            switch (text_[pos + 1]) {
                case '"': case '\\': case '/': case 'b': case 'f': case 'n': case 'r': case 't': pos += 2; continue;
                case 'u': break;
                default: fail(Code::invalid_escape, pos, std::format("invalid escape: '\\' followed by {}", describe(pos + 1)));
            }
            const std::optional<char32_t> unit = read_hex4(text_, pos + 2);
            if (!unit) {
                const std::string_view digits = text_.substr(pos + 2, 4);
                bool cut_short = digits.size() < 4;
                for (char c : digits) cut_short = cut_short && is_hex_digit(c);
                if (cut_short) fail(Code::unexpected_end, text_.size(), "expected four hexadecimal digits after \\u, found the end of the text");
                fail(Code::invalid_escape, pos, "invalid escape: \\u must be followed by four hexadecimal digits");
            }
            if (is_low_surrogate(*unit)) {
                fail(Code::invalid_escape, pos, std::format("invalid escape: \\u{} is a low surrogate without a high one before it", text_.substr(pos + 2, 4)));
            }
            if (is_high_surrogate(*unit)) {
                std::optional<char32_t> low;
                if (text_.substr(pos + 6, 2) == "\\u") low = read_hex4(text_, pos + 8);
                if (!low || !is_low_surrogate(*low)) {
                    fail(Code::invalid_escape, pos, std::format("invalid escape: the high surrogate \\u{} must be followed by a low surrogate", text_.substr(pos + 2, 4)));
                }
                pos += 12;
            } else {
                pos += 6;
            }
            continue;
        }
        if (byte < 0x20) fail(Code::control_character, pos, std::format("{} must be escaped in a string", describe(pos)));
        pos += byte < 0x80 ? 1 : checked_sequence(pos);
    }
    pos_ = pos + 1;
    return {TokenType::string, start, text_.substr(start + 1, pos - start - 1), escaped};
}

Token Lexer::scan_number(std::size_t start) {
    std::size_t pos = start;
    auto digit_at = [&](std::size_t i) { return i < text_.size() && is_digit(text_[i]); };
    auto expect_digit = [&](std::size_t i, std::string_view after) {
        if (digit_at(i)) return;
        if (i >= text_.size()) fail(Code::unexpected_end, i, std::format("expected a digit after {}, found the end of the text", after));
        fail(Code::invalid_number, start, std::format("invalid number: expected a digit after {}, found {}", after, describe(i)));
    };
    if (text_[pos] == '-') expect_digit(++pos, "'-'");
    if (text_[pos] == '0') {
        if (digit_at(++pos)) fail(Code::invalid_number, start, "invalid number: a number can't have a leading zero");
    } else {
        while (digit_at(pos)) ++pos;
    }
    if (pos < text_.size() && text_[pos] == '.') {
        expect_digit(++pos, "'.'");
        while (digit_at(pos)) ++pos;
    }
    if (pos < text_.size() && (text_[pos] == 'e' || text_[pos] == 'E')) {
        ++pos;
        if (pos < text_.size() && (text_[pos] == '+' || text_[pos] == '-')) ++pos;
        expect_digit(pos, "the exponent's 'e'");
        while (digit_at(pos)) ++pos;
    }
    pos_ = pos;
    return {TokenType::number, start, text_.substr(start, pos - start), false};
}

Token Lexer::scan_literal(std::size_t start, std::string_view word, TokenType type) {
    const std::string_view here = text_.substr(start, word.size());
    if (here == word) {
        pos_ = start + word.size();
        return {type, start, here, false};
    }
    if (here.size() < word.size() && word.starts_with(here)) {
        fail(Code::unexpected_end, text_.size(), std::format("expected '{}', found the end of the text", word));
    }
    bool printable = true;
    for (char c : here) printable = printable && is_printable_ascii(c);
    fail(Code::unexpected_character, start,
         std::format("expected '{}', found {}", word, printable ? std::format("'{}'", here) : describe(start)));
}

}  // namespace cpm::json::detail

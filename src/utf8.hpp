// UTF-8: checking, decoding and encoding (RFC 3629).
#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace cpm::json::detail::utf8 {

inline constexpr std::size_t npos = static_cast<std::size_t>(-1);

// The length (1 to 4) of the well-formed sequence that starts at text[pos], or 0 when the bytes there aren't one: a
// stray continuation byte, an overlong form, an encoded surrogate, a code point above U+10FFFF, or a cut-short sequence.
std::size_t sequence_length(std::string_view text, std::size_t pos) noexcept;

// The offset of the first byte that doesn't start a well-formed sequence, or npos when all of `text` is UTF-8.
std::size_t first_invalid(std::string_view text) noexcept;

// The code point of the well-formed sequence of `length` bytes at text[pos].
char32_t decode(std::string_view text, std::size_t pos, std::size_t length) noexcept;

// Appends the encoding of `code_point`, a Unicode scalar value.
void append(std::string& out, char32_t code_point);

}  // namespace cpm::json::detail::utf8

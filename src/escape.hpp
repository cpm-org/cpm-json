// String escapes, both ways (docs/spec.md §3.3, §4.3, §5.2).
#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace cpm::json::detail {

// The code unit of the four hexadecimal digits at text[pos], or nullopt when there aren't four.
std::optional<char32_t> read_hex4(std::string_view text, std::size_t pos) noexcept;

inline bool is_high_surrogate(char32_t unit) noexcept { return unit >= 0xD800 && unit <= 0xDBFF; }
inline bool is_low_surrogate(char32_t unit) noexcept { return unit >= 0xDC00 && unit <= 0xDFFF; }

// Decodes what is between the quotes of a string token the lexer accepted (so its escapes are well formed) into `out`.
void decode_string(std::string_view raw, std::string& out);

// Appends `text` as a JSON string in double quotes. Throws WriteError, and appends nothing, when `text` isn't UTF-8.
void append_quoted(std::string& out, std::string_view text, bool escape_non_ascii);

}  // namespace cpm::json::detail

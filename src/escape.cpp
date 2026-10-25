#include "escape.hpp"

#include <cpm/json/error.hpp>

#include <format>

#include "utf8.hpp"

namespace cpm::json::detail {

namespace {

constexpr char hex_digits[] = "0123456789abcdef";

void append_unit(std::string& out, char32_t unit) {
    out += "\\u";
    for (int shift = 12; shift >= 0; shift -= 4) out += hex_digits[(unit >> shift) & 0xF];
}

}  // namespace

std::optional<char32_t> read_hex4(std::string_view text, std::size_t pos) noexcept {
    if (text.size() < pos + 4) return std::nullopt;
    char32_t unit = 0;
    for (std::size_t i = pos; i < pos + 4; ++i) {
        const char c = text[i];
        char32_t digit = 0;
        if (c >= '0' && c <= '9') digit = static_cast<char32_t>(c - '0');
        else if (c >= 'a' && c <= 'f') digit = static_cast<char32_t>(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') digit = static_cast<char32_t>(c - 'A' + 10);
        else return std::nullopt;
        unit = (unit << 4) | digit;
    }
    return unit;
}

void decode_string(std::string_view raw, std::string& out) {
    out.clear();
    out.reserve(raw.size());
    std::size_t i = 0;
    while (i < raw.size()) {
        const std::size_t backslash = raw.find('\\', i);
        out.append(raw, i, backslash == std::string_view::npos ? std::string_view::npos : backslash - i);
        if (backslash == std::string_view::npos) break;
        const char escape = raw[backslash + 1];
        i = backslash + 2;
        switch (escape) {
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'n': out += '\n'; break;
            case 'r': out += '\r'; break;
            case 't': out += '\t'; break;
            case 'u': {
                char32_t code_point = *read_hex4(raw, i);
                i += 4;
                if (is_high_surrogate(code_point)) {  // followed by \u and a low surrogate, as the lexer checked
                    const char32_t low = *read_hex4(raw, i + 2);
                    code_point = 0x10000 + ((code_point - 0xD800) << 10) + (low - 0xDC00);
                    i += 6;
                }
                utf8::append(out, code_point);
                break;
            }
            default: out += escape; break;  // " \ /
        }
    }
}

void append_quoted(std::string& out, std::string_view text, bool escape_non_ascii) {
    const std::size_t start = out.size();
    out += '"';
    for (std::size_t i = 0; i < text.size();) {
        const auto byte = static_cast<unsigned char>(text[i]);
        if (byte >= 0x80) {
            const std::size_t length = utf8::sequence_length(text, i);
            if (length == 0) {
                out.resize(start);
                throw WriteError(std::format("a string to write isn't valid UTF-8: byte 0x{:02x} at offset {}", byte, i));
            }
            if (escape_non_ascii) {
                const char32_t code_point = utf8::decode(text, i, length);
                if (code_point > 0xFFFF) {
                    append_unit(out, 0xD800 + ((code_point - 0x10000) >> 10));
                    append_unit(out, 0xDC00 + ((code_point - 0x10000) & 0x3FF));
                } else {
                    append_unit(out, code_point);
                }
            } else {
                out.append(text, i, length);
            }
            i += length;
            continue;
        }
        switch (byte) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (byte < 0x20) append_unit(out, byte);
                else out += static_cast<char>(byte);
        }
        ++i;
    }
    out += '"';
}

}  // namespace cpm::json::detail

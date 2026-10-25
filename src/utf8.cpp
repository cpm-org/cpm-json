#include "utf8.hpp"

namespace cpm::json::detail::utf8 {

std::size_t sequence_length(std::string_view text, std::size_t pos) noexcept {
    auto byte = [&](std::size_t i) { return static_cast<unsigned char>(text[pos + i]); };
    auto continues = [&](std::size_t i, unsigned char low = 0x80, unsigned char high = 0xBF) {
        return byte(i) >= low && byte(i) <= high;
    };
    const std::size_t left = text.size() - pos;
    const unsigned char lead = byte(0);
    if (lead < 0x80) return 1;
    if (lead >= 0xC2 && lead <= 0xDF) return left >= 2 && continues(1) ? 2 : 0;
    if (lead >= 0xE0 && lead <= 0xEF) {
        // E0 would be overlong below A0; ED would encode a surrogate from A0.
        const unsigned char low = lead == 0xE0 ? 0xA0 : 0x80;
        const unsigned char high = lead == 0xED ? 0x9F : 0xBF;
        return left >= 3 && continues(1, low, high) && continues(2) ? 3 : 0;
    }
    if (lead >= 0xF0 && lead <= 0xF4) {
        // F0 would be overlong below 90; F4 would pass U+10FFFF from 90.
        const unsigned char low = lead == 0xF0 ? 0x90 : 0x80;
        const unsigned char high = lead == 0xF4 ? 0x8F : 0xBF;
        return left >= 4 && continues(1, low, high) && continues(2) && continues(3) ? 4 : 0;
    }
    return 0;
}

std::size_t first_invalid(std::string_view text) noexcept {
    for (std::size_t pos = 0; pos < text.size();) {
        std::size_t length = sequence_length(text, pos);
        if (length == 0) return pos;
        pos += length;
    }
    return npos;
}

char32_t decode(std::string_view text, std::size_t pos, std::size_t length) noexcept {
    auto byte = [&](std::size_t i) { return static_cast<char32_t>(static_cast<unsigned char>(text[pos + i])); };
    switch (length) {
        case 1: return byte(0);
        case 2: return ((byte(0) & 0x1F) << 6) | (byte(1) & 0x3F);
        case 3: return ((byte(0) & 0x0F) << 12) | ((byte(1) & 0x3F) << 6) | (byte(2) & 0x3F);
        default:
            return ((byte(0) & 0x07) << 18) | ((byte(1) & 0x3F) << 12) | ((byte(2) & 0x3F) << 6) | (byte(3) & 0x3F);
    }
}

void append(std::string& out, char32_t code_point) {
    auto put = [&](char32_t bits) { out += static_cast<char>(bits); };
    if (code_point < 0x80) {
        put(code_point);
    } else if (code_point < 0x800) {
        put(0xC0 | (code_point >> 6));
        put(0x80 | (code_point & 0x3F));
    } else if (code_point < 0x10000) {
        put(0xE0 | (code_point >> 12));
        put(0x80 | ((code_point >> 6) & 0x3F));
        put(0x80 | (code_point & 0x3F));
    } else {
        put(0xF0 | (code_point >> 18));
        put(0x80 | ((code_point >> 12) & 0x3F));
        put(0x80 | ((code_point >> 6) & 0x3F));
        put(0x80 | (code_point & 0x3F));
    }
}

}  // namespace cpm::json::detail::utf8

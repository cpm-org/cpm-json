#include "location.hpp"

#include <algorithm>

namespace cpm::json::detail {

Location locate(std::string_view text, std::size_t offset) noexcept {
    Location where;
    where.offset = offset;
    const std::size_t end = std::min(offset, text.size());
    for (std::size_t i = 0; i < end; ++i) {
        const char c = text[i];
        const bool line_end = c == '\n' || (c == '\r' && (i + 1 >= text.size() || text[i + 1] != '\n'));
        if (line_end) {
            ++where.line;
            where.column = 1;
        } else if (c != '\r' && (static_cast<unsigned char>(c) & 0xC0) != 0x80) {
            ++where.column;  // the CR of a CR LF takes no column, and continuation bytes belong to their code point
        }
    }
    return where;
}

}  // namespace cpm::json::detail

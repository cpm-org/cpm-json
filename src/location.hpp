// Byte offsets as lines and columns, for error messages.
#pragma once

#include <cpm/json/error.hpp>

#include <cstddef>
#include <string_view>

namespace cpm::json::detail {

// Where `offset` is in `text`. Lines end at LF, CR LF or a lone CR. Columns count code points (bytes that aren't UTF-8
// continuation bytes), so they match what editors show.
Location locate(std::string_view text, std::size_t offset) noexcept;

}  // namespace cpm::json::detail

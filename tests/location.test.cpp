// Byte offsets as lines and columns (src/location).

#include <cpm/test.hpp>

#include "location.hpp"
#include "support/print.hpp"

using cpm::json::Location;
using cpm::json::detail::locate;

TEST_CASE("lines and columns start at 1") {
    CHECK(locate("abc", 0) == (Location{0, 1, 1}));
    CHECK(locate("abc", 2) == (Location{2, 1, 3}));
    CHECK(locate("abc", 3) == (Location{3, 1, 4}));  // the end of the text
}

TEST_CASE("lines end at LF, CR LF and a lone CR") {
    CHECK(locate("a\nb", 2) == (Location{2, 2, 1}));
    CHECK(locate("a\r\nb", 3) == (Location{3, 2, 1}));
    CHECK(locate("a\rb", 2) == (Location{2, 2, 1}));
    CHECK(locate("a\n\r\n\rb", 5) == (Location{5, 4, 1}));
}

TEST_CASE("columns count code points") {
    CHECK(locate("\xC3\xA9x", 2) == (Location{2, 1, 2}));             // é is one column
    CHECK(locate("\xF0\x9F\x98\x80x", 4) == (Location{4, 1, 2}));     // so is an emoji
    CHECK(locate("\tx", 1) == (Location{1, 1, 2}));                   // and a tab
}

TEST_CASE("an offset past the end counts the whole text") {
    CHECK(locate("a\nb", 10) == (Location{10, 2, 2}));
}

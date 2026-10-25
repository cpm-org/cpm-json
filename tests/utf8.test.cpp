// UTF-8 checking, decoding and encoding (src/utf8).

#include <cpm/test.hpp>

#include <string>

#include "utf8.hpp"

namespace utf8 = cpm::json::detail::utf8;

TEST_CASE("well-formed sequences have their length") {
    CHECK(utf8::sequence_length("A", 0) == 1);
    CHECK(utf8::sequence_length("\x7F", 0) == 1);
    CHECK(utf8::sequence_length("\xC2\x80", 0) == 2);          // U+0080
    CHECK(utf8::sequence_length("\xDF\xBF", 0) == 2);          // U+07FF
    CHECK(utf8::sequence_length("\xE0\xA0\x80", 0) == 3);      // U+0800
    CHECK(utf8::sequence_length("\xED\x9F\xBF", 0) == 3);      // U+D7FF, just below the surrogates
    CHECK(utf8::sequence_length("\xEE\x80\x80", 0) == 3);      // U+E000, just above them
    CHECK(utf8::sequence_length("\xEF\xBF\xBF", 0) == 3);      // U+FFFF
    CHECK(utf8::sequence_length("\xF0\x90\x80\x80", 0) == 4);  // U+10000
    CHECK(utf8::sequence_length("\xF4\x8F\xBF\xBF", 0) == 4);  // U+10FFFF
}

TEST_CASE("malformed sequences have length zero") {
    CHECK(utf8::sequence_length("\x80", 0) == 0);                   // a stray continuation byte
    CHECK(utf8::sequence_length("\xC0\x80", 0) == 0);               // overlong NUL
    CHECK(utf8::sequence_length("\xC1\xBF", 0) == 0);               // overlong
    CHECK(utf8::sequence_length("\xE0\x9F\xBF", 0) == 0);           // overlong
    CHECK(utf8::sequence_length("\xED\xA0\x80", 0) == 0);           // U+D800, a surrogate
    CHECK(utf8::sequence_length("\xF0\x8F\xBF\xBF", 0) == 0);       // overlong
    CHECK(utf8::sequence_length("\xF4\x90\x80\x80", 0) == 0);       // above U+10FFFF
    CHECK(utf8::sequence_length("\xF5\x80\x80\x80", 0) == 0);       // no such lead byte
    CHECK(utf8::sequence_length("\xFF", 0) == 0);
    CHECK(utf8::sequence_length("\xE2\x82", 0) == 0);               // cut short
    CHECK(utf8::sequence_length("\xE2\x28\xA1", 0) == 0);           // not a continuation
}

TEST_CASE("first_invalid finds the first bad byte") {
    CHECK(utf8::first_invalid("plain ascii") == utf8::npos);
    CHECK(utf8::first_invalid("caf\xC3\xA9 \xE2\x82\xAC \xF0\x9F\x98\x80") == utf8::npos);
    CHECK(utf8::first_invalid("ab\xFF" "cd") == 2);
    CHECK(utf8::first_invalid("\xC3\xA9\xC3") == 2);
    CHECK(utf8::first_invalid("") == utf8::npos);
}

TEST_CASE("encoding and decoding round-trip at the boundaries") {
    for (char32_t code_point : {U'\0', U'\x7F', U'\x80', U'\x7FF', U'\x800', U'\xD7FF', U'\xE000', U'\xFFFF',
                                U'\x10000', U'\x10FFFF'}) {
        INFO("U+{:04X}", static_cast<unsigned>(code_point));
        std::string text;
        utf8::append(text, code_point);
        const std::size_t length = utf8::sequence_length(text, 0);
        CHECK(length == text.size());
        CHECK(utf8::decode(text, 0, length) == code_point);
    }
}

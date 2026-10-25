// Numbers (docs/spec.md §3.2): representations, conversions, equality, and the text both ways (src/number_text).

#include <cpm/test.hpp>

#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>

#include "number_text.hpp"
#include "support/print.hpp"

using cpm::json::Number;
using R = Number::Representation;
using cpm::json::detail::append_number;
using cpm::json::detail::read_number;

namespace {

std::string text_of(const Number& number) {
    std::string text;
    append_number(text, number);
    return text;
}

}  // namespace

TEST_CASE("integers keep one representation") {
    CHECK(Number(42).representation() == R::int64);
    CHECK(Number(-1).representation() == R::int64);
    CHECK(Number(42u).representation() == R::int64);  // unsigned values up to INT64_MAX are int64
    CHECK(Number(std::uint64_t{9223372036854775807u}).representation() == R::int64);
    CHECK(Number(std::uint64_t{9223372036854775808u}).representation() == R::uint64);
    CHECK(Number(1.5).representation() == R::float64);
    CHECK(Number().representation() == R::int64);
    CHECK(Number() == Number(0));
}

TEST_CASE("NaN and infinity aren't numbers") {
    CHECK_THROWS_AS(Number(std::numeric_limits<double>::quiet_NaN()), std::domain_error);
    CHECK_THROWS_AS(Number(std::numeric_limits<double>::infinity()), std::domain_error);
    CHECK_THROWS_AS(Number(-std::numeric_limits<float>::infinity()), std::domain_error);
}

TEST_CASE("to_integer gives whole numbers in range") {
    CHECK(Number(200).to_integer<std::uint8_t>() == std::optional<std::uint8_t>(200));
    CHECK(!Number(256).to_integer<std::uint8_t>().has_value());
    CHECK(!Number(-1).to_integer<unsigned>().has_value());
    CHECK(Number(-128).to_integer<std::int8_t>() == std::optional<std::int8_t>(-128));
    CHECK(Number(3.0).to_integer<int>() == std::optional<int>(3));   // a whole-valued double counts
    CHECK(!Number(3.5).to_integer<int>().has_value());
    CHECK(Number(-0.0).to_integer<int>() == std::optional<int>(0));
    CHECK(Number(9223372036854775808.0).to_integer<std::uint64_t>() == std::optional<std::uint64_t>(9223372036854775808u));
    CHECK(!Number(9223372036854775808.0).to_integer<std::int64_t>().has_value());
    CHECK(!Number(1e30).to_integer<std::uint64_t>().has_value());
    CHECK(Number(std::numeric_limits<std::uint64_t>::max()).to_integer<std::uint64_t>() ==
          std::optional<std::uint64_t>(std::numeric_limits<std::uint64_t>::max()));
}

TEST_CASE("to_double gives the nearest double") {
    CHECK(Number(3).to_double() == 3.0);
    CHECK(Number(std::numeric_limits<std::uint64_t>::max()).to_double() == 18446744073709551616.0);
    CHECK(Number(0.25).to_double() == 0.25);
}

TEST_CASE("equal numbers have the same value, whatever the representation") {
    CHECK(Number(1) == Number(1.0));
    CHECK(Number(1.0) == Number(1));
    CHECK(Number(-0.0) == Number(0));
    CHECK(Number(0.5) != Number(0));
    CHECK(Number(std::uint64_t{18446744073709551615u}) != Number(18446744073709551616.0));  // 2^64 isn't UINT64_MAX
    CHECK(Number(std::uint64_t{9223372036854775808u}) == Number(9223372036854775808.0));
    CHECK(Number(-1) != Number(std::uint64_t{18446744073709551615u}));
}

TEST_CASE("integer tokens read exactly") {
    CHECK(read_number("0")->representation() == R::int64);
    CHECK(*read_number("9223372036854775807") == Number(std::numeric_limits<std::int64_t>::max()));
    CHECK(*read_number("-9223372036854775808") == Number(std::numeric_limits<std::int64_t>::min()));
    CHECK(read_number("9223372036854775808")->representation() == R::uint64);
    CHECK(*read_number("18446744073709551615") == Number(std::numeric_limits<std::uint64_t>::max()));

    SUBCASE("too large for 64 bits: the nearest double") {
        CHECK(read_number("18446744073709551616")->representation() == R::float64);
        CHECK(read_number("18446744073709551616")->to_double() == 18446744073709551616.0);
        CHECK(read_number("-9223372036854775809")->representation() == R::float64);
    }
    SUBCASE("-0 keeps its sign as a double") {
        const Number zero = *read_number("-0");
        CHECK(zero.representation() == R::float64);
        CHECK(std::signbit(zero.to_double()));
    }
}

TEST_CASE("other tokens read as the nearest double") {
    CHECK(read_number("0.1")->to_double() == 0.1);
    CHECK(read_number("1e2")->representation() == R::float64);
    CHECK(read_number("1E+2")->to_double() == 100.0);
    CHECK(read_number("-2.5e-3")->to_double() == -0.0025);
    CHECK(read_number("1.7976931348623157e308")->to_double() == std::numeric_limits<double>::max());
    CHECK(read_number("4.9e-324")->to_double() == std::numeric_limits<double>::denorm_min());

    SUBCASE("too large for a double: nothing") {
        CHECK(!read_number("1e309").has_value());
        CHECK(!read_number("-1e309").has_value());
        CHECK(!read_number("123456789e301").has_value());
    }
    SUBCASE("too small for a double: zero, with its sign") {
        CHECK(read_number("1e-400")->to_double() == 0.0);
        CHECK(std::signbit(read_number("-1e-400")->to_double()));
        CHECK(read_number("0.000001e-330")->to_double() == 0.0);
    }
}

TEST_CASE("numbers are written exactly, doubles in their shortest form") {
    CHECK(text_of(Number(0)) == "0");
    CHECK(text_of(Number(-42)) == "-42");
    CHECK(text_of(Number(std::numeric_limits<std::int64_t>::min())) == "-9223372036854775808");
    CHECK(text_of(Number(std::numeric_limits<std::uint64_t>::max())) == "18446744073709551615");
    CHECK(text_of(Number(0.1)) == "0.1");
    CHECK(text_of(Number(1.5)) == "1.5");
    CHECK(text_of(Number(1e300)) == "1e+300");
    CHECK(text_of(Number(5e-324)) == "5e-324");

    SUBCASE("a double that would read back as an integer gets .0") {
        CHECK(text_of(Number(1.0)) == "1.0");
        CHECK(text_of(Number(100.0)) == "100.0");
        CHECK(text_of(Number(-0.0)) == "-0.0");
        CHECK(read_number(text_of(Number(100.0)))->representation() == R::float64);
    }
}

TEST_CASE("doubles round-trip through their text") {
    std::mt19937_64 random(20261025);
    for (int i = 0; i < 20000; ++i) {
        const double value = std::bit_cast<double>(random());
        if (!std::isfinite(value)) continue;
        const std::string text = text_of(Number(value));
        INFO("{}", text);
        const std::optional<Number> back = read_number(text);
        REQUIRE(back.has_value());
        CHECK(std::bit_cast<std::uint64_t>(back->to_double()) == std::bit_cast<std::uint64_t>(value));
    }
}

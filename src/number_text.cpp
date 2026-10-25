#include "number_text.hpp"

#include <charconv>
#include <cstdint>
#include <system_error>

#if !defined(__cpp_lib_to_chars)
#include <clocale>
#include <cmath>
#include <cstdlib>
#if defined(__APPLE__)
#include <xlocale.h>
#endif
#endif

namespace cpm::json::detail {

namespace {

#if defined(__cpp_lib_to_chars)

// Whether a token that from_chars found out of range for a double is too large rather than too small: the decimal
// exponent of its first significant digit is above zero. (Only called for out-of-range tokens, so the magnitude is
// far from 1 either way, and a saturated exponent still decides it.)
bool too_large(std::string_view token) {
    std::size_t i = token.front() == '-' ? 1 : 0;
    long long order = 0;  // the value is 0.d1d2... x 10^order, with d1 its first significant digit
    bool significant = false;
    bool fraction = false;
    for (; i < token.size() && token[i] != 'e' && token[i] != 'E'; ++i) {
        if (token[i] == '.') {
            fraction = true;
        } else if (!fraction && (significant || token[i] != '0')) {
            significant = true;
            ++order;
        } else if (fraction && !significant) {
            if (token[i] == '0') --order;
            else significant = true;
        }
    }
    long long exponent = 0;
    if (i < token.size()) {
        bool negative = token[++i] == '-';
        if (token[i] == '+' || token[i] == '-') ++i;
        for (; i < token.size() && exponent < 1'000'000'000; ++i) exponent = exponent * 10 + (token[i] - '0');
        if (negative) exponent = -exponent;
    }
    return order + exponent > 0;
}

// The nearest double; nullopt when the magnitude is too large. Values too small for a double round to zero.
std::optional<double> parse_double(std::string_view token) {
    double value = 0;
    auto [end, error] = std::from_chars(token.data(), token.data() + token.size(), value);
    if (error == std::errc()) return value;
    if (error == std::errc::result_out_of_range && !too_large(token)) return token.front() == '-' ? -0.0 : 0.0;
    return std::nullopt;
}

#else

// Without floating-point from_chars (Apple's libc++), strtod in the "C" locale, so the program's locale can't change
// what a '.' means.
std::optional<double> parse_double(std::string_view token) {
    const std::string copy(token);
    char* end = nullptr;
#if defined(_WIN32)
    static const _locale_t c_locale = _create_locale(LC_ALL, "C");
    const double value = _strtod_l(copy.c_str(), &end, c_locale);
#else
    static const locale_t c_locale = newlocale(LC_ALL_MASK, "C", static_cast<locale_t>(nullptr));
    const double value = strtod_l(copy.c_str(), &end, c_locale);
#endif
    if (std::isinf(value)) return std::nullopt;
    return value;
}

#endif

template <class T>
std::optional<T> parse_integer(std::string_view token) {
    T value = 0;
    auto [end, error] = std::from_chars(token.data(), token.data() + token.size(), value);
    if (error == std::errc() && end == token.data() + token.size()) return value;
    return std::nullopt;
}

}  // namespace

std::optional<Number> read_number(std::string_view token) {
    if (token.find_first_of(".eE") == std::string_view::npos) {
        if (token == "-0") return Number(-0.0);
        if (token.front() == '-') {
            if (auto value = parse_integer<std::int64_t>(token)) return Number(*value);
        } else if (auto value = parse_integer<std::uint64_t>(token)) {
            return Number(*value);
        }
        // Too large for 64 bits: the nearest double, below.
    }
    if (std::optional<double> value = parse_double(token)) return Number(*value);
    return std::nullopt;
}

void append_number(std::string& out, const Number& number) {
    char text[32];
    std::to_chars_result result{};
    switch (number.representation()) {
        case Number::Representation::int64:
            result = std::to_chars(text, text + sizeof text, *number.to_integer<std::int64_t>());
            break;
        case Number::Representation::uint64:
            result = std::to_chars(text, text + sizeof text, *number.to_integer<std::uint64_t>());
            break;
        case Number::Representation::float64: {
            result = std::to_chars(text, text + sizeof text, number.to_double());
            const std::string_view shortest(text, static_cast<std::size_t>(result.ptr - text));
            out += shortest;
            if (shortest.find_first_of(".e") == std::string_view::npos) out += ".0";
            return;
        }
    }
    out.append(text, result.ptr);
}

}  // namespace cpm::json::detail

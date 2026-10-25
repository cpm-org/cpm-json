// JSON numbers: exact 64-bit integers, or finite doubles (docs/spec.md §3.2).
#pragma once

#include <cmath>
#include <concepts>
#include <cstdint>
#include <limits>
#include <optional>
#include <type_traits>
#include <utility>

namespace cpm::json {

namespace detail {

template <class T>
concept character = std::same_as<std::remove_cv_t<T>, char> || std::same_as<std::remove_cv_t<T>, wchar_t> ||
                    std::same_as<std::remove_cv_t<T>, char8_t> || std::same_as<std::remove_cv_t<T>, char16_t> ||
                    std::same_as<std::remove_cv_t<T>, char32_t>;

// The integer types JSON numbers convert to and from: not bool, and not the character types.
template <class T>
concept integer = std::integral<T> && !std::same_as<std::remove_cv_t<T>, bool> && !character<T>;

// `value` itself; throws std::domain_error when it is NaN or infinite.
double finite(double value);

}  // namespace detail

class Number {
public:
    enum class Representation { int64, uint64, float64 };

    // Zero.
    constexpr Number() noexcept = default;

    // An integer. Unsigned values up to INT64_MAX are stored as int64, so equal integers have one representation.
    template <detail::integer T>
    constexpr Number(T value) noexcept {
        if constexpr (std::is_signed_v<T>) {
            int64_ = value;
        } else if (std::in_range<std::int64_t>(value)) {
            int64_ = static_cast<std::int64_t>(value);
        } else {
            representation_ = Representation::uint64;
            uint64_ = value;
        }
    }

    // A double. Throws std::domain_error for NaN and infinity, which JSON can't represent.
    template <std::floating_point T>
    Number(T value)
        : representation_(Representation::float64), float64_(detail::finite(static_cast<double>(value))) {}

    Representation representation() const noexcept { return representation_; }

    // The value as T, when it is a whole number in T's range (a whole-valued double included); else nullopt.
    template <detail::integer T>
    std::optional<T> to_integer() const noexcept;

    // The nearest double.
    double to_double() const noexcept;

    // Equal when the mathematical values are: Number(1) == Number(1.0), and Number(-0.0) == Number(0).
    friend bool operator==(const Number& a, const Number& b) noexcept;

private:
    Representation representation_ = Representation::int64;
    union {
        std::int64_t int64_ = 0;
        std::uint64_t uint64_;
        double float64_;
    };
};

template <detail::integer T>
std::optional<T> Number::to_integer() const noexcept {
    auto fit = [](auto value) -> std::optional<T> {
        if (std::in_range<T>(value)) return static_cast<T>(value);
        return std::nullopt;
    };
    switch (representation_) {
        case Representation::int64: return fit(int64_);
        case Representation::uint64: return fit(uint64_);
        case Representation::float64: {
            // Exactly representable bounds: -2^63, 2^63 and 2^64.
            constexpr double int64_min = -9223372036854775808.0;
            constexpr double int64_end = 9223372036854775808.0;
            constexpr double uint64_end = 18446744073709551616.0;
            if (std::trunc(float64_) != float64_) return std::nullopt;
            if (float64_ >= int64_min && float64_ < int64_end) return fit(static_cast<std::int64_t>(float64_));
            if (float64_ >= 0 && float64_ < uint64_end) return fit(static_cast<std::uint64_t>(float64_));
            return std::nullopt;
        }
    }
    return std::nullopt;
}

inline double Number::to_double() const noexcept {
    switch (representation_) {
        case Representation::int64: return static_cast<double>(int64_);
        case Representation::uint64: return static_cast<double>(uint64_);
        case Representation::float64: return float64_;
    }
    return 0;
}

}  // namespace cpm::json

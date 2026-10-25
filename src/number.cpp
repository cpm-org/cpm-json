#include <cpm/json/number.hpp>

#include <cmath>
#include <stdexcept>

namespace cpm::json {

namespace detail {

double finite(double value) {
    if (!std::isfinite(value)) throw std::domain_error("cpm::json::Number: JSON numbers can't be NaN or infinite");
    return value;
}

}  // namespace detail

bool operator==(const Number& a, const Number& b) noexcept {
    using R = Number::Representation;
    if (a.representation_ == b.representation_) {
        switch (a.representation_) {
            case R::int64: return a.int64_ == b.int64_;
            case R::uint64: return a.uint64_ == b.uint64_;
            case R::float64: return a.float64_ == b.float64_;
        }
    }
    // An integer and a double are equal when the double is that whole number. int64 and uint64 never overlap.
    const Number& d = a.representation_ == R::float64 ? a : b;
    const Number& i = a.representation_ == R::float64 ? b : a;
    if (d.representation_ != R::float64) return false;
    if (i.representation_ == R::int64) return d.to_integer<std::int64_t>() == i.int64_;
    return d.to_integer<std::uint64_t>() == i.uint64_;
}

}  // namespace cpm::json

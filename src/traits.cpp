#include <cpm/json/traits.hpp>

#include <format>

namespace cpm::json::detail {

void expect_array_of(const Value& json, std::size_t size) {
    const std::size_t found = json.as_array().size();
    if (found != size) {
        throw TypeError(std::format("expected an array of {} element{}, found {} element{}", size, size == 1 ? "" : "s",
                                    found, found == 1 ? "" : "s"));
    }
}

void expected_integer(const Value& json, std::string_view min, std::string_view max) {
    throw TypeError(std::format("expected an integer from {} to {}, found {}", min, max, describe(json)));
}

void float_out_of_range(const Value& json) {
    throw TypeError(std::format("expected a number that fits in a float, found {}", describe(json)));
}

}  // namespace cpm::json::detail

// Conversions between JSON and C++ types (docs/spec.md §9).
//
// A type takes part by specialising Traits with either or both of
//     static Value to_json(const T& value);
//     static T from_json(const Value& json);
// to_json(value) and from_json<T>(json) (and Value::get<T>()) use them. A TypeError thrown while converting an
// element or member is rethrown with that element's index or member's key added to its path(), so the error says
// where in the document the conversion failed.
#pragma once

#include <cpm/json/value.hpp>

#include <array>
#include <concepts>
#include <cstddef>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace cpm::json {

// No definition: a type without a specialisation doesn't convert.
template <class T>
struct Traits;

template <class T>
concept writable = requires(const T& value) {
    { Traits<T>::to_json(value) } -> std::convertible_to<Value>;
};

template <class T>
concept readable = requires(const Value& json) {
    { Traits<T>::from_json(json) } -> std::convertible_to<T>;
};

template <writable T>
Value to_json(const T& value) {
    return Traits<T>::to_json(value);
}

template <readable T>
T from_json(const Value& json) {
    return Traits<T>::from_json(json);
}

template <class T>
T Value::get() const {
    return json::from_json<T>(*this);
}

namespace detail {

// `convert()`, with `token` added to the path of a TypeError it throws.
template <class F>
decltype(auto) at_path(std::string_view token, F&& convert) {
    try {
        return std::forward<F>(convert)();
    } catch (const TypeError& error) {
        throw TypeError(error, token);
    }
}

// Throws TypeError unless `json` is an array of `size` elements.
void expect_array_of(const Value& json, std::size_t size);

// Throws TypeError("expected an integer from <min> to <max>, found ...").
[[noreturn]] void expected_integer(const Value& json, std::string_view min, std::string_view max);
[[noreturn]] void float_out_of_range(const Value& json);

template <class Map, class T>
struct MapTraits {
    static Value to_json(const Map& map)
        requires writable<T>
    {
        Object object;
        for (const auto& [key, value] : map) object.insert_or_assign(key, json::to_json(value));
        return object;
    }
    static Map from_json(const Value& json)
        requires readable<T>
    {
        Map map;
        for (const Member& member : json.as_object()) {
            map.emplace(member.key(), at_path(member.key(), [&] { return json::from_json<T>(member.value); }));
        }
        return map;
    }
};

template <class Tuple, std::size_t... I>
Value tuple_to_json(const Tuple& tuple, std::index_sequence<I...>) {
    return Array{json::to_json(std::get<I>(tuple))...};
}

template <class Tuple, std::size_t... I>
Tuple tuple_from_json(const Value& json, std::index_sequence<I...>) {
    expect_array_of(json, sizeof...(I));
    [[maybe_unused]] const Array& array = json.as_array();  // unused for empty tuples
    return Tuple{at_path(std::to_string(I), [&] {
        return json::from_json<std::tuple_element_t<I, Tuple>>(array[I]);
    })...};
}

}  // namespace detail

// The data model itself.

template <>
struct Traits<Value> {
    static Value to_json(const Value& value) { return value; }
    static Value from_json(const Value& json) { return json; }
};

template <>
struct Traits<Array> {
    static Value to_json(const Array& array) { return array; }
    static Array from_json(const Value& json) { return json.as_array(); }
};

template <>
struct Traits<Object> {
    static Value to_json(const Object& object) { return object; }
    static Object from_json(const Value& json) { return json.as_object(); }
};

template <>
struct Traits<Number> {
    static Value to_json(const Number& number) { return number; }
    static Number from_json(const Value& json) { return json.as_number(); }
};

// Scalars.

template <>
struct Traits<bool> {
    static Value to_json(bool value) { return value; }
    static bool from_json(const Value& json) { return json.as_boolean(); }
};

// Reading needs a whole number in T's range.
template <detail::integer T>
struct Traits<T> {
    static Value to_json(T value) { return value; }
    static T from_json(const Value& json) {
        if (json.is_number()) {
            if (std::optional<T> value = json.as_number().template to_integer<T>()) return *value;
        }
        detail::expected_integer(json, std::to_string(+std::numeric_limits<T>::min()),
                                 std::to_string(+std::numeric_limits<T>::max()));
    }
};

// Reading takes any number; a float that would overflow is a TypeError.
template <std::floating_point T>
struct Traits<T> {
    static Value to_json(T value) { return value; }
    static T from_json(const Value& json) {
        double value = json.as_number().to_double();
        if constexpr (std::numeric_limits<T>::max() < std::numeric_limits<double>::max()) {
            if (value > std::numeric_limits<T>::max() || value < std::numeric_limits<T>::lowest())
                detail::float_out_of_range(json);
        }
        return static_cast<T>(value);
    }
};

template <>
struct Traits<std::string> {
    static Value to_json(const std::string& value) { return value; }
    static std::string from_json(const Value& json) { return json.as_string(); }
};

// Writing only.
template <>
struct Traits<std::string_view> {
    static Value to_json(std::string_view value) { return value; }
};

template <>
struct Traits<const char*> {
    static Value to_json(const char* value) { return value; }
};

template <std::size_t N>
struct Traits<char[N]> {
    static Value to_json(const char (&value)[N]) { return std::string_view(value); }
};

template <>
struct Traits<std::nullptr_t> {
    static Value to_json(std::nullptr_t) { return nullptr; }
    static std::nullptr_t from_json(const Value& json) {
        if (!json.is_null()) detail::kind_mismatch("null", json);
        return nullptr;
    }
};

// Standard containers.

// T, or null for nullopt.
template <class T>
struct Traits<std::optional<T>> {
    static Value to_json(const std::optional<T>& value)
        requires writable<T>
    {
        return value ? json::to_json(*value) : Value();
    }
    static std::optional<T> from_json(const Value& json)
        requires readable<T>
    {
        if (json.is_null()) return std::nullopt;
        return json::from_json<T>(json);
    }
};

template <class T, class Allocator>
struct Traits<std::vector<T, Allocator>> {
    static Value to_json(const std::vector<T, Allocator>& vector)
        requires writable<T>
    {
        Array array;
        array.reserve(vector.size());
        // to_json<T> converts std::vector<bool>'s element proxies.
        for (const auto& element : vector) array.push_back(json::to_json<T>(element));
        return array;
    }
    static std::vector<T, Allocator> from_json(const Value& json)
        requires readable<T>
    {
        const Array& array = json.as_array();
        std::vector<T, Allocator> vector;
        vector.reserve(array.size());
        for (std::size_t i = 0; i < array.size(); ++i) {
            vector.push_back(detail::at_path(std::to_string(i), [&] { return json::from_json<T>(array[i]); }));
        }
        return vector;
    }
};

// Exactly N elements.
template <class T, std::size_t N>
struct Traits<std::array<T, N>> {
    static Value to_json(const std::array<T, N>& elements)
        requires writable<T>
    {
        Array array;
        array.reserve(N);
        for (const T& element : elements) array.push_back(json::to_json(element));
        return array;
    }
    static std::array<T, N> from_json(const Value& json)
        requires readable<T>
    {
        return detail::tuple_from_json<std::array<T, N>>(json, std::make_index_sequence<N>());
    }
};

// Arrays of exactly that many elements.
template <class A, class B>
struct Traits<std::pair<A, B>> {
    static Value to_json(const std::pair<A, B>& pair)
        requires writable<A> && writable<B>
    {
        return Array{json::to_json(pair.first), json::to_json(pair.second)};
    }
    static std::pair<A, B> from_json(const Value& json)
        requires readable<A> && readable<B>
    {
        return detail::tuple_from_json<std::pair<A, B>>(json, std::make_index_sequence<2>());
    }
};

template <class... T>
struct Traits<std::tuple<T...>> {
    static Value to_json(const std::tuple<T...>& tuple)
        requires(writable<T> && ...)
    {
        return detail::tuple_to_json(tuple, std::index_sequence_for<T...>());
    }
    static std::tuple<T...> from_json(const Value& json)
        requires(readable<T> && ...)
    {
        return detail::tuple_from_json<std::tuple<T...>>(json, std::index_sequence_for<T...>());
    }
};

template <class T, class Compare, class Allocator>
struct Traits<std::map<std::string, T, Compare, Allocator>>
    : detail::MapTraits<std::map<std::string, T, Compare, Allocator>, T> {};

template <class T, class Hash, class Equal, class Allocator>
struct Traits<std::unordered_map<std::string, T, Hash, Equal, Allocator>>
    : detail::MapTraits<std::unordered_map<std::string, T, Hash, Equal, Allocator>, T> {};

}  // namespace cpm::json

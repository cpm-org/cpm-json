#include <cpm/json/value.hpp>

#include <format>
#include <utility>

#include "number_text.hpp"

namespace cpm::json {

namespace detail {

std::string describe(const Value& value) {
    switch (value.kind()) {
        case Kind::null: return "null";
        case Kind::boolean: return value.as_boolean() ? "true" : "false";
        case Kind::number: {
            std::string text;
            append_number(text, value.as_number());
            return text;
        }
        case Kind::string: return "a string";
        case Kind::array: return "an array";
        case Kind::object: return "an object";
    }
    return "a value";
}

void kind_mismatch(std::string_view expected, const Value& found) {
    throw TypeError(std::format("expected {}, found {}", expected, describe(found)));
}

}  // namespace detail

Value::Value() noexcept : data_(std::in_place_type<std::nullptr_t>, nullptr) {}
Value::Value(std::nullptr_t) noexcept : data_(std::in_place_type<std::nullptr_t>, nullptr) {}
Value::Value(bool value) noexcept : data_(std::in_place_type<bool>, value) {}
Value::Value(Number value) noexcept : data_(std::in_place_type<Number>, value) {}
Value::Value(std::string value) noexcept : data_(std::in_place_type<std::string>, std::move(value)) {}
Value::Value(std::string_view value) : data_(std::in_place_type<std::string>, value) {}
Value::Value(const char* value) : data_(std::in_place_type<std::string>, value) {}
Value::Value(Array value) noexcept : data_(std::in_place_type<Array>, std::move(value)) {}
Value::Value(Object value) noexcept : data_(std::in_place_type<Object>, std::move(value)) {}

Value::Value(const Value& other) = default;
Value::Value(Value&& other) noexcept = default;
Value& Value::operator=(const Value& other) = default;
Value& Value::operator=(Value&& other) noexcept = default;
Value::~Value() = default;

Kind Value::kind() const noexcept { return static_cast<Kind>(data_.index()); }

namespace {

// The alternative T of `data`, or a TypeError naming what was expected.
template <class T, class Data>
auto& contents(Data& data, const Value& value, std::string_view expected) {
    if (auto* p = std::get_if<T>(&data)) return *p;
    detail::kind_mismatch(expected, value);
}

}  // namespace

bool Value::as_boolean() const { return contents<bool>(data_, *this, "a boolean"); }
const Number& Value::as_number() const { return contents<Number>(data_, *this, "a number"); }
const std::string& Value::as_string() const { return contents<std::string>(data_, *this, "a string"); }
std::string& Value::as_string() { return contents<std::string>(data_, *this, "a string"); }
const Array& Value::as_array() const { return contents<Array>(data_, *this, "an array"); }
Array& Value::as_array() { return contents<Array>(data_, *this, "an array"); }
const Object& Value::as_object() const { return contents<Object>(data_, *this, "an object"); }
Object& Value::as_object() { return contents<Object>(data_, *this, "an object"); }

const Value* Value::find(std::string_view key) const { return as_object().find(key); }
Value* Value::find(std::string_view key) { return as_object().find(key); }

const Value* Value::find(std::size_t index) const {
    const Array& array = as_array();
    return index < array.size() ? &array[index] : nullptr;
}

Value* Value::find(std::size_t index) {
    Array& array = as_array();
    return index < array.size() ? &array[index] : nullptr;
}

const Value& Value::at(std::string_view key) const { return as_object().at(key); }
Value& Value::at(std::string_view key) { return as_object().at(key); }

const Value& Value::at(std::size_t index) const {
    if (const Value* element = find(index)) return *element;
    throw LookupError(std::format("index {} is out of range for an array of {} elements", index, as_array().size()));
}

Value& Value::at(std::size_t index) {
    if (Value* element = find(index)) return *element;
    throw LookupError(std::format("index {} is out of range for an array of {} elements", index, as_array().size()));
}

bool operator==(const Value& a, const Value& b) { return a.data_ == b.data_; }

}  // namespace cpm::json

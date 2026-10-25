// JSON documents: Value, and the Array and Object it can hold (docs/spec.md §3, §8).
#pragma once

#include <cpm/json/error.hpp>
#include <cpm/json/number.hpp>

#include <cstddef>
#include <initializer_list>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace cpm::json {

enum class Kind { null, boolean, number, string, array, object };

class Value;
class Member;

// A JSON array.
using Array = std::vector<Value>;

namespace detail {

// Where an object's members are by key, for objects too big to search one member at a time: an open-addressing table
// of member positions, hashed by key.
class MemberIndex {
public:
    static constexpr std::size_t npos = static_cast<std::size_t>(-1);

    bool active() const noexcept { return !slots_.empty(); }
    // The position of `key` in `members`, or npos.
    std::size_t find(std::string_view key, const std::vector<Member>& members) const noexcept;
    // Records the member just appended at `position`.
    void add(std::size_t position, const std::vector<Member>& members);
    void rebuild(const std::vector<Member>& members);
    void clear() noexcept { slots_.clear(); }

private:
    void place(std::size_t position, const std::vector<Member>& members) noexcept;

    std::vector<std::size_t> slots_;  // member positions, npos where empty; the size is a power of two
};

}  // namespace detail

// Members with unique keys, in the order they were added (spec §3.4). Finding, adding and replacing take constant time
// on average; erasing is linear.
class Object {
public:
    using iterator = Member*;
    using const_iterator = const Member*;

    Object() noexcept;
    // A repeated key keeps the last value, at the first one's position.
    Object(std::initializer_list<Member> members);
    Object(const Object& other);
    Object(Object&& other) noexcept;
    Object& operator=(const Object& other);
    Object& operator=(Object&& other) noexcept;
    ~Object();

    std::size_t size() const noexcept;
    bool empty() const noexcept;
    void clear() noexcept;

    iterator begin() noexcept;
    iterator end() noexcept;
    const_iterator begin() const noexcept;
    const_iterator end() const noexcept;

    // The member's value, or nullptr.
    Value* find(std::string_view key) noexcept;
    const Value* find(std::string_view key) const noexcept;
    bool contains(std::string_view key) const noexcept;
    // The member's value; throws LookupError when there is none.
    Value& at(std::string_view key);
    const Value& at(std::string_view key) const;

    // Adds the member at the end, or replaces the value of the existing one, which keeps its position.
    Value& insert_or_assign(std::string key, Value value);
    // Removes the member, keeping the others' order. Returns whether there was one.
    bool erase(std::string_view key);

    // The same keys with equal values, in any order.
    friend bool operator==(const Object& a, const Object& b);

private:
    std::size_t position(std::string_view key) const noexcept;
    void swap(Object& other) noexcept;

    std::vector<Member> members_;
    detail::MemberIndex index_;
};

// Any JSON value (spec §3.1). A default-constructed Value is null.
class Value {
public:
    Value() noexcept;
    Value(std::nullptr_t) noexcept;
    Value(bool value) noexcept;
    template <detail::integer T>
    Value(T value) noexcept : Value(Number(value)) {}
    // Throws std::domain_error for NaN and infinity.
    template <std::floating_point T>
    Value(T value) : Value(Number(value)) {}
    Value(Number value) noexcept;
    Value(std::string value) noexcept;
    Value(std::string_view value);
    Value(const char* value);  // a string, never a boolean
    Value(Array value) noexcept;
    Value(Object value) noexcept;

    Value(const Value& other);
    Value(Value&& other) noexcept;
    Value& operator=(const Value& other);
    Value& operator=(Value&& other) noexcept;
    ~Value();

    Kind kind() const noexcept;
    bool is_null() const noexcept { return kind() == Kind::null; }
    bool is_boolean() const noexcept { return kind() == Kind::boolean; }
    bool is_number() const noexcept { return kind() == Kind::number; }
    bool is_string() const noexcept { return kind() == Kind::string; }
    bool is_array() const noexcept { return kind() == Kind::array; }
    bool is_object() const noexcept { return kind() == Kind::object; }

    // The contents, for a value of that kind; throw TypeError for another kind.
    bool as_boolean() const;
    const Number& as_number() const;
    const std::string& as_string() const;
    std::string& as_string();
    const Array& as_array() const;
    Array& as_array();
    const Object& as_object() const;
    Object& as_object();

    // Members of objects and elements of arrays: find returns nullptr when there is nothing there, and at throws
    // LookupError. On a value of another kind, both throw TypeError.
    const Value* find(std::string_view key) const;
    Value* find(std::string_view key);
    const Value* find(std::size_t index) const;
    Value* find(std::size_t index);
    const Value& at(std::string_view key) const;
    Value& at(std::string_view key);
    const Value& at(std::size_t index) const;
    Value& at(std::size_t index);

    // The value converted to T through Traits<T> (spec §9). Defined in <cpm/json/traits.hpp>.
    template <class T>
    T get() const;

    // Same kind and equal contents (spec §3.5).
    friend bool operator==(const Value& a, const Value& b);

private:
    std::variant<std::nullptr_t, bool, Number, std::string, Array, Object> data_;
};

// A key and its value in an Object. The key can't change once the member is in an object, so members can't be
// assigned; their values can.
class Member {
public:
    Member(std::string key, Value v) : value(std::move(v)), key_(std::move(key)) {}
    Member(const Member&) = default;
    Member(Member&&) noexcept = default;
    Member& operator=(const Member&) = delete;
    Member& operator=(Member&&) = delete;
    ~Member() = default;

    const std::string& key() const noexcept { return key_; }

    Value value;

private:
    std::string key_;
};

namespace detail {

// How errors name a value: null, true, false, a number's text, or "a string", "an array", "an object".
std::string describe(const Value& value);

// Throws TypeError("expected <expected>, found <describe(found)>").
[[noreturn]] void kind_mismatch(std::string_view expected, const Value& found);

}  // namespace detail

}  // namespace cpm::json

// Value::get<T>() is defined with the conversions.
#include <cpm/json/traits.hpp>

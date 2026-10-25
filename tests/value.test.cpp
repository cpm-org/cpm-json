// Values (docs/spec.md §3.1, §3.5, §8): construction, typed access, lookup and equality.

#include <cpm/test.hpp>

#include <cstdint>
#include <string>
#include <string_view>

#include "support/print.hpp"

namespace json = cpm::json;
using json::Array;
using json::Kind;
using json::Object;
using json::Value;

TEST_CASE("values are made from C++ values") {
    CHECK(Value().kind() == Kind::null);
    CHECK(Value(nullptr).kind() == Kind::null);
    CHECK(Value(true).kind() == Kind::boolean);
    CHECK(Value(0).kind() == Kind::number);
    CHECK(Value(std::uint16_t{7}).kind() == Kind::number);
    CHECK(Value(2.5f).kind() == Kind::number);
    CHECK(Value(json::Number(3)).kind() == Kind::number);
    CHECK(Value(std::string("s")).kind() == Kind::string);
    CHECK(Value(std::string_view("s")).kind() == Kind::string);
    CHECK(Value(Array{1, "two", nullptr}).kind() == Kind::array);
    CHECK(Value(Object{{"a", 1}}).kind() == Kind::object);

    SUBCASE("character pointers make strings, never booleans") {
        const char* text = "yes";
        CHECK(Value(text).is_string());
        CHECK(Value("literal").as_string() == "literal");
    }
}

TEST_CASE("typed access returns the contents") {
    Value v = Object{{"name", "cpm"}, {"stars", 42}, {"tags", Array{"c++", "json"}}, {"ok", true}};
    CHECK(v.at("name").as_string() == "cpm");
    CHECK(v.at("stars").as_number() == json::Number(42));
    CHECK(v.at("tags").as_array().size() == 2);
    CHECK(v.at("ok").as_boolean());

    SUBCASE("and can change them") {
        v.at("name").as_string() += "-json";
        v.as_object().insert_or_assign("stars", 43);
        v.at("tags").as_array().push_back("events");
        CHECK(v == Value(Object{{"name", "cpm-json"}, {"stars", 43}, {"tags", Array{"c++", "json", "events"}}, {"ok", true}}));
    }
}

TEST_CASE("access of the wrong kind is a TypeError that says what it found") {
    CHECK_THROWS_WITH(Value(1).as_string(), "expected a string, found 1");
    CHECK_THROWS_WITH(Value("x").as_number(), "expected a number, found a string");
    CHECK_THROWS_WITH(Value().as_boolean(), "expected a boolean, found null");
    CHECK_THROWS_WITH(Value(Array{}).as_object(), "expected an object, found an array");
    CHECK_THROWS_WITH(Value(Object{}).as_array(), "expected an array, found an object");
    CHECK_THROWS_WITH(Value(false).at("key"), "expected an object, found false");
    CHECK_THROWS_WITH(Value(2.5).find(std::size_t{0}), "expected an array, found 2.5");
    CHECK_THROWS_AS(Value(1).as_string(), json::TypeError);
}

TEST_CASE("lookup finds members and elements") {
    const Value v = Object{{"list", Array{10, 20}}};
    REQUIRE(v.find("list") != nullptr);
    CHECK(v.find("missing") == nullptr);
    const Value& list = v.at("list");
    CHECK(*list.find(1) == Value(20));
    CHECK(list.find(2) == nullptr);
    CHECK(list.at(0) == Value(10));

    SUBCASE("at() of something missing is a LookupError") {
        CHECK_THROWS_WITH(v.at("missing"), "no member \"missing\"");
        CHECK_THROWS_WITH(list.at(5), "index 5 is out of range for an array of 2 elements");
        CHECK_THROWS_AS(list.at(5), json::LookupError);
    }
}

TEST_CASE("equality compares kinds and contents") {
    CHECK(Value(1) == Value(1.0));
    CHECK(Value(1) != Value(true));
    CHECK(Value() == Value(nullptr));
    CHECK(Value("a") != Value("b"));
    CHECK(Value(Array{1, 2}) != Value(Array{2, 1}));  // arrays are ordered
    CHECK(Value(Object{{"a", 1}, {"b", 2}}) == Value(Object{{"b", 2}, {"a", 1}}));  // objects aren't
    CHECK(Value(Object{{"a", 1}}) != Value(Object{{"a", 1}, {"b", 2}}));
    CHECK(Value(Object{{"a", 1}}) != Value(Object{{"b", 1}}));
    CHECK(Value(Array{Object{{"x", Array{}}}}) == Value(Array{Object{{"x", Array{}}}}));
}

TEST_CASE("values copy and move") {
    Value original = Object{{"list", Array{1, 2, 3}}};
    Value copy = original;
    copy.at("list").as_array().push_back(4);
    CHECK(original.at("list").as_array().size() == 3);
    Value moved = std::move(copy);
    CHECK(moved.at("list").as_array().size() == 4);
    original = moved;
    CHECK(original == moved);
    original = 5;
    CHECK(original == Value(5));
}

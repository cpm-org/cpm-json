// Objects (docs/spec.md §3.4): order, unique keys, and lookup at any size.

#include <cpm/test.hpp>

#include <string>
#include <vector>

#include "support/print.hpp"

namespace json = cpm::json;
using json::Object;
using json::Value;

namespace {

std::vector<std::string> keys_of(const Object& object) {
    std::vector<std::string> keys;
    for (const json::Member& member : object) keys.push_back(member.key());
    return keys;
}

Object numbered(int count) {
    Object object;
    for (int i = 0; i < count; ++i) object.insert_or_assign("key" + std::to_string(i), i);
    return object;
}

}  // namespace

TEST_CASE("members stay in the order they were added") {
    Object object;
    object.insert_or_assign("z", 1);
    object.insert_or_assign("a", 2);
    object.insert_or_assign("m", 3);
    CHECK(keys_of(object) == (std::vector<std::string>{"z", "a", "m"}));
    CHECK(object.size() == 3);
    CHECK(!object.empty());

    SUBCASE("replacing a value keeps its position") {
        Value& value = object.insert_or_assign("a", 20);
        CHECK(value == Value(20));
        CHECK(keys_of(object) == (std::vector<std::string>{"z", "a", "m"}));
        CHECK(object.at("a") == Value(20));
    }
    SUBCASE("erasing keeps the others' order") {
        CHECK(object.erase("a"));
        CHECK(!object.erase("a"));
        CHECK(keys_of(object) == (std::vector<std::string>{"z", "m"}));
        CHECK(!object.contains("a"));
    }
    SUBCASE("clear empties it") {
        object.clear();
        CHECK(object.empty());
        CHECK(object.find("z") == nullptr);
    }
}

TEST_CASE("a repeated key in a list keeps the last value at the first position") {
    const Object object{{"a", 1}, {"b", 2}, {"a", 3}};
    CHECK(keys_of(object) == (std::vector<std::string>{"a", "b"}));
    CHECK(object.at("a") == Value(3));
}

TEST_CASE("members' values can change through iteration") {
    Object object{{"a", 1}, {"b", 2}};
    for (json::Member& member : object) member.value = member.value.as_number().to_double() * 10;
    CHECK(object.at("a") == Value(10));
    CHECK(object.at("b") == Value(20));
}

TEST_CASE("large objects find, add, replace and erase") {
    Object object = numbered(1000);
    CHECK(object.size() == 1000);
    for (int i = 0; i < 1000; i += 37) CHECK(object.at("key" + std::to_string(i)) == Value(i));
    CHECK(object.find("key1000") == nullptr);

    object.insert_or_assign("key500", "replaced");
    CHECK(object.size() == 1000);
    CHECK(object.at("key500") == Value("replaced"));

    for (int i = 0; i < 1000; i += 2) CHECK(object.erase("key" + std::to_string(i)));
    CHECK(object.size() == 500);
    CHECK(object.find("key2") == nullptr);
    CHECK(object.at("key3") == Value(3));
    CHECK(keys_of(object).front() == "key1");
    CHECK(keys_of(object).back() == "key999");

    SUBCASE("down to a few members, and up again") {
        for (int i = 1; i < 1000; i += 2) {
            if (i > 7) object.erase("key" + std::to_string(i));
        }
        CHECK(keys_of(object) == (std::vector<std::string>{"key1", "key3", "key5", "key7"}));
        for (int i = 0; i < 20; ++i) object.insert_or_assign("more" + std::to_string(i), i);
        CHECK(object.at("more19") == Value(19));
        CHECK(object.at("key5") == Value(5));
    }
}

TEST_CASE("copies and moves keep lookups working") {
    const Object large = numbered(100);
    Object copy = large;
    CHECK(copy.at("key99") == Value(99));
    copy.insert_or_assign("extra", true);
    CHECK(!large.contains("extra"));

    Object assigned;
    assigned = copy;
    CHECK(assigned.at("extra") == Value(true));
    Object moved = std::move(assigned);
    CHECK(moved.at("key42") == Value(42));
    assigned = numbered(3);
    CHECK(assigned.at("key2") == Value(2));
    assigned = std::move(moved);
    CHECK(assigned.at("extra") == Value(true));
    CHECK(assigned == copy);
}

TEST_CASE("at() of a missing key is a LookupError") {
    const Object object{{"a", 1}};
    CHECK_THROWS_AS(object.at("b"), json::LookupError);
    CHECK_THROWS_WITH(object.at("b"), "no member \"b\"");
}

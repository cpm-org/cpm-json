// JSON Pointer (docs/spec.md §8), with the examples of RFC 6901 §5.

#include <cpm/test.hpp>

#include "support/print.hpp"

namespace json = cpm::json;
using json::Array;
using json::Object;
using json::Value;

namespace {

const Value rfc_example = json::parse(R"({
  "foo": ["bar", "baz"],
  "": 0,
  "a/b": 1,
  "c%d": 2,
  "e^f": 3,
  "g|h": 4,
  "i\\j": 5,
  "k\"l": 6,
  " ": 7,
  "m~n": 8
})");

}  // namespace

TEST_CASE("the examples of RFC 6901") {
    CHECK(*json::find(rfc_example, "") == rfc_example);
    CHECK(*json::find(rfc_example, "/foo") == Value(Array{"bar", "baz"}));
    CHECK(*json::find(rfc_example, "/foo/0") == Value("bar"));
    CHECK(*json::find(rfc_example, "/") == Value(0));
    CHECK(*json::find(rfc_example, "/a~1b") == Value(1));
    CHECK(*json::find(rfc_example, "/c%d") == Value(2));
    CHECK(*json::find(rfc_example, "/e^f") == Value(3));
    CHECK(*json::find(rfc_example, "/g|h") == Value(4));
    CHECK(*json::find(rfc_example, "/i\\j") == Value(5));
    CHECK(*json::find(rfc_example, "/k\"l") == Value(6));
    CHECK(*json::find(rfc_example, "/ ") == Value(7));
    CHECK(*json::find(rfc_example, "/m~0n") == Value(8));
}

TEST_CASE("pointers that name nothing find nullptr") {
    CHECK(json::find(rfc_example, "/missing") == nullptr);
    CHECK(json::find(rfc_example, "/foo/2") == nullptr);
    CHECK(json::find(rfc_example, "/foo/-") == nullptr);
    CHECK(json::find(rfc_example, "/foo/01") == nullptr);  // no leading zeros
    CHECK(json::find(rfc_example, "/foo/+1") == nullptr);
    CHECK(json::find(rfc_example, "/foo/x") == nullptr);
    CHECK(json::find(rfc_example, "/foo/99999999999999999999999") == nullptr);
    CHECK(json::find(rfc_example, "/a~1b/c") == nullptr);  // a step into a number
    CHECK(json::find(rfc_example, "/a/b") == nullptr);
}

TEST_CASE("pointers that aren't well formed are errors") {
    CHECK_THROWS_AS(json::find(rfc_example, "foo"), json::PointerError);
    CHECK_THROWS_WITH(json::find(rfc_example, "foo"), "a JSON Pointer must start with '/': \"foo\"");
    CHECK_THROWS_AS(json::find(rfc_example, "/m~2n"), json::PointerError);
    CHECK_THROWS_AS(json::find(rfc_example, "/m~"), json::PointerError);
    CHECK_THROWS_AS(json::find(rfc_example, "/missing/~x"), json::PointerError);  // even after a missing step
}

TEST_CASE("found values can change") {
    Value v = Object{{"servers", Array{Object{{"port", 80}}}}};
    json::Value* port = json::find(v, "/servers/0/port");
    REQUIRE(port != nullptr);
    *port = 8080;
    CHECK(v.at("servers").at(0).at("port") == Value(8080));
}

TEST_CASE("pointers are made from tokens") {
    CHECK(json::escape_pointer_token("a/b~c") == "a~1b~0c");
    CHECK(json::make_pointer({}) == "");
    CHECK(json::make_pointer({"a/b", "0", "~"}) == "/a~1b/0/~0");
    CHECK(*json::find(rfc_example, json::make_pointer({"m~n"})) == Value(8));
}

// Conversions (docs/spec.md §9): the built-in types, user types, and the paths in errors.

#include <cpm/test.hpp>

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "support/print.hpp"

namespace json = cpm::json;
using json::Array;
using json::Object;
using json::Value;

namespace {

struct Point {
    int x = 0;
    int y = 0;
    friend bool operator==(const Point&, const Point&) = default;
};

struct Server {
    std::string host;
    std::uint16_t port = 0;
    std::optional<std::vector<std::string>> aliases;
};

struct NoTraits {};

}  // namespace

template <>
struct cpm::json::Traits<Point> {
    static Value to_json(const Point& p) { return Object{{"x", p.x}, {"y", p.y}}; }
    static Point from_json(const Value& v) { return {v.at("x").get<int>(), v.at("y").get<int>()}; }
};

// A user type that extends error paths the way the built-in containers do.
template <>
struct cpm::json::Traits<Server> {
    static Server from_json(const Value& v) {
        auto field = [&]<class T>(std::string_view key, T& out) {
            try {
                out = v.at(key).get<T>();
            } catch (const TypeError& error) {
                throw TypeError(error, key);
            }
        };
        Server server;
        field("host", server.host);
        field("port", server.port);
        if (const Value* aliases = v.find("aliases")) {
            server.aliases = detail::at_path("aliases", [&] { return aliases->get<std::vector<std::string>>(); });
        }
        return server;
    }
};

static_assert(json::writable<int> && json::readable<int>);
static_assert(json::writable<std::string_view> && !json::readable<std::string_view>);
static_assert(json::writable<std::vector<Point>> && json::readable<std::map<std::string, Point>>);
static_assert(!json::writable<Server> && json::readable<Server>);
static_assert(!json::writable<NoTraits> && !json::readable<NoTraits>);
static_assert(!json::writable<std::vector<NoTraits>> && !json::readable<std::optional<NoTraits>>);
static_assert(!json::readable<char> && !json::writable<wchar_t>);

TEST_CASE("scalars convert both ways") {
    CHECK(json::to_json(true) == Value(true));
    CHECK(json::from_json<bool>(Value(false)) == false);
    CHECK(json::to_json(42) == Value(42));
    CHECK(Value(42).get<long long>() == 42);
    CHECK(Value(3.0).get<int>() == 3);
    CHECK(Value(2.5).get<double>() == 2.5);
    CHECK(Value(7).get<float>() == 7.0f);
    CHECK(json::to_json(std::string("s")) == Value("s"));
    CHECK(Value("s").get<std::string>() == "s");
    CHECK(json::to_json(std::string_view("v")) == Value("v"));
    CHECK(json::to_json("literal") == Value("literal"));
    const char* pointer = "pointer";
    CHECK(json::to_json(pointer) == Value("pointer"));
    CHECK(json::to_json(nullptr) == Value());
    CHECK(Value().get<std::nullptr_t>() == nullptr);
    CHECK(json::to_json(json::Number(5)) == Value(5));
    CHECK(Value(Array{1}).get<Value>() == Value(Array{1}));
}

TEST_CASE("integers must be whole and in range") {
    CHECK(Value(65535).get<std::uint16_t>() == 65535);
    CHECK_THROWS_WITH(Value(70000).get<std::uint16_t>(), "expected an integer from 0 to 65535, found 70000");
    CHECK_THROWS_WITH(Value(-1).get<unsigned>(), "expected an integer from 0 to 4294967295, found -1");
    CHECK_THROWS_WITH(Value(2.5).get<int>(), "expected an integer from -2147483648 to 2147483647, found 2.5");
    CHECK_THROWS_WITH(Value("7").get<std::int8_t>(), "expected an integer from -128 to 127, found a string");
    CHECK_THROWS_AS(Value(1e30).get<std::int64_t>(), json::TypeError);
}

TEST_CASE("floats must fit") {
    CHECK_THROWS_WITH(Value(1e300).get<float>(), "expected a number that fits in a float, found 1e+300");
    CHECK_THROWS_WITH(Value(true).get<double>(), "expected a number, found true");
}

TEST_CASE("optional is null or the value") {
    CHECK(json::to_json(std::optional<int>()) == Value());
    CHECK(json::to_json(std::optional<int>(3)) == Value(3));
    CHECK(Value().get<std::optional<int>>() == std::nullopt);
    CHECK(Value(4).get<std::optional<int>>() == std::optional<int>(4));
}

TEST_CASE("sequences are arrays") {
    CHECK(json::to_json(std::vector<int>{1, 2, 3}) == Value(Array{1, 2, 3}));
    CHECK(Value(Array{1, 2}).get<std::vector<int>>() == (std::vector<int>{1, 2}));
    CHECK(json::to_json(std::vector<bool>{true, false}) == Value(Array{true, false}));
    CHECK(Value(Array{false, true}).get<std::vector<bool>>() == (std::vector<bool>{false, true}));
    CHECK(json::to_json(std::array<int, 2>{4, 5}) == Value(Array{4, 5}));
    CHECK(Value(Array{4, 5}).get<std::array<int, 2>>() == (std::array<int, 2>{4, 5}));
    CHECK(json::to_json(std::pair<int, std::string>{1, "a"}) == Value(Array{1, "a"}));
    CHECK(Value(Array{1, "a"}).get<std::pair<int, std::string>>() == (std::pair<int, std::string>{1, "a"}));
    CHECK(json::to_json(std::tuple<bool, int, std::string>{true, 2, "c"}) == Value(Array{true, 2, "c"}));
    CHECK(Value(Array{true, 2, "c"}).get<std::tuple<bool, int, std::string>>() ==
          (std::tuple<bool, int, std::string>{true, 2, "c"}));

    SUBCASE("of the right length") {
        CHECK_THROWS_WITH((Value(Array{1, 2, 3}).get<std::array<int, 2>>()),
                          "expected an array of 2 elements, found 3 elements");
        CHECK_THROWS_WITH((Value(Array{1}).get<std::pair<int, int>>()), "expected an array of 2 elements, found 1 element");
        CHECK(Value(Array{}).get<std::array<int, 0>>().empty());
    }
}

TEST_CASE("string maps are objects") {
    const std::map<std::string, int> ordered{{"b", 2}, {"a", 1}};
    CHECK(json::to_json(ordered) == Value(Object{{"a", 1}, {"b", 2}}));
    CHECK((Value(Object{{"x", 1}}).get<std::map<std::string, int>>()) == (std::map<std::string, int>{{"x", 1}}));
    const std::unordered_map<std::string, std::vector<int>> lists{{"l", {1, 2}}};
    CHECK(json::to_json(lists) == Value(Object{{"l", Array{1, 2}}}));
    CHECK(Value(Object{{"l", Array{1, 2}}}).get<std::unordered_map<std::string, std::vector<int>>>() == lists);
}

TEST_CASE("user types convert through their Traits") {
    CHECK(json::to_json(Point{1, 2}) == Value(Object{{"x", 1}, {"y", 2}}));
    CHECK(Value(Object{{"y", 4}, {"x", 3}}).get<Point>() == (Point{3, 4}));
    CHECK(json::to_json(std::vector<Point>{{1, 2}}) == Value(Array{Object{{"x", 1}, {"y", 2}}}));
}

TEST_CASE("errors say where in the document a conversion failed") {
    const Value v = json::parse(R"({"servers": [
        {"host": "a", "port": 80},
        {"host": "b", "port": 443},
        {"host": "c", "port": 70000}
    ]})");
    const std::string message = support::message_of([&] { v.at("servers").get<std::vector<Server>>(); });
    CHECK(message == "at /2/port: expected an integer from 0 to 65535, found 70000");

    try {
        json::from_json<std::map<std::string, std::vector<Server>>>(v);
        FAIL("the conversion should have failed");
    } catch (const json::TypeError& error) {
        CHECK(error.path() == "/servers/2/port");
        CHECK(error.reason() == "expected an integer from 0 to 65535, found 70000");
    }

    SUBCASE("keys are escaped in the path") {
        const Value odd = Object{{"a/b", Array{Object{{"aliases", Array{"x", 5}}, {"host", "h"}, {"port", 1}}}}};
        CHECK(support::message_of([&] { odd.get<std::map<std::string, std::vector<Server>>>(); }) ==
              "at /a~1b/0/aliases/1: expected a string, found 5");
    }
}

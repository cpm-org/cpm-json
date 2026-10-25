// Writing (docs/spec.md §5): formats, escapes, numbers, the streaming writer and sinks.

#include <cpm/test.hpp>

#include <sstream>
#include <stdexcept>
#include <string>

#include "support/print.hpp"

namespace json = cpm::json;
using json::Array;
using json::Object;
using json::Value;

namespace {

const Value sample = Object{{"name", "cpm"}, {"tags", Array{"c++", "json"}}, {"stars", nullptr},
                            {"empty", Array{}}, {"none", Object{}}};

}  // namespace

TEST_CASE("compact text has no whitespace") {
    CHECK(json::write(sample) == R"({"name":"cpm","tags":["c++","json"],"stars":null,"empty":[],"none":{}})");
    CHECK(json::write(Value()) == "null");
    CHECK(json::write(Value(true)) == "true");
    CHECK(json::write(Value(Array{false, 0, -1.5})) == "[false,0,-1.5]");
}

TEST_CASE("indented text has one element or member per line") {
    CHECK(json::write(sample, {.indent = 2}) == R"({
  "name": "cpm",
  "tags": [
    "c++",
    "json"
  ],
  "stars": null,
  "empty": [],
  "none": {}
})");
    // Array{Value(...)}: with a lone Array inside the braces, Array{Array{1}} is a copy of [1] on some compilers.
    CHECK(json::write(Value(Array{Value(Array{1})}), {.indent = 4}) == "[\n    [\n        1\n    ]\n]");
    CHECK(json::write(Value(5), {.indent = 2}) == "5");
}

TEST_CASE("keys can be sorted") {
    const Value v = Object{{"b", 1}, {"a", Object{{"d", 1}, {"c", 2}}}, {"B", 3}};
    CHECK(json::write(v, {.sort_keys = true}) == R"({"B":3,"a":{"c":2,"d":1},"b":1})");
    CHECK(json::write(v) == R"({"b":1,"a":{"d":1,"c":2},"B":3})");
}

TEST_CASE("strings escape what they must") {
    CHECK(json::write(Value("quote \" backslash \\ slash /")) == R"("quote \" backslash \\ slash /")");
    CHECK(json::write(Value("\b\f\n\r\t")) == R"("\b\f\n\r\t")");
    CHECK(json::write(Value(std::string("\x00\x01\x1f\x7f", 4))) == "\"\\u0000\\u0001\\u001f\x7f\"");
    CHECK(json::write(Value("caf\xC3\xA9 \xF0\x9F\x98\x80")) == "\"caf\xC3\xA9 \xF0\x9F\x98\x80\"");

    SUBCASE("or everything above ASCII, on request") {
        CHECK(json::write(Value("caf\xC3\xA9 \xE2\x82\xAC \xF0\x9F\x98\x80"), {.escape_non_ascii = true}) ==
              R"("caf\u00e9 \u20ac \ud83d\ude00")");
        CHECK(json::write(Value(Object{{"\xC3\xA9", 1}}), {.escape_non_ascii = true}) == R"({"\u00e9":1})");
    }
}

TEST_CASE("strings that aren't UTF-8 can't be written") {
    CHECK_THROWS_AS(json::write(Value("bad \xFF byte")), json::WriteError);
    CHECK_THROWS_WITH(json::write(Value("bad \xFF byte")), "a string to write isn't valid UTF-8: byte 0xff at offset 4");
    CHECK_THROWS_AS(json::write(Value(Object{{"\xC3", 1}})), json::WriteError);
}

TEST_CASE("numbers are exact, and doubles stay doubles") {
    CHECK(json::write(Value(Array{1, -0.0, 1.0, 0.1, 1e300, 18446744073709551615u})) ==
          "[1,-0.0,1.0,0.1,1e+300,18446744073709551615]");
}

TEST_CASE("the writer writes events") {
    std::string out;
    json::StringSink sink(out);
    json::Writer w(sink, {.indent = 1});
    w.begin_object();
    w.key("ids");
    w.begin_array();
    for (int id : {3, 1, 2}) w.number(id);
    w.number(2.5);
    w.end_array();
    w.key("ok");
    w.boolean(true);
    w.end_object();
    CHECK(w.complete());
    CHECK(out == "{\n \"ids\": [\n  3,\n  1,\n  2,\n  2.5\n ],\n \"ok\": true\n}");
}

TEST_CASE("the writer reformats text as the handler of a parse") {
    std::string out;
    json::StringSink sink(out);
    json::Writer w(sink, {.indent = 2});
    json::parse("// config\n{ \"a\" : [ 1,2, ], \"b\":{\"c\":\"\\u00e9\"} }", w, json::jsonc);
    CHECK(out == "{\n  \"a\": [\n    1,\n    2\n  ],\n  \"b\": {\n    \"c\": \"\xC3\xA9\"\n  }\n}");
}

TEST_CASE("the writer rejects calls out of order") {
    std::string out;
    json::StringSink sink(out);
    json::Writer w(sink);
    CHECK_THROWS_AS(w.key("a"), std::logic_error);
    CHECK_THROWS_AS(w.end_object(), std::logic_error);
    w.begin_object();
    CHECK_THROWS_AS(w.number(1), std::logic_error);  // a key is due
    CHECK_THROWS_AS(w.end_array(), std::logic_error);
    w.key("a");
    CHECK_THROWS_AS(w.key("b"), std::logic_error);  // a value is due
    CHECK_THROWS_AS(w.end_object(), std::logic_error);
    w.null();
    w.end_object();
    CHECK_THROWS_AS(w.null(), std::logic_error);  // a second top-level value
    CHECK(out == R"({"a":null})");
    CHECK_THROWS_AS(json::Writer(sink, {.indent = -1}), std::invalid_argument);
}

TEST_CASE("a string that can't be written leaves the writer as it was") {
    std::string out;
    json::StringSink sink(out);
    json::Writer w(sink);
    w.begin_array();
    w.number(1);
    CHECK_THROWS_AS(w.string("\xFF"), json::WriteError);
    w.string("ok");
    w.begin_object();
    CHECK_THROWS_AS(w.key("\xFF"), json::WriteError);
    w.key("k");
    w.number(2);
    w.end_object();
    w.end_array();
    CHECK(out == R"([1,"ok",{"k":2}])");
}

TEST_CASE("text reaches the sink when a value is complete, or on flush") {
    std::string out;
    json::StringSink sink(out);
    json::Writer w(sink);
    w.begin_array();
    w.string("partial");
    CHECK(out.empty());
    w.flush();
    CHECK(out == R"(["partial")");
    w.end_array();
    CHECK(out == R"(["partial"])");

    SUBCASE("and in pieces while a large value is written") {
        std::string big;
        json::StringSink big_sink(big);
        json::Writer writer(big_sink);
        writer.begin_array();
        for (int i = 0; i < 10000; ++i) writer.string("0123456789");
        CHECK(!big.empty());
        writer.end_array();
        CHECK(big.size() == 2 + 10000 * 12 + 9999);  // brackets, quoted strings, commas
    }
}

TEST_CASE("stream sinks write to streams") {
    std::ostringstream stream;
    json::StreamSink sink(stream);
    json::write(sample, sink);
    CHECK(stream.str() == json::write(sample));

    SUBCASE("and report a failed stream") {
        std::ostringstream failed;
        failed.setstate(std::ios::badbit);
        json::StreamSink bad(failed);
        CHECK_THROWS_AS(json::write(Value(1), bad), json::WriteError);
    }
}

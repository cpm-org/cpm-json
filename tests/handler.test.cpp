// Events (docs/spec.md §6): what parse and emit deliver, and the document builder.

#include <cpm/test.hpp>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

#include "support/print.hpp"

namespace json = cpm::json;
using json::Array;
using json::Object;
using json::Value;

namespace {

// Writes each event down as a short string.
class Recorder final : public json::Handler {
public:
    std::vector<std::string> events;

    void null() override { events.emplace_back("null"); }
    void boolean(bool value) override { events.emplace_back(value ? "true" : "false"); }
    void number(const json::Number& value) override { events.push_back("n:" + json::write(Value(value))); }
    void string(std::string_view value) override { events.push_back("s:" + std::string(value)); }
    void begin_array() override { events.emplace_back("["); }
    void end_array() override { events.emplace_back("]"); }
    void begin_object() override { events.emplace_back("{"); }
    void key(std::string_view name) override { events.push_back("k:" + std::string(name)); }
    void end_object() override { events.emplace_back("}"); }
};

// Counts nesting and nothing else, so depth costs no memory beyond the parser's.
class DepthCounter final : public json::Handler {
public:
    std::size_t depth = 0;
    std::size_t deepest = 0;

    void null() override {}
    void boolean(bool) override {}
    void number(const json::Number&) override {}
    void string(std::string_view) override {}
    void begin_array() override { deepest = std::max(deepest, ++depth); }
    void end_array() override { --depth; }
    void begin_object() override { deepest = std::max(deepest, ++depth); }
    void key(std::string_view) override {}
    void end_object() override { --depth; }
};

using Events = std::vector<std::string>;

}  // namespace

TEST_CASE("parse delivers events in document order") {
    Recorder recorder;
    json::parse(R"({"a":[1,"x",true,null],"b":{},"c":2.5})", recorder);
    CHECK(recorder.events == (Events{"{", "k:a", "[", "n:1", "s:x", "true", "null", "]", "k:b", "{", "}", "k:c",
                                     "n:2.5", "}"}));
}

TEST_CASE("decoded strings and keys reach the handler") {
    Recorder recorder;
    json::parse(R"({"\u0061\n":"\u00e9"})", recorder);
    CHECK(recorder.events == (Events{"{", "k:a\n", "s:\xC3\xA9", "}"}));
}

TEST_CASE("with a handler, repeated keys are reported unless they are errors") {
    const std::string text = R"({"a":1,"a":2})";
    Recorder recorder;
    json::parse(text, recorder, {.duplicate_keys = json::DuplicateKeys::keep_first});
    CHECK(recorder.events == (Events{"{", "k:a", "n:1", "k:a", "n:2", "}"}));
    Recorder strict;
    CHECK_THROWS_AS(json::parse(text, strict), json::ParseError);
}

TEST_CASE("events before an error have been delivered") {
    Recorder recorder;
    CHECK_THROWS_AS(json::parse("[1, 2, }", recorder), json::ParseError);
    CHECK(recorder.events == (Events{"[", "n:1", "n:2"}));
}

TEST_CASE("exceptions from the handler pass through") {
    class Stopper final : public json::Handler {
    public:
        void null() override {}
        void boolean(bool) override {}
        void number(const json::Number&) override { throw std::runtime_error("found a number"); }
        void string(std::string_view) override {}
        void begin_array() override {}
        void end_array() override {}
        void begin_object() override {}
        void key(std::string_view) override {}
        void end_object() override {}
    } stopper;
    CHECK_THROWS_WITH(json::parse(R"(["a", 1, nonsense)", stopper), "found a number");
}

TEST_CASE("parsing events nests as deep as max_depth allows, without recursion") {
    const std::size_t levels = 200000;
    const std::string deep = std::string(levels, '[') + std::string(levels, ']');
    DepthCounter counter;
    json::parse(deep, counter, {.max_depth = levels});
    CHECK(counter.deepest == levels);
    CHECK(counter.depth == 0);
}

TEST_CASE("emit walks a document") {
    const Value v = Object{{"z", Array{1, "two"}}, {"a", Object{{"y", nullptr}, {"b", false}}}};
    Recorder in_order;
    json::emit(v, in_order);
    CHECK(in_order.events ==
          (Events{"{", "k:z", "[", "n:1", "s:two", "]", "k:a", "{", "k:y", "null", "k:b", "false", "}", "}"}));
    Recorder sorted;
    json::emit(v, sorted, true);
    CHECK(sorted.events ==
          (Events{"{", "k:a", "{", "k:b", "false", "k:y", "null", "}", "k:z", "[", "n:1", "s:two", "]", "}"}));
}

TEST_CASE("the document builder builds what it is given") {
    json::DocumentBuilder builder;
    CHECK(!builder.complete());
    builder.begin_object();
    builder.key("list");
    builder.begin_array();
    builder.number(json::Number(1));
    builder.string("x");
    builder.end_array();
    builder.key("flag");
    builder.boolean(true);
    builder.end_object();
    REQUIRE(builder.complete());
    CHECK(builder.take() == Value(Object{{"list", Array{1, "x"}}, {"flag", true}}));
    CHECK(!builder.complete());

    SUBCASE("then another value") {
        builder.null();
        CHECK(builder.take() == Value());
    }
}

TEST_CASE("the document builder rejects events out of order") {
    json::DocumentBuilder builder;
    CHECK_THROWS_AS(builder.key("a"), std::logic_error);
    CHECK_THROWS_AS(builder.end_array(), std::logic_error);
    CHECK_THROWS_AS(builder.take(), std::logic_error);
    builder.begin_object();
    CHECK_THROWS_AS(builder.null(), std::logic_error);  // a key is due
    CHECK_THROWS_AS(builder.end_array(), std::logic_error);
    builder.key("a");
    CHECK_THROWS_AS(builder.key("b"), std::logic_error);  // a value is due
    CHECK_THROWS_AS(builder.end_object(), std::logic_error);
    builder.null();
    builder.end_object();
    CHECK_THROWS_AS(builder.null(), std::logic_error);  // a second top-level value
}

TEST_CASE("the document builder's duplicate policies") {
    auto build = [](json::DuplicateKeys duplicates) {
        json::DocumentBuilder builder(duplicates);
        json::parse(R"({"a":1,"a":[2]})", builder, {.duplicate_keys = json::DuplicateKeys::keep_last});
        return builder.take();
    };
    CHECK(build(json::DuplicateKeys::keep_first) == Value(Object{{"a", 1}}));
    CHECK(build(json::DuplicateKeys::keep_last) == Value(Object{{"a", Array{2}}}));
    CHECK_THROWS_AS(build(json::DuplicateKeys::error), json::Error);
}

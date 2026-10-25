// Reading (docs/spec.md §4): the grammar, options, and errors with their codes and locations.

#include <cpm/test.hpp>

#include <string>
#include <string_view>

#include "support/print.hpp"

namespace json = cpm::json;
using json::Array;
using json::Object;
using json::Value;
using Code = json::ParseError::Code;

namespace {

void expect_error(std::string_view text, Code code, std::size_t line, std::size_t column,
                  const json::ParseOptions& options = {}) {
    INFO("text: {}", text);
    const std::optional<json::ParseError> error = support::parse_error(text, options);
    REQUIRE(error.has_value());
    INFO("error: {}", error->what());
    CHECK(error->code() == code);
    CHECK(error->location().line == line);
    CHECK(error->location().column == column);
}

}  // namespace

TEST_CASE("scalars") {
    CHECK(json::parse("null") == Value());
    CHECK(json::parse("true") == Value(true));
    CHECK(json::parse("false") == Value(false));
    CHECK(json::parse("0") == Value(0));
    CHECK(json::parse("-12") == Value(-12));
    CHECK(json::parse("1.5e3") == Value(1500.0));
    CHECK(json::parse("\"text\"") == Value("text"));
}

TEST_CASE("arrays and objects, nested and empty") {
    CHECK(json::parse("[]") == Value(Array{}));
    CHECK(json::parse("{}") == Value(Object{}));
    CHECK(json::parse("[1,[2,[3,[]]]]") == Value(Array{1, Array{2, Array{3, Array{}}}}));
    CHECK(json::parse(R"({"a":{"b":[true,null,{}]},"c":""})") ==
          Value(Object{{"a", Object{{"b", Array{true, nullptr, Object{}}}}}, {"c", ""}}));
    const Value v = json::parse(R"({"z":1,"a":2})");
    CHECK(v.as_object().begin()->key() == "z");  // members keep the order of the text
}

TEST_CASE("whitespace and a byte order mark") {
    CHECK(json::parse(" \t\n\r[ 1 ,\n2 ] \r\n") == Value(Array{1, 2}));
    CHECK(json::parse("\xEF\xBB\xBF{}") == Value(Object{}));
    expect_error("\v1", Code::unexpected_character, 1, 1);  // vertical tab isn't JSON whitespace
}

TEST_CASE("strings: escapes and UTF-8") {
    CHECK(json::parse(R"("\"\\\/\b\f\n\r\t")") == Value("\"\\/\b\f\n\r\t"));
    CHECK(json::parse(R"("éÉ")") == Value("\xC3\xA9\xC3\x89"));
    CHECK(json::parse(R"("😀")") == Value("\xF0\x9F\x98\x80"));
    CHECK(json::parse("\"caf\xC3\xA9 \xE2\x82\xAC\"") == Value("caf\xC3\xA9 \xE2\x82\xAC"));
    CHECK(json::parse(R"("a\u0000b")").as_string() == std::string("a\0b", 3));
    CHECK(json::parse(R"("\u007f")").as_string() == "\x7f");
}

TEST_CASE("numbers follow the grammar") {
    expect_error("01", Code::invalid_number, 1, 1);
    expect_error("-01", Code::invalid_number, 1, 1);
    expect_error("1.e5", Code::invalid_number, 1, 1);
    expect_error("-a", Code::invalid_number, 1, 1);
    expect_error("1e+x", Code::invalid_number, 1, 1);
    expect_error(".5", Code::unexpected_character, 1, 1);
    expect_error("+1", Code::unexpected_character, 1, 1);
    expect_error("Infinity", Code::unexpected_character, 1, 1);
    expect_error("NaN", Code::unexpected_character, 1, 1);
    expect_error("0x10", Code::trailing_content, 1, 2);
    expect_error("1e400", Code::number_out_of_range, 1, 1);
    expect_error("[0, -1e999]", Code::number_out_of_range, 1, 5);

    SUBCASE("cut short by the end of the text") {
        expect_error("-", Code::unexpected_end, 1, 2);
        expect_error("1.", Code::unexpected_end, 1, 3);
        expect_error("1e", Code::unexpected_end, 1, 3);
        expect_error("1e-", Code::unexpected_end, 1, 4);
    }
}

TEST_CASE("literals are exact") {
    expect_error("trux", Code::unexpected_character, 1, 1);
    expect_error("True", Code::unexpected_character, 1, 1);
    expect_error("tru", Code::unexpected_end, 1, 4);
    expect_error("[nul", Code::unexpected_end, 1, 5);
    CHECK(support::parse_error("trux")->reason() == "expected 'true', found 'trux'");
}

TEST_CASE("malformed strings") {
    expect_error("\"abc", Code::unexpected_end, 1, 5);
    CHECK(support::parse_error("[\n  \"abc")->reason() == "expected '\"' to close the string that starts at line 2, column 3");
    expect_error(R"("a\x")", Code::invalid_escape, 1, 3);
    expect_error(R"("\u12G4")", Code::invalid_escape, 1, 2);
    expect_error(R"("\u12)", Code::unexpected_end, 1, 6);
    expect_error(R"("\ud800")", Code::invalid_escape, 1, 2);
    expect_error(R"("\udc00")", Code::invalid_escape, 1, 2);
    expect_error(R"("\ud800A")", Code::invalid_escape, 1, 2);
    expect_error("\"a\tb\"", Code::control_character, 1, 3);
    expect_error("\"a\nb\"", Code::control_character, 1, 3);
    expect_error("\"\x01\"", Code::control_character, 1, 2);
    CHECK(support::parse_error("\"a\tb\"")->reason() == "U+0009 must be escaped in a string");
}

TEST_CASE("invalid UTF-8 is an error anywhere") {
    expect_error("\"\xFF\"", Code::invalid_utf8, 1, 2);
    expect_error("\"\xC0\xAF\"", Code::invalid_utf8, 1, 2);       // overlong '/'
    expect_error("\"\xED\xA0\x80\"", Code::invalid_utf8, 1, 2);   // an encoded surrogate
    expect_error("\xFF", Code::invalid_utf8, 1, 1);
    expect_error("[1, \x80]", Code::invalid_utf8, 1, 5);
    expect_error("// \xFF\n1", Code::invalid_utf8, 1, 4, json::jsonc);
    expect_error("/* \xC3 */ 1", Code::invalid_utf8, 1, 4, json::jsonc);
    expect_error("[1, \xC3\xA9]", Code::unexpected_character, 1, 5);  // valid, but not JSON
}

TEST_CASE("structure errors say what was expected") {
    expect_error("", Code::unexpected_end, 1, 1);
    expect_error("   ", Code::unexpected_end, 1, 4);
    expect_error("[1,2", Code::unexpected_end, 1, 5);
    expect_error("[1 2]", Code::unexpected_character, 1, 4);
    expect_error(R"({"a" 1})", Code::unexpected_character, 1, 6);
    expect_error("{1:2}", Code::unexpected_character, 1, 2);
    expect_error(R"({"a":})", Code::unexpected_character, 1, 6);
    expect_error("[1}", Code::unexpected_character, 1, 3);
    expect_error("[1] 2", Code::trailing_content, 1, 5);
    expect_error("{} {}", Code::trailing_content, 1, 4);
    expect_error("{\n  \"a\": 1,\n  \"b\" 2\n}", Code::unexpected_character, 3, 7);

    CHECK(std::string(support::parse_error("{\n  \"a\": 1,\n  \"b\" 2\n}")->what()) ==
          "line 3, column 7: expected ':' after the key, found a number");
    CHECK(support::parse_error("[1 2]")->reason() == "expected ',' or ']' after an array element, found a number");
    CHECK(support::parse_error("")->reason() == "expected a value, found the end of the text");
    CHECK(support::parse_error("[1] x")->reason() == "expected the end of the text after the value, found 'x'");
}

TEST_CASE("trailing commas and comments need the options") {
    expect_error("[1,]", Code::unexpected_character, 1, 4);
    expect_error(R"({"a":1,})", Code::unexpected_character, 1, 8);
    expect_error("/**/1", Code::unexpected_character, 1, 1);
    expect_error("[1] // comment", Code::trailing_content, 1, 5);
    CHECK(support::parse_error("[1,]")->reason() ==
          "expected a value after ',', found ']' (trailing commas need the jsonc options)");

    SUBCASE("with jsonc") {
        const std::string text = "// settings\n{\n  /* list */ \"a\": [1, 2,], // two\n  \"b\": {},\n}\n/* end */";
        CHECK(json::parse(text, json::jsonc) == Value(Object{{"a", Array{1, 2}}, {"b", Object{}}}));
        CHECK(json::parse("[1,]", {.trailing_commas = true}) == Value(Array{1}));
        CHECK(json::parse("1 // no newline at the end", json::jsonc) == Value(1));
        CHECK(json::parse("/* a */ /* b */ 1 /**/", json::jsonc) == Value(1));
        CHECK(json::parse("\"// not a comment\"", json::jsonc) == Value("// not a comment"));
        expect_error("[1,,]", Code::unexpected_character, 1, 4, json::jsonc);  // only one comma
        expect_error("[,]", Code::unexpected_character, 1, 2, json::jsonc);
        expect_error("/* never closed", Code::unterminated_comment, 1, 1, json::jsonc);
        expect_error("1 /", Code::unexpected_end, 1, 4, json::jsonc);
        expect_error("[1 /x]", Code::unexpected_character, 1, 4, json::jsonc);
        expect_error("/* /* nested */ */ 1", Code::unexpected_character, 1, 17, json::jsonc);
    }
}

TEST_CASE("duplicate keys") {
    const std::string text = R"({"a":1,"b":2,"a":3})";
    expect_error(text, Code::duplicate_key, 1, 14);
    CHECK(support::parse_error(text)->reason() == "the object already has the key \"a\"");

    const Value first = json::parse(text, {.duplicate_keys = json::DuplicateKeys::keep_first});
    CHECK(first == Value(Object{{"a", 1}, {"b", 2}}));
    const Value last = json::parse(text, {.duplicate_keys = json::DuplicateKeys::keep_last});
    CHECK(last == Value(Object{{"a", 3}, {"b", 2}}));
    CHECK(last.as_object().begin()->key() == "a");  // at the first one's position

    SUBCASE("only within one object") {
        CHECK(json::parse(R"([{"a":1},{"a":2}])") == Value(Array{Object{{"a", 1}}, Object{{"a", 2}}}));
        CHECK(json::parse(R"({"a":{"a":1}})") == Value(Object{{"a", Object{{"a", 1}}}}));
        CHECK(json::parse(R"({"a":{"b":1},"b":2})") == Value(Object{{"a", Object{{"b", 1}}}, {"b", 2}}));
    }
    SUBCASE("after escapes are decoded") {
        expect_error(R"({"a":1,"a":2})", Code::duplicate_key, 1, 8);
    }
}

TEST_CASE("nesting is limited by max_depth") {
    const std::string deep = std::string(512, '[') + std::string(512, ']');
    CHECK(json::parse(deep).is_array());
    expect_error("[" + deep + "]", Code::too_deep, 1, 513);
    CHECK(support::parse_error("[" + deep + "]")->reason() == "arrays and objects nest deeper than 512 levels");

    CHECK(json::parse("[1]", {.max_depth = 1}) == Value(Array{1}));
    expect_error(R"([{"a":1}])", Code::too_deep, 1, 2, {.max_depth = 1});
    CHECK(json::parse("1", {.max_depth = 0}) == Value(1));
    expect_error("[]", Code::too_deep, 1, 1, {.max_depth = 0});

}

TEST_CASE("try_parse returns the error instead of throwing it") {
    json::ParseResult good = json::try_parse("[1]");
    REQUIRE(static_cast<bool>(good));
    CHECK(good.error() == nullptr);
    CHECK(good.value() == Value(Array{1}));
    CHECK(std::move(good).value() == Value(Array{1}));

    const json::ParseResult bad = json::try_parse("[1");
    CHECK(!static_cast<bool>(bad));
    REQUIRE(bad.error() != nullptr);
    CHECK(bad.error()->code() == Code::unexpected_end);
    CHECK_THROWS_AS(bad.value(), json::ParseError);
}

TEST_CASE("parse throws ParseError, a json::Error") {
    CHECK_THROWS_AS(json::parse("{"), json::ParseError);
    CHECK_THROWS_AS(json::parse("{"), json::Error);
}

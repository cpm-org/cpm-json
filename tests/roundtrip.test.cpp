// Round trips: random documents, written compact and indented, read back equal (docs/spec.md §11).

#include <cpm/test.hpp>

#include <bit>
#include <cmath>
#include <cstdint>
#include <random>
#include <string>

#include "support/print.hpp"
#include "utf8.hpp"

namespace json = cpm::json;
using json::Array;
using json::Object;
using json::Value;

namespace {

class Generator {
public:
    explicit Generator(std::uint64_t seed) : random_(seed) {}

    Value value(int depth) {
        switch (pick(depth > 0 ? 7 : 5)) {
            case 0: return nullptr;
            case 1: return pick(2) == 1;
            case 2: return number();
            case 3: return text();
            case 4: return number();
            case 5: {
                Array array;
                for (int i = pick(5); i > 0; --i) array.push_back(value(depth - 1));
                return array;
            }
            default: {
                Object object;
                for (int i = pick(5); i > 0; --i) object.insert_or_assign(text(), value(depth - 1));
                return object;
            }
        }
    }

private:
    int pick(int n) { return static_cast<int>(random_() % static_cast<std::uint64_t>(n)); }

    json::Number number() {
        switch (pick(4)) {
            case 0: return static_cast<std::int64_t>(random_());
            case 1: return random_();
            case 2: return pick(2001) - 1000;
            default: {
                double d = std::bit_cast<double>(random_());
                return std::isfinite(d) ? d : 0.5;
            }
        }
    }

    // Any code points, including the ones that must be escaped and the ones above U+FFFF.
    std::string text() {
        std::string out;
        for (int i = pick(8); i > 0; --i) {
            char32_t code_point = 0;
            switch (pick(4)) {
                case 0: code_point = static_cast<char32_t>(pick(0x80)); break;
                case 1: code_point = static_cast<char32_t>(0x80 + pick(0x780)); break;
                case 2: code_point = static_cast<char32_t>(0xE000 + pick(0x2000)); break;
                default: code_point = static_cast<char32_t>(0x10000 + pick(0x100000)); break;
            }
            cpm::json::detail::utf8::append(out, code_point);
        }
        return out;
    }

    std::mt19937_64 random_;
};

}  // namespace

TEST_CASE("random documents read back equal") {
    Generator generate(20261025);
    for (int i = 0; i < 2000; ++i) {
        const Value original = generate.value(4);
        for (int indent : {0, 3}) {
            for (bool ascii : {false, true}) {
                const std::string text = json::write(original, {.indent = indent, .escape_non_ascii = ascii});
                INFO("document {}, indent {}, ascii {}: {}", i, indent, ascii, text);
                CHECK(json::parse(text) == original);
            }
        }
    }
}

TEST_CASE("reading and writing again gives the same text") {
    const std::string text = R"({"a":[1,2.5,-0.0,"\u0001\"\\"],"b":{"c":null,"d":true},"e":18446744073709551615})";
    CHECK(json::write(json::parse(text)) == text);
}

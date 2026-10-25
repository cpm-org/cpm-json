#include "parser.hpp"

#include <format>
#include <string>
#include <vector>

#include "escape.hpp"
#include "key_tracker.hpp"
#include "lexer.hpp"
#include "number_text.hpp"

namespace cpm::json::detail {

namespace {

using Code = ParseError::Code;

// The grammar as a loop over tokens, with the open arrays and objects on a stack rather than in recursion, so depth
// costs no native stack.
class Parser {
public:
    Parser(std::string_view text, Handler& handler, const ParseOptions& options)
        : lexer_(text, options.comments), handler_(handler), options_(options),
          track_keys_(options.duplicate_keys == DuplicateKeys::error) {}

    void run() {
        Token token = lexer_.next();
        bool key_due = false;
        for (;;) {
            if (key_due) token = read_key(token);
            if (!read_value(token)) {
                // A container opened: `token` is its first element or key, or its end.
                key_due = open_.back() && token.type != TokenType::end_object;
                if (!at_close(token)) continue;
            }
            // A value is complete. Find what follows it, closing the containers that end there.
            if (!after_value(token, key_due)) return;
        }
    }

private:
    // Delivers the value that starts at `token`. Returns true for a complete value; false when it opened an array or
    // object, and then `token` is the token after the opening bracket.
    bool read_value(Token& token) {
        switch (token.type) {
            case TokenType::begin_array:
            case TokenType::begin_object: {
                const bool object = token.type == TokenType::begin_object;
                if (open_.size() == options_.max_depth) {
                    lexer_.fail(Code::too_deep, token.offset,
                                std::format("arrays and objects nest deeper than {} levels", options_.max_depth));
                }
                open_.push_back(object);
                if (object) {
                    if (track_keys_) keys_.open();
                    handler_.begin_object();
                } else {
                    handler_.begin_array();
                }
                token = lexer_.next();
                return false;
            }
            case TokenType::string: handler_.string(text_of(token)); return true;
            case TokenType::number: handler_.number(number_of(token)); return true;
            case TokenType::literal_true: handler_.boolean(true); return true;
            case TokenType::literal_false: handler_.boolean(false); return true;
            case TokenType::literal_null: handler_.null(); return true;
            default: expected(token, "a value");
        }
    }

    // Whether `token` closes the container just opened (an empty one), closing it if so.
    bool at_close(const Token& token) {
        if (token.type != (open_.back() ? TokenType::end_object : TokenType::end_array)) return false;
        close();
        return true;
    }

    // Reads a key and its ':'. Returns the token after the ':', where the member's value starts.
    Token read_key(const Token& token) {
        if (token.type != TokenType::string) expected(token, "a key (a string in double quotes)");
        const std::string_view key = text_of(token);
        if (track_keys_ && !keys_.add(key)) {
            lexer_.fail(Code::duplicate_key, token.offset, std::format("the object already has the key \"{}\"", key));
        }
        handler_.key(key);
        const Token colon = lexer_.next();
        if (colon.type != TokenType::colon) expected(colon, "':' after the key");
        return lexer_.next();
    }

    // After a complete value: reads what follows it, closing containers. Returns false when the top-level value is
    // complete; otherwise `token` starts the next element (key_due false) or member (key_due true).
    bool after_value(Token& token, bool& key_due) {
        for (;;) {
            if (open_.empty()) {
                const Token rest = lexer_.next();
                if (rest.type != TokenType::end) {
                    lexer_.fail(Code::trailing_content, rest.offset,
                                std::format("expected the end of the text after the value, found {}", name_of(rest)));
                }
                return false;
            }
            const bool object = open_.back();
            const TokenType end = object ? TokenType::end_object : TokenType::end_array;
            token = lexer_.next();
            if (token.type == end) {
                close();
                continue;
            }
            if (token.type != TokenType::comma) {
                expected(token, object ? "',' or '}' after an object member" : "',' or ']' after an array element");
            }
            token = lexer_.next();
            if (token.type == end) {
                if (!options_.trailing_commas) {
                    lexer_.fail(Code::unexpected_character, token.offset,
                                std::format("expected {} after ',', found {} (trailing commas need the jsonc options)",
                                            object ? "a key" : "a value", name_of(token)));
                }
                close();
                continue;
            }
            key_due = object;
            return true;
        }
    }

    void close() {
        if (open_.back()) {
            if (track_keys_) keys_.close();
            handler_.end_object();
        } else {
            handler_.end_array();
        }
        open_.pop_back();
    }

    std::string_view text_of(const Token& token) {
        if (!token.escaped) return token.text;
        decode_string(token.text, buffer_);
        return buffer_;
    }

    Number number_of(const Token& token) const {
        if (std::optional<Number> number = read_number(token.text)) return *number;
        lexer_.fail(Code::number_out_of_range, token.offset,
                    std::format("the number {} is too large for a double", token.text));
    }

    std::string name_of(const Token& token) const {
        switch (token.type) {
            case TokenType::string: return "a string";
            case TokenType::number: return "a number";
            case TokenType::literal_true: return "'true'";
            case TokenType::literal_false: return "'false'";
            case TokenType::literal_null: return "'null'";
            default: return lexer_.describe(token.offset);
        }
    }

    [[noreturn]] void expected(const Token& token, std::string_view what) const {
        const Code code = token.type == TokenType::end ? Code::unexpected_end : Code::unexpected_character;
        lexer_.fail(code, token.offset, std::format("expected {}, found {}", what, name_of(token)));
    }

    Lexer lexer_;
    Handler& handler_;
    const ParseOptions& options_;
    bool track_keys_;
    KeyTracker keys_;
    std::vector<bool> open_;  // the open containers, innermost last: true for objects
    std::string buffer_;      // decoded strings and keys
};

}  // namespace

void parse_events(std::string_view text, Handler& handler, const ParseOptions& options) {
    Parser(text, handler, options).run();
}

}  // namespace cpm::json::detail

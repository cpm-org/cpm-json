#include <cpm/json/pointer.hpp>

#include <charconv>
#include <format>
#include <optional>
#include <system_error>
#include <vector>

namespace cpm::json {

namespace {

// The unescaped tokens of `pointer`; throws PointerError when it isn't well formed.
std::vector<std::string> tokens_of(std::string_view pointer) {
    std::vector<std::string> tokens;
    if (pointer.empty()) return tokens;
    if (pointer.front() != '/') throw PointerError(std::format("a JSON Pointer must start with '/': \"{}\"", pointer));
    for (std::size_t i = 1;; ++i) {
        std::string token;
        for (; i < pointer.size() && pointer[i] != '/'; ++i) {
            if (pointer[i] != '~') {
                token += pointer[i];
            } else if (i + 1 < pointer.size() && (pointer[i + 1] == '0' || pointer[i + 1] == '1')) {
                token += pointer[++i] == '0' ? '~' : '/';
            } else {
                throw PointerError(std::format("in the JSON Pointer \"{}\", '~' must be followed by 0 or 1", pointer));
            }
        }
        tokens.push_back(std::move(token));
        if (i >= pointer.size()) return tokens;
    }
}

// The array index a token names: decimal digits without leading zeros. nullopt for anything else, "-" included.
std::optional<std::size_t> index_of(const std::string& token) {
    if (token.empty() || (token.size() > 1 && token.front() == '0')) return std::nullopt;
    std::size_t index = 0;
    auto [end, error] = std::from_chars(token.data(), token.data() + token.size(), index);
    if (error != std::errc() || end != token.data() + token.size()) return std::nullopt;
    return index;
}

template <class V>
V* resolve(V& root, std::string_view pointer) {
    V* value = &root;
    for (const std::string& token : tokens_of(pointer)) {
        if (value->is_object()) {
            value = value->as_object().find(token);
        } else if (value->is_array()) {
            std::optional<std::size_t> index = index_of(token);
            value = index && *index < value->as_array().size() ? &value->as_array()[*index] : nullptr;
        } else {
            value = nullptr;
        }
        if (value == nullptr) return nullptr;
    }
    return value;
}

}  // namespace

const Value* find(const Value& root, std::string_view pointer) { return resolve(root, pointer); }
Value* find(Value& root, std::string_view pointer) { return resolve(root, pointer); }

std::string escape_pointer_token(std::string_view token) {
    std::string escaped;
    escaped.reserve(token.size());
    for (char c : token) {
        if (c == '~') escaped += "~0";
        else if (c == '/') escaped += "~1";
        else escaped += c;
    }
    return escaped;
}

std::string make_pointer(std::initializer_list<std::string_view> tokens) {
    std::string pointer;
    for (std::string_view token : tokens) pointer += '/' + escape_pointer_token(token);
    return pointer;
}

}  // namespace cpm::json

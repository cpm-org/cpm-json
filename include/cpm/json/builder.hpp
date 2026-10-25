// Building a document from events (docs/spec.md §6).
#pragma once

#include <cpm/json/handler.hpp>
#include <cpm/json/read.hpp>
#include <cpm/json/value.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cpm::json {

// A Handler that builds a Value. Events out of order throw std::logic_error.
class DocumentBuilder final : public Handler {
public:
    // What a repeated key does: keep_first ignores the new member, keep_last replaces the value, and error throws
    // json::Error.
    explicit DocumentBuilder(DuplicateKeys duplicates = DuplicateKeys::keep_last) noexcept;

    void null() override;
    void boolean(bool value) override;
    void number(const Number& value) override;
    void string(std::string_view value) override;
    void begin_array() override;
    void end_array() override;
    void begin_object() override;
    void key(std::string_view key) override;
    void end_object() override;

    // Whether a whole value has been built.
    bool complete() const noexcept { return result_.has_value(); }

    // The value built; afterwards the builder takes a new one. Throws std::logic_error before a whole value.
    Value take();

private:
    struct Frame {
        Value container;
        std::string key;
        bool has_key = false;
    };

    void add(Value value);
    Frame& open_container(const char* event);

    std::vector<Frame> stack_;
    std::optional<Value> result_;
    DuplicateKeys duplicates_;
};

}  // namespace cpm::json

#include <cpm/json/builder.hpp>

#include <format>
#include <stdexcept>
#include <utility>

namespace cpm::json {

namespace {

[[noreturn]] void misuse(const char* what) { throw std::logic_error(std::format("cpm::json::DocumentBuilder: {}", what)); }

}  // namespace

DocumentBuilder::DocumentBuilder(DuplicateKeys duplicates) noexcept : duplicates_(duplicates) {}

void DocumentBuilder::null() { add(Value()); }
void DocumentBuilder::boolean(bool value) { add(Value(value)); }
void DocumentBuilder::number(const Number& value) { add(Value(value)); }
void DocumentBuilder::string(std::string_view value) { add(Value(value)); }

void DocumentBuilder::begin_array() {
    if (stack_.empty() && result_) misuse("a second top-level value");
    stack_.push_back({Value(Array()), {}, false});
}

void DocumentBuilder::begin_object() {
    if (stack_.empty() && result_) misuse("a second top-level value");
    stack_.push_back({Value(Object()), {}, false});
}

void DocumentBuilder::key(std::string_view name) {
    if (stack_.empty() || !stack_.back().container.is_object()) misuse("a key outside an object");
    Frame& frame = stack_.back();
    if (frame.has_key) misuse("a key where a value is due");
    frame.key.assign(name);
    frame.has_key = true;
}

void DocumentBuilder::end_array() {
    if (stack_.empty() || !stack_.back().container.is_array()) misuse("end_array without a matching begin_array");
    Value array = std::move(stack_.back().container);
    stack_.pop_back();
    add(std::move(array));
}

void DocumentBuilder::end_object() {
    if (stack_.empty() || !stack_.back().container.is_object()) misuse("end_object without a matching begin_object");
    if (stack_.back().has_key) misuse("end_object where a value is due");
    Value object = std::move(stack_.back().container);
    stack_.pop_back();
    add(std::move(object));
}

void DocumentBuilder::add(Value value) {
    if (stack_.empty()) {
        if (result_) misuse("a second top-level value");
        result_ = std::move(value);
        return;
    }
    Frame& frame = stack_.back();
    if (frame.container.is_array()) {
        frame.container.as_array().push_back(std::move(value));
        return;
    }
    if (!frame.has_key) misuse("a value where a key is due");
    frame.has_key = false;
    Object& object = frame.container.as_object();
    if (duplicates_ != DuplicateKeys::keep_last && object.contains(frame.key)) {
        if (duplicates_ == DuplicateKeys::keep_first) return;
        throw Error(std::format("the object already has the key \"{}\"", frame.key));
    }
    object.insert_or_assign(std::move(frame.key), std::move(value));
}

Value DocumentBuilder::take() {
    if (!result_) misuse("take() before a whole value");
    Value value = std::move(*result_);
    result_.reset();
    return value;
}

}  // namespace cpm::json

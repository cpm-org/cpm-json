#include <cpm/json/write.hpp>

#include <format>
#include <ostream>
#include <stdexcept>

#include "escape.hpp"
#include "number_text.hpp"

namespace cpm::json {

namespace {

// Text reaches the sink in pieces of about this size while a large value is written.
constexpr std::size_t flush_size = 16 * 1024;

[[noreturn]] void misuse(const char* event, const char* what) {
    throw std::logic_error(std::format("cpm::json::Writer: {} {}", event, what));
}

}  // namespace

void StringSink::write(std::string_view text) { out_.append(text); }

void StreamSink::write(std::string_view text) {
    out_.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!out_) throw WriteError("writing JSON to the stream failed");
}

Writer::Writer(Sink& sink, const WriteOptions& options) : sink_(sink), options_(options) {
    if (options_.indent < 0) throw std::invalid_argument("cpm::json::Writer: indent can't be negative");
}

void Writer::flush() {
    if (buffer_.empty()) return;
    sink_.write(buffer_);
    buffer_.clear();
}

void Writer::new_line(std::size_t depth) {
    if (options_.indent == 0) return;
    buffer_ += '\n';
    buffer_.append(depth * static_cast<std::size_t>(options_.indent), ' ');
}

// Checks that a value may come now, and writes what separates it from the one before.
void Writer::before_value(const char* event) {
    if (complete_) misuse(event, "after the top-level value is complete");
    if (stack_.empty()) return;
    Frame& frame = stack_.back();
    if (frame.object) {
        if (!frame.key_written) misuse(event, "where a key is due");
        return;  // the key and its ':' are written
    }
    if (!frame.empty) buffer_ += ',';
    new_line(stack_.size());
}

void Writer::after_value() {
    if (stack_.empty()) {
        complete_ = true;
        flush();
        return;
    }
    Frame& frame = stack_.back();
    frame.empty = false;
    frame.key_written = false;
    if (buffer_.size() >= flush_size) flush();
}

void Writer::null() {
    before_value("null()");
    buffer_ += "null";
    after_value();
}

void Writer::boolean(bool value) {
    before_value("boolean()");
    buffer_ += value ? "true" : "false";
    after_value();
}

void Writer::number(const Number& value) {
    before_value("number()");
    detail::append_number(buffer_, value);
    after_value();
}

void Writer::string(std::string_view value) {
    const std::size_t before = buffer_.size();
    before_value("string()");
    try {
        detail::append_quoted(buffer_, value, options_.escape_non_ascii);
    } catch (...) {
        buffer_.resize(before);  // the separator goes too, so the writer can carry on
        throw;
    }
    after_value();
}

void Writer::begin_array() {
    before_value("begin_array()");
    buffer_ += '[';
    stack_.push_back({.object = false});
}

void Writer::begin_object() {
    before_value("begin_object()");
    buffer_ += '{';
    stack_.push_back({.object = true});
}

void Writer::key(std::string_view name) {
    if (stack_.empty() || !stack_.back().object) misuse("key()", "outside an object");
    Frame& frame = stack_.back();
    if (frame.key_written) misuse("key()", "where a value is due");
    const std::size_t before = buffer_.size();
    if (!frame.empty) buffer_ += ',';
    new_line(stack_.size());
    try {
        detail::append_quoted(buffer_, name, options_.escape_non_ascii);
    } catch (...) {
        buffer_.resize(before);  // the separator goes too, so the writer can carry on
        throw;
    }
    buffer_ += options_.indent > 0 ? ": " : ":";
    frame.key_written = true;
}

void Writer::close(bool object, const char* event) {
    if (stack_.empty() || stack_.back().object != object) {
        misuse(event, object ? "without a matching begin_object()" : "without a matching begin_array()");
    }
    if (stack_.back().key_written) misuse(event, "where a value is due");
    const bool empty = stack_.back().empty;
    stack_.pop_back();
    if (!empty) new_line(stack_.size());
    buffer_ += object ? '}' : ']';
    after_value();
}

void Writer::end_array() { close(false, "end_array()"); }
void Writer::end_object() { close(true, "end_object()"); }

}  // namespace cpm::json

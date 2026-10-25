// Writing JSON text, from documents or from events (docs/spec.md §5).
#pragma once

#include <cpm/json/handler.hpp>
#include <cpm/json/number.hpp>
#include <cpm/json/value.hpp>

#include <cstddef>
#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

namespace cpm::json {

struct WriteOptions {
    int indent = 0;                 // 0: compact. Otherwise this many spaces per level, one element or member per line
    bool sort_keys = false;         // for documents: members in byte order of their keys, instead of their own order
    bool escape_non_ascii = false;  // code points above U+007F as \uXXXX (surrogate pairs above U+FFFF)
};

// Where written text goes.
class Sink {
public:
    virtual ~Sink() = default;
    virtual void write(std::string_view text) = 0;

protected:
    Sink() = default;
    Sink(const Sink&) = default;
    Sink& operator=(const Sink&) = default;
};

// Appends to a string.
class StringSink final : public Sink {
public:
    explicit StringSink(std::string& out) noexcept : out_(out) {}
    void write(std::string_view text) override;

private:
    std::string& out_;
};

// Writes to a stream; throws WriteError when the stream fails.
class StreamSink final : public Sink {
public:
    explicit StreamSink(std::ostream& out) noexcept : out_(out) {}
    void write(std::string_view text) override;

private:
    std::ostream& out_;
};

// Writes JSON from events, without a document: as the handler of a parse it reformats text, and it produces large
// documents piece by piece. Text reaches the sink when a top-level value is complete, when enough has gathered, and
// on flush(). Calls out of order throw std::logic_error; a string that isn't valid UTF-8 throws WriteError and writes
// nothing. Keys come out in the order they are given (WriteOptions::sort_keys applies to documents).
class Writer final : public Handler {
public:
    // Throws std::invalid_argument for a negative indent.
    explicit Writer(Sink& sink, const WriteOptions& options = {});

    void null() override;
    void boolean(bool value) override;
    void number(const Number& value) override;
    void string(std::string_view value) override;
    void begin_array() override;
    void end_array() override;
    void begin_object() override;
    void key(std::string_view key) override;
    void end_object() override;

    template <detail::integer T>
    void number(T value) {
        number(Number(value));
    }
    void number(double value) { number(Number(value)); }

    // Whether a whole top-level value has been written.
    bool complete() const noexcept { return complete_; }

    // Passes what has been written so far to the sink.
    void flush();

private:
    struct Frame {
        bool object = false;
        bool empty = true;
        bool key_written = false;
    };

    void before_value(const char* event);
    void after_value();
    void close(bool object, const char* event);
    void new_line(std::size_t depth);

    Sink& sink_;
    WriteOptions options_;
    std::string buffer_;
    std::vector<Frame> stack_;
    bool complete_ = false;
};

// The text of `value`.
std::string write(const Value& value, const WriteOptions& options = {});

// Writes `value` to `sink`.
void write(const Value& value, Sink& sink, const WriteOptions& options = {});

}  // namespace cpm::json

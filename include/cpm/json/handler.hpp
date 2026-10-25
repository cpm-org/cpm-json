// Events: JSON as a sequence of calls, for reading and writing without a document (docs/spec.md §6).
#pragma once

#include <cpm/json/number.hpp>

#include <string_view>

namespace cpm::json {

// Receives a JSON value as events, in document order. Every object member is key() followed by its value. The views
// passed to string() and key() are valid only during the call. To stop early, throw: producers let the exception pass.
//
// Producers: parse(text, handler) and emit(value, handler). Consumers: DocumentBuilder and Writer.
class Handler {
public:
    virtual ~Handler() = default;

    virtual void null() = 0;
    virtual void boolean(bool value) = 0;
    virtual void number(const Number& value) = 0;
    virtual void string(std::string_view value) = 0;
    virtual void begin_array() = 0;
    virtual void end_array() = 0;
    virtual void begin_object() = 0;
    virtual void key(std::string_view key) = 0;
    virtual void end_object() = 0;

protected:
    Handler() = default;
    Handler(const Handler&) = default;
    Handler& operator=(const Handler&) = default;
};

}  // namespace cpm::json

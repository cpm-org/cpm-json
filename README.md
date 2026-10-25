# cpm-json

JSON for C++20, made for [CPM](https://github.com/meirborowski/CPM). It reads JSON, and JSONC with comments and trailing commas, into documents you can query and change. It writes them back compact or indented, and converts between JSON and your own types. Errors say where the problem is.

```sh
cpm install cpm-json
```

```cpp
#include <cpm/json.hpp>

namespace json = cpm::json;

json::Value config = json::parse(R"({
    // where the service listens
    "host": "localhost",
    "port": 8080,
    "tags": ["api", "internal",],
})", json::jsonc);

int port = config.at("port").get<int>();
config.as_object().insert_or_assign("debug", true);

std::string text = json::write(config, {.indent = 2});
```

When text isn't JSON, the error gives the line and column:

```
line 4, column 5: expected ',' or '}' after an object member, found a string
```

When a value isn't what the program expects, the error gives its path in the document:

```
at /servers/2/port: expected an integer from 0 to 65535, found 70000
```

The [specification](docs/spec.md) has every rule in detail.

## Reading

```cpp
json::Value v = json::parse(text);                     // strict JSON, RFC 8259
json::Value c = json::parse(text, json::jsonc);        // comments and trailing commas too
json::ParseResult r = json::try_parse(text);           // returns the error instead of throwing it
```

`parse` throws `json::ParseError`. Its `what()` is the location and a sentence. `code()`, `location()` (offset, line and column) and `reason()` give the parts. `try_parse` returns a `ParseResult`, which converts to `true` when the text parsed. Then `value()` gives the value; otherwise `error()` gives the `ParseError`.

`ParseOptions` controls what is accepted:

| Option | Default | |
|---|---|---|
| `comments` | `false` | `//` to the end of the line, and `/* ... */` |
| `trailing_commas` | `false` | one comma after the last element or member |
| `max_depth` | `512` | how deeply arrays and objects may nest |
| `duplicate_keys` | `error` | `error`, `keep_first` or `keep_last` |

`json::jsonc` turns on comments and trailing commas. Everything else is strict:
- numbers follow JSON's grammar (no `01`, `.5`, `+1` or `NaN`);
- strings must be valid UTF-8, and characters below U+0020 must be escaped;
- a leading byte order mark is skipped.

## Values

A `json::Value` is null, a boolean, a number, a string, an array or an object. Build values from C++ values:

```cpp
json::Value v = json::Object{
    {"name", "cpm"},
    {"stars", 42},
    {"tags", json::Array{"c++", "json"}},
    {"license", nullptr},
};
```

- `kind()` and `is_string()` and the like say which kind a value is.
- `as_boolean()`, `as_number()`, `as_string()`, `as_array()` and `as_object()` give the contents, or throw `json::TypeError` for another kind: `expected a string, found 42`.
- On objects, `find(key)` returns a pointer or `nullptr`, and `at(key)` throws `json::LookupError` when the key is missing. On arrays, `find(index)` and `at(index)` do the same.

**Objects** keep their members in the order they were added, and find any key in constant time on average:
- `insert_or_assign(key, value)` adds a member at the end, or replaces the value of an existing one, which keeps its position.
- `erase(key)` removes a member, keeping the others' order.
- Iterating gives members, with `member.key()` and `member.value`. Values can be changed through it, keys can't.

**Arrays** are `std::vector<json::Value>`. An array whose only element is an array needs `json::Array{json::Value(inner)}`, because some compilers read `json::Array{inner}` as a copy of `inner`.

**Numbers** are exact for 64-bit integers and doubles otherwise. `to_integer<T>()` gives a whole number that fits in `T`, or `nullopt`. `to_double()` gives the nearest double.

**Equality:** numbers compare by value (`1 == 1.0`), and objects compare regardless of member order, since JSON objects are unordered.

**JSON Pointer** ([RFC 6901](https://www.rfc-editor.org/rfc/rfc6901)) finds a value by path:

```cpp
if (const json::Value* name = json::find(v, "/servers/0/name")) { ... }
```

## Writing

```cpp
std::string compact = json::write(v);                      // {"name":"cpm","stars":42,...}
std::string pretty = json::write(v, {.indent = 2});        // one member per line
std::string sorted = json::write(v, {.sort_keys = true});  // members in key order
```

Doubles are written in the shortest form that reads back as the same double, and keep a `.0` when they are whole (`1.0`), so they read back as doubles. `escape_non_ascii` writes everything above ASCII as `\uXXXX`. A string that isn't valid UTF-8 can't be written and throws `json::WriteError`.

To write to something other than a string, pass a `json::Sink`. `json::StreamSink` writes to a `std::ostream`:

```cpp
json::StreamSink out(std::cout);
json::write(v, out, {.indent = 2});
```

## Events

`json::Handler` receives JSON as a sequence of calls, so programs can read and write JSON without building a document:
- `parse(text, handler)` delivers what it reads to a handler as it goes.
- `json::emit(value, handler)` walks a document.
- `json::DocumentBuilder` builds a document from events.
- `json::Writer` writes text from them.

Generate a large document piece by piece with a `Writer`:

```cpp
json::StreamSink sink(file);
json::Writer w(sink);
w.begin_array();
for (const Row& row : rows) {
    w.begin_object();
    w.key("id");
    w.number(row.id);
    w.key("name");
    w.string(row.name);
    w.end_object();
}
w.end_array();
```

A `Writer` is also a handler, so `json::parse(text, writer, json::jsonc)` reformats text: it drops the comments and lays the text out the writer's way.

## Your own types

Specialise `json::Traits` to convert a type. A specialisation may give either direction, or both:

```cpp
template <>
struct cpm::json::Traits<Point> {
    static Value to_json(const Point& p) { return Object{{"x", p.x}, {"y", p.y}}; }
    static Point from_json(const Value& v) { return {v.at("x").get<int>(), v.at("y").get<int>()}; }
};

json::Value v = json::to_json(Point{1, 2});
Point p = v.get<Point>();
```

These are built in:

| Types | JSON |
|---|---|
| `bool`, integers, `float`, `double` | booleans and numbers. Integers must be whole and in range. |
| `std::string` | strings. `std::string_view` and `const char*` too, for writing only. |
| `std::optional<T>` | the value, or null |
| `std::vector<T>` | arrays |
| `std::array<T, N>`, `std::pair`, `std::tuple` | arrays of exactly that length |
| `std::map<std::string, T>`, `std::unordered_map<std::string, T>` | objects |

When converting an element or member fails, the built-in conversions add its index or key to the error's path: `at /servers/2/port: ...`. A `Traits` of your own can do the same with `TypeError`'s constructor that takes a key:

```cpp
try {
    settings.port = v.at("port").get<std::uint16_t>();
} catch (const json::TypeError& error) {
    throw json::TypeError(error, "port");
}
```

[examples/consumer](examples/consumer) reads a settings file this way.

## Errors

Every exception for bad data derives from `json::Error`, which derives from `std::runtime_error`:

| Exception | Thrown for |
|---|---|
| `ParseError` | text that isn't JSON |
| `TypeError` | a value of another kind than asked for, or a conversion that doesn't fit |
| `LookupError` | a missing member, or an index out of range, from `at()` |
| `PointerError` | a JSON Pointer that isn't well formed |
| `WriteError` | a string that isn't UTF-8, or a stream that fails |

Calling a `Writer` or `DocumentBuilder` out of order throws `std::logic_error`.

## Platforms

cpm-json is tested on Linux (GCC and Clang), macOS (Apple Clang) and Windows (MinGW GCC). On macOS, writing doubles needs macOS 13.3 or newer as the deployment target, for `std::to_chars`.

# cpm-json 0.1.0: specification

cpm-json reads and writes JSON in C++20. It is a [cpm-org](https://github.com/cpm-org) library: original code with its own API, built and tested with [CPM](https://github.com/meirborowski/CPM). This document is what the code is written against. The README will be the user's guide.

## 1. Scope

0.1.0 covers:

- **Documents:** a `Value` that holds any JSON value, with typed access, lookup and building.
- **Reading:** strict JSON ([RFC 8259](https://www.rfc-editor.org/rfc/rfc8259)), and JSONC (comments and trailing commas) on request, with errors that give the line and column.
- **Writing:** compact or indented text, with keys in their own order or sorted.
- **Events:** one `Handler` interface for reading and writing without a document. The parser produces events, and a document builder and the writer consume them.
- **JSON Pointer** ([RFC 6901](https://www.rfc-editor.org/rfc/rfc6901)) lookup.
- **Conversions** between JSON and C++ types through a `Traits<T>` customization point, with the standard types built in. A failed conversion says where in the document it failed.

Not in 0.1.0:

- Editing text while keeping its comments and layout.
- JSON5 (single quotes, unquoted keys, hexadecimal numbers, `NaN`).
- Numbers beyond 64-bit integers and doubles.
- Reading input in pieces. The whole text is in memory.
- Remembering where in the text a value came from.
- Mapping structs automatically, schemas, and binary formats such as CBOR.

## 2. The package

| | |
|---|---|
| Repository | `github.com/cpm-org/cpm-json`, MIT |
| Registry name | `cpm-json` |
| Header | `<cpm/json.hpp>`, which includes the parts in `<cpm/json/*.hpp>` |
| Namespace | `cpm::json` |
| Standard | `"std": "c++20"` |
| Type | `static-lib` |
| Tests | `tests/*.test.cpp` on cpm-test, run with `cpm test` |
| CI | Linux (GCC and Clang), macOS (Apple Clang) and Windows (MinGW GCC), with CPM installed by its installer: strict warnings as errors, release, and AddressSanitizer with UBSan on Linux |

It depends on nothing but the standard library, and the test framework as a devDependency.

## 3. Data model

### 3.1 Values

A `Value` is one of six kinds:

```cpp
enum class Kind { null, boolean, number, string, array, object };
```

| Kind | Holds |
|---|---|
| `null` | nothing. A default-constructed `Value` is null. |
| `boolean` | `bool` |
| `number` | a `Number` (§3.2) |
| `string` | a `std::string` of UTF-8 text (§3.3) |
| `array` | an `Array`, which is `std::vector<Value>` |
| `object` | an `Object` (§3.4) |

`Value` is a regular type: copyable, movable (moves are `noexcept`), and equality-comparable (§3.5).

### 3.2 Numbers

A `Number` holds one of three representations, chosen so that integers stay exact:

| Representation | Holds | Read from |
|---|---|---|
| `int64` | `std::int64_t` | an integer token (no fraction, no exponent) from INT64_MIN to INT64_MAX |
| `uint64` | `std::uint64_t` above INT64_MAX | an integer token above INT64_MAX that fits in 64 bits |
| `float64` | `double` | any other token, rounded to the nearest double |

- Constructing a `Number` from an unsigned value up to INT64_MAX stores `int64`, so equal integers have one representation.
- An integer token too large for 64 bits reads as the nearest double.
- A token whose magnitude is too large for a double (`1e400`) is an error, `number_out_of_range`. Tokens too small for a double round to zero.
- `-0` reads as the double `-0.0`, which keeps its sign. Every other integer token reads as an integer.
- Doubles in a `Value` are always finite. Constructing a `Number` from NaN or infinity throws `std::domain_error`.

Reading a number:
- `to_integer<T>()` returns `std::optional<T>`: the value when it is a whole number within `T`'s range, a whole-valued double included, else `nullopt`.
- `to_double()` returns the nearest double.

Decimal text is converted with correct rounding and doesn't depend on the C locale. Where the standard library has floating-point `std::from_chars`, it is used. Apple's libc++ doesn't have it, so there the conversion falls back to `strtod_l` with the `"C"` locale (`_strtod_l` on Windows).

### 3.3 Strings

- Strings are UTF-8. Reading rejects malformed UTF-8 anywhere in the text, comments included: overlong forms, surrogates encoded in UTF-8, code points above U+10FFFF, and truncated sequences.
- `\uXXXX` escapes are decoded. A surrogate pair becomes one code point. A lone surrogate (`"\ud800"`) is an error, `invalid_escape`.
- A string can contain U+0000, through `\u0000`.
- A `Value` built in code can hold any bytes. Writing a string that isn't valid UTF-8 throws `WriteError` (§5).

### 3.4 Objects

An `Object` is a sequence of members with unique keys, kept in the order they were added. A `Member` is made from a key and a value. `member.key()` reads the key, which can't change once the member is in an object, and `member.value` is the value.

| Operation | Effect |
|---|---|
| `find(key)` | a pointer to the member's value, or `nullptr` |
| `contains(key)` | whether the key is there |
| `at(key)` | the value. A missing key throws `LookupError`. |
| `insert_or_assign(key, value)` | adds the member at the end, or replaces the value of the existing one, which keeps its position. Returns a reference to the value. |
| `erase(key)` | removes the member, keeping the others' order. Returns whether there was one. |
| `size()`, `empty()`, `clear()` | as for containers |
| iteration | the members in order |
| `Object{{"a", 1}, {"b", true}}` | an object built from members. A repeated key keeps the last value, at the first one's position. |

Finding, adding and replacing take constant time on average, at any size. Erasing takes time linear in the number of members. Const operations change nothing, so concurrent reads are safe.

### 3.5 Equality

Two values are equal when they are of the same kind and:
- **numbers** have the same mathematical value, whatever their representation. `Number(1) == Number(1.0)`, and `Number(-0.0) == Number(0)`.
- **strings** have the same bytes.
- **arrays** have equal elements in the same order.
- **objects** have the same keys with equal values, in any order. JSON objects are unordered, so order doesn't count.

## 4. Reading

### 4.1 Functions

```cpp
Value parse(std::string_view text, const ParseOptions& options = {});               // throws ParseError
ParseResult try_parse(std::string_view text, const ParseOptions& options = {});     // never throws ParseError
void parse(std::string_view text, Handler& handler, const ParseOptions& options = {});  // events (§6)
```

- `try_parse` returns a `ParseResult`. It converts to `true` when the text parsed. Then `value()` gives the value; otherwise `error()` gives the `ParseError`, and `value()` throws it.
- With a handler, events are delivered as the text is read, so an error can come after some events. Exceptions the handler throws pass through unchanged.

### 4.2 Options

```cpp
enum class DuplicateKeys { error, keep_first, keep_last };

struct ParseOptions {
    bool comments = false;         // `// to the end of the line` and `/* ... */` (they don't nest)
    bool trailing_commas = false;  // one comma after the last element or member
    std::size_t max_depth = 512;   // nesting of arrays and objects; deeper is the error too_deep
    DuplicateKeys duplicate_keys = DuplicateKeys::error;
};

inline constexpr ParseOptions jsonc{.comments = true, .trailing_commas = true};
```

- Comments count as whitespace, wherever whitespace may appear.
- `duplicate_keys` applies to documents. With a handler, every member is reported and the handler decides, so only `error` changes anything there.

### 4.3 Grammar

The text is one JSON value, as RFC 8259 defines it, with optional whitespace around it:

- Whitespace is space, tab, line feed and carriage return.
- A leading UTF-8 byte order mark is skipped.
- Literals are exactly `true`, `false` and `null`.
- Numbers match `-?(0|[1-9][0-9]*)(\.[0-9]+)?([eE][+-]?[0-9]+)?`. So `01`, `1.`, `.5`, `+1`, `Infinity` and `NaN` are errors, with comments on or off.
- Strings are in double quotes. The escapes are `\" \\ \/ \b \f \n \r \t \uXXXX`. Characters below U+0020 must be escaped.
- Anything after the value other than whitespace (and comments, with `comments`) is the error `trailing_content`.

### 4.4 Errors

`ParseError` derives from `json::Error` (§7). It has:

- `code()`: one of `unexpected_end`, `unexpected_character`, `invalid_number`, `number_out_of_range`, `invalid_escape`, `invalid_utf8`, `control_character`, `duplicate_key`, `too_deep`, `trailing_content`, `unterminated_comment`;
- `location()`: a `Location { std::size_t offset, line, column; }`. `offset` is in bytes from the start of the text. `line` and `column` start at 1. Columns count code points, so a tab or an `é` is one column. A line ends at LF, CR LF or a lone CR;
- `what()`: the location and a sentence, such as `line 3, column 14: expected ',' or '}' after an object member, found ']'`.

The location is where the problem starts: the first byte of an invalid number or escape, the second occurrence of a duplicate key, and the end of the text for `unexpected_end`.

## 5. Writing

### 5.1 Functions and options

```cpp
struct WriteOptions {
    int indent = 0;                 // 0: compact. Otherwise this many spaces per level, one element or member per line
    bool sort_keys = false;         // members in byte order of their keys, instead of their own order
    bool escape_non_ascii = false;  // write code points above U+007F as \uXXXX (surrogate pairs above U+FFFF)
};

std::string write(const Value& value, const WriteOptions& options = {});
void write(const Value& value, Sink& sink, const WriteOptions& options = {});
```

### 5.2 Format

- **Compact** text has no whitespace: `{"name":"cpm","tags":["c++","json"],"stars":null}`.
- **Indented** text puts each element and member on its own line, indented by `indent` spaces per level, with `": "` after keys. Empty arrays and objects stay `[]` and `{}`. There is no newline after the last line.

  ```json
  {
    "name": "cpm",
    "tags": [
      "c++",
      "json"
    ],
    "stars": null
  }
  ```

- **Integers** are written exactly.
- **Doubles** are written in the shortest form that reads back as the same double, through `std::to_chars`. When that form has neither a `.` nor an exponent, `.0` is added (`1.0`, `-0.0`), so it reads back as a double rather than an integer. `1e+300` stays as it is.
- **Strings** escape `"` and `\`, and characters below U+0020: as `\b \f \n \r \t`, and the others as `\u00XX` with lowercase hex. `/` and U+007F aren't escaped. Other text is written as it is, or as `\uXXXX` with `escape_non_ascii`.
- A string that isn't valid UTF-8 throws `WriteError`. Doubles can't be NaN or infinite (§3.2), so every `Value` can be written otherwise.

### 5.3 Streaming writer and sinks

`Writer` writes JSON from events (§6) without a document, so it can be the handler of a parse, which reformats text. It is also the API for producing large documents piece by piece:

```cpp
std::string out;
StringSink sink(out);
Writer w(sink, {.indent = 2});
w.begin_object();
w.key("ids");
w.begin_array();
for (int id : ids) w.number(id);
w.end_array();
w.end_object();
```

- Calls out of order throw `std::logic_error`, since they are bugs in the caller. Examples: a value where a key is due, a key outside an object, an end that matches nothing, or a second top-level value.
- `Writer` checks UTF-8 as §5.2 says, and has `number` overloads for the integer types, `double` and `Number`.
- `Sink` is an interface with one function, `void write(std::string_view text)`. `StringSink` appends to a `std::string`. `StreamSink` writes to a `std::ostream`, and throws `WriteError` when the stream fails.

## 6. Events

```cpp
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
};
```

- A parse delivers events in document order. Every object member is `key` followed by its value. The views passed to `string` and `key` are valid only during the call.
- **Producers:** `parse(text, handler)`, and `emit(const Value&, Handler&, bool sort_keys = false)`, which walks a document. Objects come in their own order, or by key with `sort_keys`.
- **Consumers:** `DocumentBuilder` builds a `Value` (`take()` returns it after a complete value), and `Writer`.
- To stop early, a handler throws, and the exception passes through the producer.

## 7. Errors

Every exception cpm-json throws for bad data derives from `cpm::json::Error`, which derives from `std::runtime_error`:

| Exception | When |
|---|---|
| `ParseError` | the text isn't JSON (§4.4) |
| `TypeError` | a value isn't of the kind asked for: `as_string()` on a number, a conversion that doesn't fit (§9). `what()` says what was expected and what was found. |
| `LookupError` | `at(key)` finds no member, or `at(index)` is out of range |
| `PointerError` | a JSON Pointer isn't well formed (§8) |
| `WriteError` | text can't be written (§5) |

Misusing the API (§5.3) throws `std::logic_error`, and constructing a non-finite `Number` throws `std::domain_error`. Memory exhaustion throws `std::bad_alloc` as usual.

## 8. Access

`Value` has:

- `kind()`, and `is_null()`, `is_boolean()`, `is_number()`, `is_string()`, `is_array()`, `is_object()`.
- `as_boolean()`, `as_number()`, `as_string()`, `as_array()` and `as_object()`, const and non-const, which throw `TypeError` for another kind.
- `find(key)` and `at(key)` on objects, and `find(index)` and `at(index)` on arrays. `find` returns `nullptr` when there is nothing there. On a value of another kind, all four throw `TypeError`.
- `get<T>()`, which converts through `Traits<T>` (§9).

Values are constructed from `bool`, any integer type, `double` and `float`, `Number`, strings (`std::string`, `std::string_view`, `const char*`), `Array`, `Object` and `nullptr`. Character pointers make strings, never booleans.

**JSON Pointer:** `find(const Value& root, std::string_view pointer)`, with a non-const overload, resolves an RFC 6901 pointer such as `/servers/0/name`:
- `""` is the root itself, and `~0` and `~1` in a token stand for `~` and `/`.
- A pointer that names nothing, including a step into a value that isn't an array or object, returns `nullptr`. Array indexes are decimal without leading zeros, and `-` (past the end) names nothing.
- A pointer that isn't well formed throws `PointerError`: it doesn't start with `/`, or a `~` isn't followed by `0` or `1`.
- `make_pointer(tokens)` builds a pointer, escaping the tokens.

## 9. Conversions

### 9.1 Traits

A type takes part by specialising `Traits`:

```cpp
template <class T>
struct Traits;  // no definition: a type without a specialisation doesn't convert

template <>
struct cpm::json::Traits<Point> {
    static Value to_json(const Point& p) { return Object{{"x", p.x}, {"y", p.y}}; }
    static Point from_json(const Value& v) { return {v.at("x").get<int>(), v.at("y").get<int>()}; }
};
```

A specialisation may define one direction only. The free functions use it:
- `to_json(const T&) -> Value`;
- `from_json<T>(const Value&) -> T`, which `Value::get<T>()` calls.

They are constrained, so using a direction a type lacks doesn't compile.

### 9.2 Built in

| C++ type | JSON |
|---|---|
| `Value`, `Array`, `Object`, `Number` | themselves |
| `bool` | boolean |
| integer types (not `bool` or character types) | number. Reading needs a whole number in the type's range. |
| `float`, `double` | number. Reading takes any number. A `float` that would overflow is a `TypeError`. |
| `std::string` | string |
| `std::string_view`, `const char*` | string (writing only) |
| `std::nullptr_t` | null |
| `std::optional<T>` | `T`, or null for `nullopt`. Null reads as `nullopt`. |
| `std::vector<T>` | array |
| `std::array<T, N>` | array of exactly `N` elements |
| `std::pair<A, B>`, `std::tuple<T...>` | array of exactly that many elements |
| `std::map<std::string, T>`, `std::unordered_map<std::string, T>` | object |

Enums aren't built in, since a JSON form (a name or a number) is the program's choice. A `Traits` specialisation gives them one.

### 9.3 Where a conversion failed

A `TypeError` from a conversion carries the path to the value that failed, as a JSON Pointer in `path()`. The built-in conversions extend it as the error passes up through arrays and objects. `what()` includes it:

```
at /servers/2/port: expected an integer from 0 to 65535, found 70000
```

`TypeError` has a constructor that takes a path token, so a user's `Traits` can extend the path the same way.

## 10. Structure

Each part has one job and sits behind a small interface. The logic does no I/O, uses no global state, and can be tested directly.

| Part | Job |
|---|---|
| `utf8` | validate, decode and encode UTF-8 |
| `location` | turn a byte offset into a line and a column |
| `number_text` | number tokens to `Number`, and `Number` to text. The one place that differs by platform (§3.2). |
| `escape` | decode string escapes when reading, and escape strings when writing |
| `lexer` | split text into tokens, skipping whitespace and comments, with offsets |
| `parser` | the grammar: tokens to `Handler` events, depth and trailing commas |
| `key_tracker` | the keys of each open object, for `DuplicateKeys::error` |
| `document_builder` | `Handler` events to a `Value` |
| `emit` | a `Value` to `Handler` events |
| `writer` | `Handler` events to text through a `Sink`, checking the order of calls |
| `object` | member storage with insertion order and a hash index |
| `value`, `number` | the data model |
| `pointer` | parse, escape and resolve JSON Pointers |
| `traits` | the customization point and the built-in conversions, in the header |

`parse(text)` is `parse(text, builder)` with a `DocumentBuilder`, and `write(value)` is `emit(value, writer)`. Reading and writing documents go through the same parser and writer as the streaming API.

## 11. Testing

- Each part has its own tests. The parser has accepted and rejected cases for every rule in §4.3, with the exact error code and location.
- The tests cover:
  - number boundaries: INT64_MIN, INT64_MAX, UINT64_MAX and one above it, `-0`, `1e308`, `1e309` and denormals;
  - the shortest double output on round trips;
  - UTF-8 edge cases;
  - each `DuplicateKeys` policy;
  - nesting at and over `max_depth`.
- **Round trips:** randomly generated documents, written compact and indented, read back equal.
- The example project under `examples/consumer` uses cpm-json through a path dependency, so CI checks it the way users use it.

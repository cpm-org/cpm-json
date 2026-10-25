# Changelog

## 0.1.0

The first release.

- `json::Value`, holding null, booleans, numbers, strings, arrays and objects. Numbers are exact 64-bit integers or doubles. Objects keep insertion order, with constant-time lookup at any size.
- `parse` and `try_parse` for strict JSON, and `jsonc` for comments and trailing commas. Options cover nesting depth and duplicate keys. Errors give a code, the line and column, and a sentence.
- `write`, compact or indented, with sorted keys and `\u` escapes as options. Doubles round-trip in their shortest form.
- `json::Handler` events: `parse(text, handler)` and `emit(value, handler)` produce them, and `DocumentBuilder` and the streaming `Writer` consume them. `Sink`, `StringSink` and `StreamSink` take the output.
- JSON Pointer: `find(value, "/a/0")`, `escape_pointer_token` and `make_pointer`.
- `json::Traits<T>` conversions, with the standard types built in. Errors give the path to the value that failed.

# JSON

📘 **Public API wiki** · 🛠 Handbook: [Core module](../../handbook/modules/core.md)

Read, build and write JSON for configuration, save games, scene text and tool protocols. Include
`conjure/core/json/json.h` to get the whole area (`conjure::core::json`); it pulls in
`conjure/core/json/value.h` (read-only views), `conjure/core/json/document.h` (owning, mutable trees) and
`conjure/core/json/writer.h` (streaming output) and adds the free functions `json::parse()` and
`json::to_string()`.

## At a glance

| You want to... | Use |
|---|---|
| Parse text | `json::parse(text)` or `json::document::parse(text)` |
| Parse or write a file | `json::document::parse_file(path)`, `doc.write_file(path, pretty)` |
| Read a setting with a default | `doc.root()["window"]["width"].as_i64(1280)` |
| Read a value and know why it failed | `value.to<T>()` (`result<T>`) |
| Loop over an array or object | `for (json::value item : array)`, `for (auto [key, item] : object.members())` |
| Build a small tree in one expression | `json::object({{"a", 1}})`, `json::array({1, 2})` |
| Add, replace or remove members | `doc.set(key, item)`, `doc.push_back(item)`, `doc.erase(key)` |
| Address a nested value by path | `doc.at_pointer("/a/b/0")`, `doc.set_pointer("/a/b/0", 5)` |
| Apply a partial update | `doc.merge_patch(patch)` (RFC 7386) |
| Text for humans | `doc.to_string(true)` (4-space indentation) |
| Text for hashing, diffing or tests | `doc.to_canonical_string()` (sorted keys, minified) |
| Write a lot of JSON without building a tree | `json::writer` |

## Quick example

```cpp
#include "conjure/core/json/json.h"

using namespace conjure;
using namespace conjure::core;

struct window_settings {
    u32 width = 1280;
    u32 height = 720;
    bool fullscreen = false;
};

result<window_settings> load_window_settings(std::string_view text) {
    CONJURE_TRY_ASSIGN(const json::document doc, json::parse(text));
    const json::value window = doc.root()["window"];

    window_settings settings = {};
    settings.width = static_cast<u32>(window["width"].as_u64(settings.width));
    settings.height = static_cast<u32>(window["height"].as_u64(settings.height));
    settings.fullscreen = window["fullscreen"].as_bool(false);
    return settings;
}
```

A missing `"window"` member is not an error here: indexing a missing key yields an *invalid* value, and
every accessor on an invalid value returns its fallback.

## Documents own, values view

A `json::document` owns a tree and is the only way to create or change JSON. A `json::value` is a small
non-owning view into a document: `doc.root()` returns one, and indexing a value returns more of them.
Copying a document deep-copies the tree; moving it is a pointer swap.

> [!WARNING]
> A `value` is valid only until its document is **mutated, moved or destroyed**. Do not keep views across
> a `set()`, `push_back()`, `erase()` or `merge_patch()` call, and never return a `value` from a function
> that owns the document locally. Keep the document and index into it again instead.

> [!WARNING]
> A moved-from document is *empty*: `root()` is invalid, it serialises to `""` and every mutation asserts
> or returns `false`. Only assign to it or let it go out of scope.

## Reading values safely

Reading never asserts. Indexing a missing key, an out-of-range element or a scalar yields an invalid value;
`is_valid()` tells you whether the lookup succeeded, and `is_null()` is true for JSON `null` **and** for
invalid values.

| Accessor | Returns |
|---|---|
| `as_bool(fallback)` | the boolean, or the fallback when the value is not a bool |
| `as_i64(fallback)`, `as_u64(fallback)` | integers (integral reals such as `3.0` convert; `2.5` does not) |
| `as_f64(fallback)`, `as_f32(fallback)` | any number (the fallback when the magnitude exceeds `f32`) |
| `as_string(fallback)` | a `std::string_view` into the document, UTF-8, may contain embedded zeros |
| `size()` | element or member count; 0 for scalars |
| `type()`, `is_number()`, `is_integer()`, ... | the kind of the value |

```cpp
usize count_components(const json::document &scene) {
    usize total = 0;
    for (const json::value entity : scene.root()["entities"]) {
        for (const auto [key, component] : entity["components"].members()) {
            total += component.is_object() ? 1 : 0;
        }
    }
    return total;
}
```

Iterating a non-array with `begin()`/`end()` or a non-object with `members()` visits nothing.
Duplicate keys are kept: `operator[]` and `get()` return the first, `members()` visits all of them.

> [!TIP]
> Use the `as_*` accessors for optional settings with sensible defaults. When a value is **required** and
> the caller must be told what went wrong, use `value.to<T>()`: it returns a `result<T>` whose error code
> is `NOT_FOUND` (missing), `INVALID_ARGUMENT` (wrong type), `OUT_OF_RANGE` (number does not fit `T`) or
> `PARSE_FAILURE` (a string that is not a valid `uuid`), with a message such as
> `expected integer, found string`.

`to<T>()` is available for `bool`, the fixed-width integers (range-checked), `f32`, `f64`, `std::string`,
`uuid` and `std::vector<T>` of any of these. The reflection area adds `string_id` and the math types (see
📘 API: [Reflection](reflection.md)). To support your own type, specialise `json::json_converter<T>` with
`static result<T> from_json(const json::value &source)`.

```cpp
result<std::vector<i32>> read_ids(const json::document &doc) {
    return doc.root()["ids"].to<std::vector<i32>>();   // "element 1: expected integer, found string"
}
```

## Building and changing a document

The builders `json::object()` and `json::array()` take every scalar type the mutation API accepts, other
documents (copied, or moved when passed as an rvalue) and values (copied). A `const char *` that is null
becomes JSON `null`.

```cpp
json::document make_save() {
    json::document save = json::object({
      {"name", "player"},
      {"level", 3},
      {"position", json::array({1.0, 2.5, -4.0})},
      {"tags", json::array({"hero", "melee"})}
    });
    save.set("checkpoint", "bridge");       // appended as the last member
    save.set("level", 4);                   // replaced in place, member order kept
    save.set_pointer("/stats/health", 100); // creates the "stats" object on the way
    save.erase("tags");                     // removes every member with that key
    return save;
}
```

A default-constructed document has a `null` root; the first `set()` turns it into an object and the first
`push_back()` into an array. Calling `set()` on an array root (or `push_back()` on an object root) asserts
and does nothing. Integers of any width are accepted: non-negative values are stored unsigned, negative
ones signed.

> [!CAUTION]
> `NaN` and infinity cannot be represented in JSON. Storing one asserts (Debug and Development builds) and
> writes `null`, so the
> document stays serialisable; the value is lost. Check floats before storing them if they may be
> non-finite.

> [!NOTE]
> Documents are pool-allocated: erasing or replacing members does not release memory until the document is
> destroyed. A long-lived document that is edited constantly should be rebuilt now and then (copy it, or
> parse its text).

## JSON Pointer and merge patches

`at_pointer()` and `set_pointer()` follow RFC 6901 (`"/a/b/0"`, `""` is the root, `~0` and `~1` escape
`~` and `/`). `set_pointer()` creates missing intermediate objects; in an array the last token replaces an
existing index while `-` or an index equal to the current size appends. It returns `false` for a
malformed pointer, a scalar parent or an index past the end.

`merge_patch()` applies an RFC 7386 patch: object members merge recursively, a `null` member removes the
target member, anything else replaces the target. This is the right tool for "apply the user's overrides
on top of the defaults".

```cpp
json::document apply_user_settings(json::document defaults, const json::document &userFile) {
    defaults.merge_patch(userFile);   // {"volume":7,"theme":"dark"} removes "muted" when it is null
    return defaults;
}
```

## Writing text

| Call | Output |
|---|---|
| `doc.to_string()` | minified, members in document order |
| `doc.to_string(true)` | pretty, 4-space indentation |
| `doc.to_canonical_string()` | minified, members sorted by key (byte order), shortest round-trip reals |
| `json::to_string(value, canonical)` | the same for any node, not just the root |

> [!TIP]
> Use `to_canonical_string()` whenever two documents must compare as text: content hashes, asset
> fingerprints, golden files and test expectations. The output is byte-identical on Windows, Linux and
> macOS. `operator==` on documents and values is structural (member order is ignored) and matches the
> canonical text.

Serialisation returns an empty string and logs a warning only when a string holds invalid UTF-8.

## Streaming with `writer`

`json::writer` writes minified JSON straight into a `std::string` without building a tree. Use it for
large outputs produced in one pass (logs, exported tables, protocol messages) or in hot paths. When keys
are written in sorted order its output is byte-identical to `to_canonical_string()`.

```cpp
#include <span>

std::string write_player(std::string_view name, std::span<const i32> tags) {
    json::writer out = {};
    out.begin_object();
    out.key("name");
    out.value(name);
    out.key("tags");
    out.begin_array();
    for (const i32 tag : tags) {
        out.value(tag);
    }
    out.end_array();
    out.end_object();
    return out.finish();   // {"name":"player","tags":[1,2]}
}
```

Structural misuse (a key outside an object, a value where a key is expected, an unbalanced `end_*`, a
second root) asserts and is ignored so the buffer stays well formed. `write_raw_json()` inserts text that
is already JSON; Debug builds validate it. `finish()` returns the text and resets the writer for reuse.

> [!WARNING]
> Strings and keys are copied as-is and must be valid UTF-8. The writer only escapes what JSON requires;
> invalid bytes produce text that `parse()` rejects (Debug builds assert at the `value()` call).

## Files and strictness

`json::document::parse_file()` reads and parses a file (`IO_FAILURE` when it is not a readable regular
file, `PARSE_FAILURE` otherwise) and `write_file()` creates parent directories. To read JSON from the
virtual file system, use `file_system::read_text_file()` and `json::parse()` (see 📘 API:
[Files and the virtual file system](files.md)).

Parsing is strict RFC 8259: comments, trailing commas, `NaN`, single quotes, a byte order mark and
trailing content are all rejected with `PARSE_FAILURE` and a message of the form
`parse error at line L, column C: reason`. Containers may nest at most `json::MAX_NESTING_DEPTH` (1024)
levels.

## Threading

Distinct documents are independent. One document may be **read** from many threads at once
(`root()`, accessors, iteration, `to_string()`, `at_pointer()`), but mutating it while any other thread
reads or mutates it is a data race: guard it. A `writer` belongs to one thread.

## API summary

| Signature (short) | Purpose |
|---|---|
| `result<document> json::parse(string_view text)` | parse strict JSON text |
| `std::string json::to_string(const value &, bool canonical = false)` | serialise any node |
| `static result<document> document::parse(string_view)` / `parse_file(path)` | parse text or a file |
| `static document document::object()` / `array()` / `from_value(const value &)` | create documents |
| `document json::object({{key, item}, ...})` / `json::array({item, ...})` | build in one expression |
| `value document::root() const` | view of the root |
| `void document::set(string_view key, T item)` / `set_null(key)` / `bool erase(key)` | object mutation |
| `void document::push_back(T item)` / `bool erase(usize index)` / `void clear()` | array mutation |
| `value document::at_pointer(string_view) const` / `bool set_pointer(string_view, T)` | RFC 6901 access |
| `bool document::merge_patch(const value &)` / `(const document &)` | RFC 7386 merge patch |
| `std::string document::to_string(bool pretty = false) const` | document-order text |
| `std::string document::to_canonical_string() const` | sorted, platform-stable text |
| `result<void> document::write_file(path, bool pretty = false) const` | write to disk |
| `bool value::is_valid() const` / `is_null()` / `type()` / `is_integer()` | classify a view |
| `T value::as_bool/as_i64/as_u64/as_f64/as_f32/as_string(fallback) const` | read with a fallback |
| `value value::operator[] (usize index)` / `operator[] (string_view key)` / `contains(key)` / `get(key)` | navigate |
| `array_iterator value::begin()/end()` / `object_range value::members()` | iterate |
| `result<T> value::to<T>() const` | checked conversion through `json_converter<T>` |
| `void writer::begin_object()/end_object()/begin_array()/end_array()` | structure |
| `void writer::key(string_view)` / `value(T)` / `write_raw_json(string_view)` | content |
| `std::string writer::finish()` / `void reset()` / `bool is_complete() const` | output |

## See also

- 📘 API: [Errors and results](errors-and-results.md) for `result<T>`, `CONJURE_TRY` and the generic error codes
- 📘 API: [Reflection](reflection.md) to convert whole structs to and from JSON without writing field code
- 📘 API: [Files and the virtual file system](files.md) to load JSON through `project://` and friends
- 📘 API: [Data types](data-types.md) for `u32`, `i64`, `f32` and the other aliases used here
- 🛠 Handbook: [Core module](../../handbook/modules/core.md) for the yyjson wrapper, canonical form and nesting limits

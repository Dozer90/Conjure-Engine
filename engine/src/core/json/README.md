# core/json

JSON for the engine: configuration, project files, scene text formats, editor protocol messages
and the reflection layer's serialisation all go through this area. It wraps yyjson 0.10 (private
dependency, never visible in public headers) behind three types and a handful of free functions.
Design: `docs/design/10_core.md` section 2.8.

## Public headers (`conjure/core/json/`)

| Header | Contents |
|---|---|
| `value.h` | `json_type`, `value` (non-owning view, read API, iteration, `to<T>()`), `json_converter<T>` customisation point with built-in specialisations |
| `document.h` | `document` (owning, mutable, deep copy / cheap move, parse, pointer access, merge patch, serialisation), builders `json::object(...)` / `json::array(...)` |
| `writer.h` | `writer` streaming builder over a `std::string` |
| `json.h` | umbrella plus free `parse()` and `to_string(value, canonical)` |

## Main types

- **`value`** holds an opaque node pointer and the owning document pointer. It never asserts:
  a failed lookup (`v["missing"]`, `v[99]`, indexing a scalar) returns an *invalid* value whose
  accessors return their fallback, so `doc.root()["window"]["width"].as_i64(1280)` is safe.
  `is_null()` is true for JSON null **and** for invalid values; `is_valid()` tells them apart.
  Views are valid until the owning document is mutated, moved or destroyed.
- **`document`** owns a `yyjson_mut_doc`. A default document has a null root that becomes an
  object on the first `set()` or an array on the first `push_back()`. Mutation through a root of
  the wrong kind asserts and does nothing. `set()` replaces an existing member in place so member
  order is preserved. Non-negative integers are stored unsigned, negative ones signed; non-finite
  reals assert and are stored as null so a document is always serialisable; a null `const char *`
  stores JSON null everywhere (mutation API, builders, writer). The fixed-width overloads are
  backed by a constrained template so `long`, `size_t`, `i8`, `short` and friends widen to
  i64/u64 instead of being ambiguous on platforms where they are distinct types. A moved-from
  document is *empty*: `root()` is invalid, it serialises to `""`, mutations assert or return
  false and it equals nothing; assign to it or let it die. `set_pointer()` follows RFC 6902
  "add" for a missing target, so an array index equal to the size appends like `-`.
- **`writer`** writes minified JSON straight into a string. Its output is byte-identical to
  `document::to_canonical_string()` for the same data when keys are written in sorted order; to
  guarantee that, reals are formatted by yyjson itself (through a stack allocator, no heap) and
  escaping mirrors yyjson's default table. Structural misuse asserts and is ignored so the buffer
  stays well formed. Strings and keys are copied as-is and must be UTF-8; Debug builds validate
  them and assert (where `document::to_string()` would refuse to serialise the same bytes).
- **Builders** (`json::object`, `json::array`) create each scalar node directly in the target
  document; only `document` and `value` arguments are deep-copied, once.
- **`json_converter<T>`** is the extension point for `value::to<T>()`. This area specialises it for
  `bool`, the fixed-width integers (range-checked; whole reals such as `3.0` are accepted),
  `f32`/`f64`, `std::string`, `uuid` and `std::vector<T>`. The reflect area adds `string_id` and
  the math types. Error codes: `NOT_FOUND` (invalid value), `INVALID_ARGUMENT` (type mismatch),
  `OUT_OF_RANGE`, `PARSE_FAILURE` (bad uuid text).

## Canonical form

`to_canonical_string()` builds a sorted deep copy in a scratch yyjson document (members sorted by
UTF-8 byte order, stable for duplicate keys) and writes it minified. Reals use yyjson's shortest
round-trip formatting (`0.1`, `1.0`, `-0.0`, `1e21`, `1e-7`), integers are plain digits with no
exponent. The algorithm is yyjson's own (no libc `printf`), so the text is identical on Windows,
Linux and macOS and can be hashed.

Equality (`value::operator==`, `document::operator==`) is structural and mirrors the canonical text:
objects compare regardless of member order, integers compare mathematically, reals bit for bit, and
an integer never equals a real. Duplicate keys pair up in order (the n-th `"a"` on one side with the
n-th on the other). Objects with the same key sequence compare pairwise; otherwise each member is
matched to its occurrence on the other side, so nothing is sorted. Both the comparison and the
canonical copy walk with explicit stacks (one `std::vector` of frames), never recursively.

## Parsing

Strict RFC 8259: comments, trailing commas, `NaN`/`Infinity`, single quotes, a BOM and trailing
content are rejected with `PARSE_FAILURE` and a message of the form
`parse error at line L, column C: reason`. Duplicate keys are kept (`operator[]` returns the first,
`erase()` removes all). `parse_file()` skips a leading UTF-8 byte order mark (as
`file_system::read_text_file` does) before handing the text to `parse()`, which itself rejects one.

### Nesting limit

yyjson's reader and its immutable-to-mutable copy are iterative, but its deep copy (used by the
copy constructor, `from_value`, `set`/`push_back`/`set_pointer` of a value) and `merge_patch`
recurse once per level, so unbounded input could overflow the stack. `parse()` therefore walks the
parsed tree with an explicit stack and rejects anything nested deeper than `MAX_NESTING_DEPTH`
(1024; the design requires 1000) with `PARSE_FAILURE` "nesting deeper than N levels". The limit is
sized for a 512 KiB thread stack in Debug. Serialisation, the canonical form and equality are
iterative and handle any depth. The mutation API does not enforce the limit: a tree built deeper
through `set()`/`push_back()` still serialises and compares, but copying or merging it is not
protected.

## Memory and allocation

yyjson's mutable documents are pool allocators: values and strings are never freed individually,
only when the document is destroyed. Erasing or replacing members therefore does not shrink a
document; rebuild it (copy, or parse its text) if that matters. Serialisation buffers come from the
C allocator and are released with `std::free` immediately after being copied into a `std::string`.

## Threading

- Distinct documents are independent: parsing, building and serialising on any number of threads
  at once is safe.
- One document may be read (`root()`, `value` accessors, iteration, `to_string`,
  `to_canonical_string`, `at_pointer`) from many threads concurrently.
- Mutating a document while any other thread reads or mutates it is a data race. Guard it.
- `writer` instances are not shared between threads.

## Testing

`tests/core/json/` builds `conjure_core_json_tests` (one file per public header plus the umbrella):

```
python tools/build.py --build-dir build/core_json --target conjure_core_json_tests --test -R core_json
build/core_json/bin/conjure_core_json_tests
```

The suite covers every accessor and failure path, canonical stability for a table of reals and
large integers, writer/canonical byte identity, RFC 7386 merge patch examples, RFC 6901 pointer
access, parse error positions, nesting at and past `MAX_NESTING_DEPTH`, the moved-from state,
duplicate-key lookups and equality, file round trips and multi-threaded use.

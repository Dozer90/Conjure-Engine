# Strings and identifiers

📘 **Public API wiki** · 🛠 Handbook: [Core module](../../handbook/modules/core.md)

Names in Conjure are usually compared as hashes, not as text: a `string_id` (`conjure/core/string_id.h`) is the
64-bit FNV-1a hash of a name and still prints as the name. This page covers that type, the hash functions behind
it (`conjure/core/hash.h`), the ASCII string helpers in `namespace str` (`conjure/core/string_utils.h`), the
packed bit set `bit_vector` (`conjure/core/bit_vector.h`) and the stable radix sort for integer keys
(`conjure/core/radix_sort.h`).

## At a glance

| You want to... | Use |
|---|---|
| Name a component, event or asset and compare it cheaply | `string_id` |
| A compile-time id in a hot loop | `"ecs.position"_sid` (`conjure::core::literals`, an inline namespace: `using namespace conjure::core;` is enough; add `using namespace conjure::core::literals;` only when you do not want the whole namespace) |
| An id for a static or namespace-scope variable that tools can print | `string_id::intern("name")` |
| Get the text of an id back | `id.view()`, `id.c_str()`, `id.to_string()`, `std::format("{}", id)` |
| Hash a string yourself | `hash::fnv1a64(text)`, `hash::fnv1a32(text)`, `hash::combine(a, b)` |
| Trim, split, join, change case, parse a number | `str::trim`, `str::split`, `str::join`, `str::to_lower`, `str::parse_int` |
| Convert between UTF-8 and UTF-16 / wide strings | `str::to_utf16`, `str::from_utf16`, `str::to_wide`, `str::from_wide` |
| A growable set of bits (masks, dirty flags) | `bit_vector` |
| Sort integer keys with attached indices, fast and stable | `radix_sort_u64`, `radix_sort_u32` |

## Quick example

```cpp
#include "conjure/core/string_id.h"
#include "conjure/core/string_utils.h"

#include <string>
#include <string_view>

using namespace conjure;
using namespace conjure::core;

bool is_position_component(std::string_view nameFromFile) {
    const string_id id(nameFromFile);       // hashed and interned at runtime
    return id == "ecs.position"_sid;        // compile-time hash, no lock, no allocation
}

std::string normalise_key(std::string_view raw) {
    return str::to_lower(str::trim(raw));
}
```

## string_id

A `string_id` is exactly eight bytes: the FNV-1a 64 hash of its name. Comparison, ordering, hashing and storage
never touch the text. The text is kept in a process-wide table ("interning") so tools, the editor and log output
can show the name again through `view()`, `c_str()`, `to_string()` or `std::format`.

### Making ids

| Form | Hashed | Interned | Use for |
|---|---|---|---|
| `string_id id(runtimeText)` | at runtime | yes | names read from files, function arguments, `std::string` values |
| `constexpr string_id ID("name")` | at compile time | no | constants compared against, never printed |
| `"name"_sid` | at compile time, always | no | literals in hot code; never takes the table lock |
| `string_id::intern("name")` | at runtime | yes, always | `static` and namespace-scope ids whose name must be printable |
| `string_id::from_hash(hash)` | - | no | wrapping a hash read from a file or from C# |

The constructor is `explicit`, so a string never turns into an id by accident. The default id is `string_id::null()`
with hash 0; `is_null()` tests for it.

> [!WARNING]
> `static const string_id ID("x");` and namespace-scope ids are constant-initialised by the language, so the
> constructor never interns them: they compare fine but print as `#<16 hex digits>`. Initialise such ids with
> `string_id::intern("x")` when their name matters. Ids built from runtime text (a `std::string`, a file) always
> intern.

> [!TIP]
> Use `"name"_sid` wherever the name is a literal: it is `consteval`, so it costs nothing at runtime and can never
> contend for the table lock. Intern the name once elsewhere (for example where the component is registered) if
> tools need to resolve it.

> [!NOTE]
> `string_id("")` is not the null id; its hash is the FNV offset basis, exactly as the C# `NameHash` computes it.
> Interning a second, different spelling under a hash that already has one is a collision: it asserts, logs an
> `ERR` on `LOG_CORE` and keeps the first spelling.

### Using ids

`string_id` works as a key in `std::unordered_map` (via `std::hash`), in `flat_hash_map`, and in sorted containers
(`<=>` compares hashes). `std::format("{}", id)` prints the interned name or `#` followed by the 16-digit hex hash
when the name is unknown. Interning and lookup are thread-safe; names are never removed, so a `view()` stays valid
for the life of the process.

## Hashing

`conjure/core/hash.h` provides the engine's canonical content hashes in `namespace hash`. They are the basis of
`string_id`, error categories and asset identity, and the constants and byte order are part of the engine's ABI.

| Function | Returns |
|---|---|
| `hash::fnv1a64(std::string_view text, u64 seed = ...)` | 64-bit FNV-1a of the UTF-8 bytes; `constexpr` |
| `hash::fnv1a64(std::span<const std::byte> bytes, u64 seed = ...)` | the same over raw bytes |
| `hash::fnv1a32(text)` / `hash::fnv1a32(bytes)` | 32-bit variants |
| `hash::fnv1a64_step(hash, byte)` / `fnv1a32_step(hash, byte)` | continue a hash one byte at a time |
| `hash::combine(u64 seed, u64 value)` | order-dependent combination of two hashes |

Passing a previous result as `seed` continues the hash, so `fnv1a64(" world", fnv1a64("hello"))` equals
`fnv1a64("hello world")`.

```cpp
#include "conjure/core/hash.h"

using namespace conjure;
using namespace conjure::core;

constexpr u64 PLAYER_TAG = hash::fnv1a64("player");

u64 identity_of(u64 sceneHash, u64 entityHash) {
    return hash::combine(sceneHash, entityHash);
}
```

## String helpers (`str::`)

Everything in `namespace conjure::core::str` is ASCII-oriented on purpose: identifiers, paths and configuration
values are ASCII, and locale-aware behaviour would make builds non-deterministic. UTF-8 passes through untouched
except in the explicit conversion functions.

| Group | Functions |
|---|---|
| Classification (one `char`) | `is_space`, `is_digit`, `is_alpha`, `is_alnum`, `to_lower`, `to_upper` |
| Case (whole strings) | `to_lower(text)`, `to_upper(text)` return copies; `to_lower_in_place`, `to_upper_in_place` |
| Trimming | `trim`, `trim_left`, `trim_right` (default set `str::WHITESPACE`, or your own characters) |
| Comparison | `starts_with`, `ends_with`, `contains`; case-insensitive `iequals`, `istarts_with`, `iends_with` |
| Splitting and joining | `split(text, ',')`, `split(text, "::")` with `keepEmpty`, `split_lines`, `join(range, separator)`, `replace_all` |
| Parsing | `parse_int`, `parse_uint`, `parse_integer<T>(text, base)`, `parse_double`, `parse_float`, `parse_bool` (all return `std::optional`) |
| Formatting | `format_bytes(1536)` gives `"1.5 KiB"` |
| Unicode | `to_utf16`, `from_utf16`, `to_utf32`, `from_utf32`, `to_wide`, `from_wide`, `is_valid_utf8` |
| Identifiers | `is_valid_identifier` (C-style: letter or `_`, then letters, digits, `_`) |

The parsers accept the whole string or nothing: no surrounding whitespace, no trailing characters, and out-of-range
values fail. `parse_bool` understands `true/false`, `yes/no`, `on/off` and `1/0` in any case. The Unicode
conversions never fail; malformed input becomes U+FFFD.

```cpp
#include "conjure/core/string_utils.h"

#include <optional>
#include <string_view>
#include <vector>

using namespace conjure;
using namespace conjure::core;

std::optional<i64> read_setting(std::string_view line) {
    const std::vector<std::string_view> parts = str::split(line, '=');
    if (parts.size() != 2 || !str::iequals(str::trim(parts[0]), "max_players")) {
        return std::nullopt;
    }
    return str::parse_int(str::trim(parts[1]));
}
```

> [!WARNING]
> `trim`, `split` and `split_lines` return views into the text you passed in. The input must outlive the result;
> do not call them on a temporary `std::string` and keep the pieces.

## Bit sets: `bit_vector`

`bit_vector` is a dynamically sized set of bits packed into 64-bit words, used for component masks, free-slot
tracking and dirty flags. Bits past `size()` are always zero, so counting, `any()`, equality and the bitwise
operators work on whole words.

```cpp
#include "conjure/core/bit_vector.h"

#include <vector>

using namespace conjure;
using namespace conjure::core;

std::vector<usize> entities_to_draw(const bit_vector &alive, const bit_vector &visible) {
    const bit_vector drawable = alive & visible;        // same size required
    std::vector<usize> indices = {};
    indices.reserve(drawable.count());
    for (const usize index : drawable.set_bits()) {     // ascending indices of the set bits
        indices.push_back(index);
    }
    return indices;
}
```

| Group | Members |
|---|---|
| Size | `size()`, `empty()`, `capacity()`, `reserve(bits)`, `resize(size, value = false)`, `push_back(value)`, `clear()` |
| Single bits | `test(i)` / `operator[]`, `set(i)`, `set(i, value)`, `reset(i)`, `flip(i)` |
| All bits | `set_all()`, `reset_all()`, `flip_all()`, `any()`, `none()`, `all()`, `count()` |
| Searching | `find_first_set(from)`, `find_first_clear(from)`, `find_last_set()` (return `bit_vector::NPOS` when none), `set_bits()` range |
| Set operations | `&=`, `\|=`, `^=`, `remove(other)` (this and not other), `is_subset_of(other)`, `intersects(other)`, free `&`, `\|`, `^`, `~` |
| Raw | `words()` as `std::span<const u64>`, `word_count()` |

> [!WARNING]
> Out-of-range indices assert; `test()` then returns false and the setters do nothing. Binary operations require
> both operands to have the same `size()` (asserted). Do not modify the vector while iterating `set_bits()`.
> `bit_vector` is a plain value type and is not thread-safe.

## Sorting integer keys: radix sort

`radix_sort_u64` and `radix_sort_u32` sort unsigned keys ascending, optionally carrying a `u32` payload (an index
into your own arrays) alongside each key. The sort is stable (equal keys keep their order), allocation-free (you
provide scratch buffers) and skips passes that would not change anything, so already sorted or small-range input is
cheap. Use it for draw-call sorting, depth sorting and anything else with tens of thousands of integer keys.

```cpp
#include "conjure/core/radix_sort.h"

#include <vector>

using namespace conjure;
using namespace conjure::core;

void sort_draws(std::vector<u64> &sortKeys, std::vector<u32> &drawIndices) {
    std::vector<u64> scratchKeys(sortKeys.size());
    std::vector<u32> scratchIndices(drawIndices.size());
    radix_sort_u64(sortKeys, drawIndices, scratchKeys, scratchIndices);
    // sortKeys is now ascending and drawIndices[i] still belongs to sortKeys[i]
}
```

> [!WARNING]
> `values` must have the same size as `keys`, both scratch spans must be at least that large, and neither scratch
> span may overlap the inputs (the sort ping-pongs between them). All of this is asserted and the input is left
> untouched on a violation. The scratch contents are unspecified afterwards.

## API summary

| Signature (short) | Purpose |
|---|---|
| `constexpr explicit string_id(std::string_view name)` | hash (and intern when evaluated at runtime) |
| `static string_id string_id::intern(std::string_view)` | hash and always intern |
| `static constexpr string_id string_id::from_hash(u64)` / `string_id::null()` | wrap a hash / the null id |
| `u64 hash() const` / `bool is_null() const` | the hash |
| `std::string_view view() const` / `const char *c_str() const` / `std::string to_string() const` | the name, when interned |
| `static usize string_id::interned_count()` | names in the table |
| `consteval string_id operator""_sid(const char *, usize)` | literal ids (`inline namespace literals`, visible through `conjure::core`) |
| `u64 hash::fnv1a64(text \| bytes, seed)` / `u32 hash::fnv1a32(text \| bytes, seed)` | FNV-1a |
| `u64 hash::combine(u64 seed, u64 value)` | combine hashes |
| `str::trim / trim_left / trim_right(text, characters = WHITESPACE)` | views without padding |
| `str::split(text, delimiter, keepEmpty = true)` / `str::split_lines(text)` / `str::join(range, separator)` | pieces |
| `str::replace_all(text, from, to)` | substitution |
| `str::parse_int / parse_uint / parse_double / parse_float / parse_bool(text)` | `std::optional` parsers |
| `str::to_lower / to_upper(text)` / `str::iequals(a, b)` / `str::starts_with / ends_with / contains` | case and search |
| `str::to_utf16 / from_utf16 / to_utf32 / from_utf32 / to_wide / from_wide` / `str::is_valid_utf8` | Unicode |
| `str::format_bytes(u64)` / `str::is_valid_identifier(text)` | formatting, identifiers |
| `bit_vector(usize size, bool value = false)` | sized bit set |
| `bit_vector::set / reset / flip / test(usize)` / `count()` / `any()` / `find_first_set(usize from = 0)` / `set_bits()` | bit access |
| `void radix_sort_u64(keys, values, scratchKeys, scratchValues)` / `radix_sort_u64(keys, scratchKeys)` | stable 64-bit key sort |
| `void radix_sort_u32(...)` | 32-bit variants |

## See also

- 📘 API: [Containers](containers.md): `flat_hash_map<string_id, T>` is the usual home for things named by id
- 📘 API: [Data types](data-types.md): `uuid`, the identifier for things that must survive across machines
- 📘 API: [Glossary](../glossary.md)

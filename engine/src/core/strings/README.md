# core/strings

Hashed identifiers, string helpers, a packed bit set and a stable radix sort. Static library
`conjure::core_strings`, depends only on `conjure::core_foundation`. Design: `docs/design/10_core.md`
section 2.4.

Headers (`conjure/core/`):

- `string_id.h`: `string_id` (64-bit FNV-1a of a name, interned spelling), `"name"_sid` literal,
  `std::hash` / `std::formatter`.
- `string_utils.h`: `namespace str`: ASCII case/classification, `trim*`, `starts_with` / `ends_with`
  (+ `i` variants), `iequals`, `contains`, `split` / `split_lines` / `join` / `replace_all`,
  `parse_integer<T>` / `parse_int` / `parse_uint` / `parse_double` / `parse_float` / `parse_bool`,
  `format_bytes`, UTF-8 <-> UTF-16 / UTF-32 / wide, `is_valid_utf8`, `is_valid_identifier`.
- `bit_vector.h`: `bit_vector`: dynamic bit set over 64-bit words with search, counting, bitwise ops
  and set-bit iteration.
- `radix_sort.h`: `radix_sort_u64` / `radix_sort_u32`: stable LSD sort with optional `u32` payloads
  and caller-provided scratch.

## string_id

- `constexpr string_id(std::string_view)` hashes the name. When the construction is
  constant-evaluated nothing else happens; otherwise the name is also interned.
  `string_id::intern(name)` interns unconditionally and `from_hash(u64)` wraps a hash
  (deserialisation) without interning.
- Whether a construction is constant-evaluated is decided by the language, not by the spelling:
  - `static const string_id ID("x");`, `static const auto ID = "x"_sid;` and namespace-scope ids are
    constant-initialised and therefore *never* interned. Initialise statics with
    `string_id::intern("x")` when tools must be able to show the name.
  - A `const bool`/`const int` local initialised from an id comparison may be constant-evaluated on
    GCC/Clang but not on MSVC; never rely on interning from such an initialiser.
  - Constructing from a runtime value (`std::string`, data read from a file, a function argument)
    always interns.
  - `"x"_sid` is `consteval`: always a compile-time hash, never interned, never takes the mutex, so
    it is safe in hot loops.
- The constructor is `explicit` (a deliberate deviation from the design doc's implicit signature) so
  a plain string never converts to an id by accident.
- The null id is hash 0. Note that `string_id("")` is *not* null: its hash is the FNV offset basis,
  exactly as the C# `NameHash` computes it.
- `view()` / `c_str()` return the interned spelling or `""`; `to_string()` and the formatter print the
  name when known, else `#` + 16 hex digits.
- Collision policy: interning a second spelling under an existing hash asserts, logs `ERROR` on
  `LOG_CORE` and keeps the first spelling. The table lock is released before the log and assert
  calls, so log sinks and assert handlers may freely construct, resolve or format ids.
  `detail::register_string_id_name` / `find_string_id_name` expose the table so tests and tooling
  can drive it directly.

## Threading

- The `string_id` name table is a process-wide, mutex-guarded `std::unordered_map`. Interning and
  lookup are safe from any thread; names are never erased, so views returned by `view()`/`c_str()`
  stay valid for the life of the process. The table is never destroyed (it lives in static raw
  storage; one intentional leak per process), so ids may still be resolved and formatted from
  static destructors during shutdown. Comparison and hashing never touch the table.
- Everything in `str` is pure and reentrant.
- `bit_vector` is a plain value type: not thread-safe; callers synchronise.
- `radix_sort_*` only touch the spans they are given and allocate nothing, so concurrent sorts of
  disjoint data are fine.

## Notes for callers

- `str::split` returns `std::vector<std::string_view>` into the input; the input must outlive the
  result.
- `str::parse_double` uses `std::from_chars` where the standard library provides it and falls back to
  `strtod` (C locale assumed) otherwise. Both paths reject hexadecimal floats, overflow (`1e400`) and
  underflow to zero (`1e-400`), and both accept denormals (`1e-320`), so behaviour is identical
  everywhere. Both paths also reject the `infinity` and `nan(...)` spellings that `from_chars` and
  `strtod` accept, so only `inf` and `nan` (any case, optional sign) parse as non-finite values.
  `str::parse_float` rejects finite values outside the `f32` range before narrowing.
- `str::to_wide` / `from_wide` produce UTF-16 where `wchar_t` is 16 bits (Windows) and UTF-32 where
  it is 32 bits, with no platform `#ifdef`; `to_utf16` / `from_utf16` and `to_utf32` / `from_utf32`
  expose both encodings on every platform.
- `bit_vector` keeps every bit past `size()` zero, so `count()`, `any()`, `==` and the bitwise
  operators work on whole words. Mixing vectors of different sizes asserts.
- `bit_vector`'s move constructor and move assignment are written out in `bit_vector.cpp`: the
  moved-from vector is empty (size 0, no words) and reusable, and a self-move assignment is a
  no-op. Defaulted moves would leave the old size over an emptied word array.
- `bit_vector::set_bit_iterator` models `std::forward_iterator` but reports the legacy
  `input_iterator_tag` because it dereferences to a value, like `std::ranges::iota_view`'s iterator.
  Incrementing the end iterator asserts and leaves it at the end.
- The radix sort skips any 8-bit pass in which every key shares the same digit, so already sorted,
  all-equal and small-range inputs cost one histogram sweep plus the passes that matter. No two of
  the spans (keys, values and both scratch buffers) may overlap (every pair is asserted; the input
  is left untouched).

## Testing

`tests/core/strings/` -> `conjure_core_strings_tests` (one file per header). Build and run with

```
python tools/build.py --build-dir build/core_strings --target conjure_core_strings_tests --test -R core_strings
```

The suite covers constexpr construction, lazy interning, static/namespace-scope ids staying
un-interned, collision asserts through the public constructor and `intern` (`assert_capture` +
`log_capture`), re-entrant handlers and sinks during a collision report, interning from 8 threads,
UTF-8/UTF-16/UTF-32 round trips and malformed input, float parsing at the overflow/underflow edges,
the `std::forward_iterator` concept for set-bit iteration, the moved-from `bit_vector`, radix
sorting against `std::stable_sort` on random, sorted, equal, empty, single and 100k-element inputs,
and aliasing scratch detection.

# core/foundation

The part of core every other core area (and every module) builds on: platform detection, fundamental
types, hashing, the error model, assertions, logging and uuids. Static library
`conjure::core_foundation`; depends on nothing but the C++ standard library (plus `Threads::Threads`).
Design: `docs/design/10_core.md` 2.1 (the headers themselves are the detailed reference).

| Header (`conjure/core/`) | Contents |
|---|---|
| `platform_detect.h` | `CONJURE_PLATFORM_*`, `CONJURE_COMPILER_*`, `CONJURE_ARCH_*`, `CONJURE_STDLIB_*`, the configuration macros `CONJURE_DEBUG` / `CONJURE_DEVELOPMENT` / `CONJURE_DISTRIBUTION` (exactly one is 1), `CONJURE_ASSERTS_ENABLED`, `CONJURE_DEV_TOOLS_ENABLED`, compiler helpers (`CONJURE_FORCE_INLINE`, `CONJURE_DEBUG_BREAK`, `CONJURE_UNREACHABLE_HINT`, `CONJURE_UNIQUE_NAME`, ...). The only header allowed to read compiler-defined macros such as `_WIN32`. |
| `types.h` | `u8`..`u64`, `i8`..`i64`, `f32`, `f64`, `usize`, `isize`, `byte`, `KIB` / `MIB` / `GIB`, `CACHE_LINE_SIZE`, all directly in `namespace conjure`. 64-bit targets only (static_assert). |
| `api.h` | `CONJURE_API` export/import macro for the shared umbrella library. |
| `build_info.h` | Generated from `build_info.h.in` by `engine/src/core/CMakeLists.txt`: `CONJURE_VERSION_MAJOR/MINOR/PATCH/STRING`, `CONJURE_ENGINE_NAME` and the `build_info::VERSION_*` / `VERSION_NUMBER` constants. |
| `hash.h` | `hash::fnv1a64` / `fnv1a32` over `std::string_view` and `std::span<const std::byte>`, the `*_step` helpers and `hash::combine`. `constexpr`; the constants are ABI (`string_id`, error categories and the C# `NameHash` all depend on them). |
| `error.h` | `error_code` (generic codes: `NOT_FOUND`, `INVALID_ARGUMENT`, `IO_FAILURE`, `PARSE_FAILURE`, ...), `class error` (`category()`, `code()`, `message()`, `is(code)`, `to_string()`), `make_error(code)` / `make_error(code, detail)` / `make_error(code, fmt, args...)`, `CONJURE_DECLARE_ERROR_CATEGORY(enum, "name", name_fn)` binding a module enum to a category, `std::formatter` for both. |
| `result.h` | `result<T>` (value or `error`; `value()`, `get_error()`, `value_or()`, `operator->`), `result<void>` and the alias `status`, `CONJURE_TRY(expression)` and `CONJURE_TRY_ASSIGN(declaration, expression)`. Accessing the wrong alternative asserts and aborts. |
| `assert.h` | `CONJURE_ASSERT(condition, fmt, ...)` and `CONJURE_VERIFY` (both check in Debug and Development; in Distribution `CONJURE_ASSERT` compiles out while `CONJURE_VERIFY` still evaluates its expression), `CONJURE_UNREACHABLE()` (asserts then aborts in Debug and Development, even when the handler continues; a compiler hint in Distribution, where reaching it is undefined behaviour), `CONJURE_FATAL_ASSERT`, `set_assert_handler` / `current_assert_handler` returning `assert_action` (`BREAK`, `CONTINUE`, `ABORT`), `assert_info`. |
| `log.h` | `log_level` (`TRACE` .. `FATAL`, `OFF`; `ERR` rather than the Windows macro `ERROR`), `log_category` with `CONJURE_DECLARE_LOG_CATEGORY(name)` / `CONJURE_DEFINE_LOG_CATEGORY(name, "display")`, `CONJURE_LOG_TRACE/DEBUG/INFO/WARN/ERROR/FATAL(category, fmt, ...)` (TRACE and DEBUG compile out of Distribution), `log_sink` with `console_log_sink`, `file_log_sink`, `callback_log_sink`, `log::add_sink` / `remove_sink` / `clear_sinks` taking `std::shared_ptr<log_sink>`, global and per-category levels, `log::categories()` / `find_category` / `set_category_level`, `format_log_line`, `LOG_CORE`. |
| `uuid.h` | `uuid` (two `u64` halves): `generate()` (RFC 4122 v4), `parse`, `to_string`, `is_null`, `hash`, `<=>`, `std::hash` and `std::formatter`. |
| `enum_flags.h` | `CONJURE_ENUM_FLAGS(type);` operators for `enum class <x>_flags`, plus `has_flag`, `has_any_flag`, `with_flag`, `without_flag`, `to_underlying`. |
| `non_copyable.h` | `non_copyable` / `non_movable` base classes. |

## Behaviour worth knowing

- `make_error(code, "fixed text")` with one string argument resolves to the `std::string_view` overload (the
  `std::format_string` overload needs at least one format argument to be more specialised), so braces in a
  fixed message are kept verbatim; `test_error.cpp` pins this for literals, `const char *`, `std::string` and
  `std::string_view`.
- Error categories are the FNV-1a 32 of the category name; `error::generic_code()` is `error_code::NONE` for
  module errors, so compare with `is(<enum>::VALUE)` rather than through the generic code.
- Log messages are formatted only when the category and the global level allow them; `log::write` delivers to
  the sinks outside the logger lock, so a sink may itself log or add/remove sinks.
- Private sources: `format_log_line` gets the local time of day from `detail::local_time_of` (`local_time.h`),
  implemented once per OS in `local_time_windows.cpp` (`localtime_s`) and `local_time_posix.cpp` (`localtime_r`);
  `CMakeLists.txt` picks the file, so `log.cpp` holds no platform conditionals. `console_output.h` declares
  `detail::console_stream_for` (stdout below `WARN`, stderr from `WARN` up) and `detail::write_console_line`
  (optional ANSI colour, flush from `ERR` up), which `console_log_sink::write` is built on.
- `uuid::generate()` seeds a per-thread `std::mt19937_64` from `std::random_device` on first use.

## Threading model

- Logging is thread-safe; sinks must synchronise themselves: `file_log_sink` locks internally, `console_log_sink`
  relies on the C runtime's per-call stream locking, and a `callback_log_sink` callable is invoked concurrently from
  every logging thread and must be thread-safe itself.
- `log_category::set_level` / `log::set_global_level` are relaxed atomics readable from every thread.
- The assert handler is an atomic function pointer; `set_assert_handler` is safe from any thread.
- `uuid::generate()` is thread-safe (per-thread generator). `error`, `result` and `uuid` values are plain values
  with no shared state.

## Testing

`tests/core/foundation/` builds `conjure_core_foundation_tests`, one file per public header that has
behaviour (`test_assert.cpp`, `test_enum_flags.cpp`, `test_error.cpp`, `test_hash.cpp`, `test_log.cpp`,
`test_result.cpp`, `test_uuid.cpp`; `platform_detect.h`, `types.h` and `api.h` are macro/alias-only headers
and `non_copyable.h` holds two trivial base classes, all four listed in `tools/style_exceptions.json`;
`build_info.h` is generated from `build_info.h.in` into the build tree, so the test-mirror check does not apply
to it). `test_hash.cpp` carries the FNV-1a test vectors mirrored by `Conjure.Managed`. `test_log.cpp` checks the
console sink's stream routing, colour codes and ERR flush through `console_output.h` against a temporary file,
since redirecting the process's own console would need platform code in the test.

```
python tools/build.py --build-dir build/core_foundation --target conjure_core_foundation_tests --test -R core_foundation
build/core_foundation/bin/conjure_core_foundation_tests
```

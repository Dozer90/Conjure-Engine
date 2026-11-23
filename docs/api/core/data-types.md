# Data types
<!-- doc-template:wiki -->
[API](../index.md)

<!-- doc-content:purpose:start -->
The small set of types and macros every other Conjure header builds on: fixed-width numbers, flag enumerations,
128-bit identifiers and the platform macros, plus the way a type forbids copying. Include `conjure/core/types.h` for
the numbers; the other headers are named in the section that covers them.
<!-- doc-content:purpose:end -->

|---|---|
| Name | Data types |
| Module | Core |
| `#include` | `conjure/core/api.h`, `conjure/core/enum_flags.h`, `conjure/core/platform_detect.h`, `conjure/core/types.h`, `conjure/core/uuid.h` |
| Engineering Handbook | [Engineering handbook](../../handbook/engine/core.md) |

## Syntax

<!-- doc-content:syntax:start -->
<a id="quick-example"></a>

```cpp
#include "conjure/core/enum_flags.h"
#include "conjure/core/types.h"
#include "conjure/core/uuid.h"

using namespace conjure;
using namespace conjure::core;

enum class window_flags : u32 {
    NONE = 0,
    RESIZABLE = 1 << 0,
    DECORATED = 1 << 1
};
CONJURE_ENUM_FLAGS(window_flags);

struct window_desc {
    uuid id = uuid::generate();
    u32 width = 1280;
    u32 height = 720;
    usize frameBudget = 4 * MIB;
    window_flags flags = window_flags::RESIZABLE | window_flags::DECORATED;
};

bool wants_border(const window_desc &desc) {
    return has_flag(desc.flags, window_flags::DECORATED);
}
```
<!-- doc-content:syntax:end -->

## At a glance

<!-- doc-content:at_a_glance:start -->
| You want to... | Use |
|---|---|
| A 32-bit unsigned integer, a byte count, a 32-bit float | `u32`, `usize`, `f32` from `conjure/core/types.h` |
| Write "64 KiB" without counting zeros | `64 * KIB` (`KIB`, `MIB`, `GIB`) |
| Combine options with `\|` and test them | `enum class my_flags : u32 { NONE = 0, ... };` + `CONJURE_ENUM_FLAGS(my_flags);` then `has_flag()` |
| Convert an enumerator to its integer | `to_underlying(value)` |
| Stop a class from being copied | delete its copy operations yourself: `my_type(const my_type &) = delete;` (there is no base class for it) |
| A stable identifier for an asset or a prefab | `uuid::generate()`, `uuid::parse()`, `uuid::to_string()` |
| Know whether asserts are compiled in | `#if CONJURE_ASSERTS_ENABLED` |
| Silence an unused variable | `CONJURE_UNUSED(value)` |
<!-- doc-content:at_a_glance:end -->

## Variables and Properties

<!-- doc-content:variables:start -->
Types, fields, and state are described in the reference sections below.
<!-- doc-content:variables:end -->

<!-- doc-content:body:start -->
## Numbers

`conjure/core/types.h` defines the aliases directly in `namespace conjure`, so they are usable unqualified from
every module namespace (`conjure::ecs`, `conjure::renderer`,...). Conjure only supports 64-bit targets; the header
checks this at compile time.

| Alias | Standard type | Notes |
|---|---|---|
| `u8`, `u16`, `u32`, `u64` | `std::uint8_t`.. `std::uint64_t` | unsigned, exact width |
| `i8`, `i16`, `i32`, `i64` | `std::int8_t`.. `std::int64_t` | signed, exact width |
| `f32`, `f64` | `float`, `double` | 32 and 64 bits, checked |
| `usize`, `isize` | `std::size_t`, `std::ptrdiff_t` | sizes, counts, indices, pointer differences |
| `byte` | `std::byte` | raw memory; arithmetic is not allowed on it |

Constants: `KIB` (1024), `MIB`, `GIB` as `usize`, and `CACHE_LINE_SIZE` (64), used by the allocators to keep data of
different threads on different cache lines.

> [!TIP]
> Use `usize` for anything that counts or indexes memory and `u32` for anything that is stored or serialised, so
> file formats do not change between platforms.

## Flag enumerations

`conjure/core/enum_flags.h` turns a scoped enumeration into a flag set. Declare the enumeration with an explicit
unsigned underlying type and a `NONE = 0` member, then expand `CONJURE_ENUM_FLAGS(type);` once (the macro call takes
a semicolon) in the same namespace as the enumeration. That defines `|`, `&`, `^`, `~`, `|=`, `&=` and `^=` for it.

The free functions work on any enumeration, with or without the macro:

| Function | Meaning |
|---|---|
| `has_flag(value, flag)` | every bit of `flag` is set in `value` |
| `has_any_flag(value, flags)` | at least one bit of `flags` is set in `value` |
| `with_flag(value, flag)` | copy of `value` with `flag` set |
| `without_flag(value, flag)` | copy of `value` with `flag` cleared |
| `to_underlying(value)` | the enumerator as its underlying integer |

Everything is `constexpr`, so flags can be combined in constant expressions and `static_assert`s.

> [!TIP]
> Prefer `has_flag(flags, my_flags::X)` over `(flags & my_flags::X) == my_flags::X`: it reads as a question and
> works for multi-bit masks (`has_flag(flags, my_flags::A | my_flags::B)` asks for both).

> [!WARNING]
> `has_flag(value, my_flags::NONE)` is always true (every bit of nothing is set) and `has_any_flag(value,
> my_flags::NONE)` is always false. Test for "no flags at all" with `value == my_flags::NONE`.

> [!IMPORTANT]
> Expand `CONJURE_ENUM_FLAGS` in the namespace that declares the enumeration. The operators are found through
> argument-dependent lookup, so an expansion in a different namespace is not seen from code that uses the flags
> elsewhere.

## Non-copyable and non-movable types

Conjure has no base class for this. A type that owns something with an identity says so itself, in standard C++,
by deleting or defaulting its copy and move operations. Every engine type does the same, so what a class allows
is readable in the class.

- Copies deleted, moves defaulted: a resource that can change hands (a file handle, a GPU buffer).
- Copies and moves deleted: an object whose address is handed out (a system other code points at).

```cpp
#include <cstddef>
#include <vector>

class texture_upload {
    private:
        std::vector<std::byte> mPixels;

    public:
        texture_upload() = default;
        ~texture_upload() = default;

        texture_upload(const texture_upload &) = delete;
        texture_upload &operator=(const texture_upload &) = delete;

        texture_upload(texture_upload &&) noexcept = default;
        texture_upload &operator=(texture_upload &&) noexcept = default;
};

class render_device {
    public:
        render_device() = default;
        ~render_device() = default;

        render_device(const render_device &) = delete;
        render_device &operator=(const render_device &) = delete;

        render_device(render_device &&) = delete;
        render_device &operator=(render_device &&) = delete;
};
```

`noexcept` on the defaulted moves tells the standard containers that moving cannot fail, so `std::vector` keeps its
strong exception guarantee when it grows (and, for a type that is also copyable, moves instead of copying). Deleting
the copies alone already suppresses the implicit moves, so a type that should move must default them as shown.

## UUIDs

`conjure/core/uuid.h` provides `uuid`, a 128-bit identifier (two `u64` halves). The engine uses it for assets,
prefabs and anything that must stay stable across sessions and machines.

| You want to... | Use |
|---|---|
| A fresh random identifier (RFC 4122 version 4) | `uuid::generate()` (thread-safe) |
| "No identifier" | `uuid::null()`, test with `is_null()` |
| Read one from text | `uuid::parse("123e4567-e89b-12d3-a456-426614174000")` returns `std::optional<uuid>`; braces and upper case are accepted |
| Write one as text | `to_string()` gives the lower-case canonical form; `std::format("{}", id)` works too |
| Use it as a map key | `std::hash<uuid>` is provided; `hash()` returns the same 64-bit value |
| Order them | `<=>` and `==` are defaulted (high half first) |

```cpp
#include "conjure/core/uuid.h"

#include <optional>
#include <string_view>

using namespace conjure::core;

uuid identifier_for(std::string_view savedText) {
    const std::optional<uuid> parsed = uuid::parse(savedText);
    if (!parsed.has_value()) {
        return uuid::generate();
    }
    return *parsed;
}
```

> [!WARNING]
> A default-constructed `uuid` is the null identifier, not a fresh one. Call `uuid::generate()` when you need a
> new identity, and check `is_null()` before treating an identifier read from data as valid.

## Platform and build macros

`conjure/core/platform_detect.h` is the only header that inspects compiler-defined macros. Every platform, compiler,
architecture and configuration switch is always defined, to `0` or `1`, so test them with `#if`, never `#ifdef`;
`CONJURE_PLATFORM_NAME` and `CONJURE_CONFIGURATION_NAME` are string literals.

| Group | Macros |
|---|---|
| Platform | `CONJURE_PLATFORM_WINDOWS`, `CONJURE_PLATFORM_MACOS`, `CONJURE_PLATFORM_LINUX`, `CONJURE_PLATFORM_POSIX`, `CONJURE_PLATFORM_NAME` |
| Compiler | `CONJURE_COMPILER_MSVC`, `CONJURE_COMPILER_CLANG`, `CONJURE_COMPILER_APPLECLANG`, `CONJURE_COMPILER_GCC` |
| Architecture | `CONJURE_ARCH_X64`, `CONJURE_ARCH_ARM64` |
| Build configuration | exactly one of `CONJURE_DEBUG`, `CONJURE_DEVELOPMENT`, `CONJURE_DISTRIBUTION` is 1; `CONJURE_CONFIGURATION_NAME` |
| Feature switches | `CONJURE_ASSERTS_ENABLED`, `CONJURE_DEV_TOOLS_ENABLED` (both off only in Distribution) |
| Helpers | `CONJURE_FORCE_INLINE`, `CONJURE_NO_INLINE`, `CONJURE_DEBUG_BREAK()`, `CONJURE_FUNCTION_NAME`, `CONJURE_RESTRICT`, `CONJURE_UNUSED(x)`, `CONJURE_CONCAT(a, b)`, `CONJURE_STRINGIFY(x)`, `CONJURE_UNIQUE_NAME(base)` |

`conjure/core/api.h` adds `CONJURE_API` (symbol export/import when the engine is built as a shared library; empty
for static builds) and `CONJURE_PLUGIN_EXPORT` for entry points a plugin exports with C linkage.

> [!NOTE]
> Game code rarely needs these. Platform differences are hidden behind the engine's backends (windows, files,
> audio), and the build configuration decides for you whether asserts and `CONJURE_LOG_TRACE`/`DEBUG` exist. Reach
> for `CONJURE_ASSERTS_ENABLED` only to compile out expensive validation code of your own.
<!-- doc-content:body:end -->

## Functions

<!-- doc-content:functions:start -->
<a id="api-summary"></a>

| Signature (short) | Purpose |
|---|---|
| `u8 u16 u32 u64 i8 i16 i32 i64 f32 f64 usize isize byte` | fixed-width aliases in `namespace conjure` |
| `KIB MIB GIB CACHE_LINE_SIZE` | size constants (`usize`) |
| `CONJURE_ENUM_FLAGS(enum_type);` | bitwise operators for a flag enumeration |
| `bool has_flag(E value, E flag)` | every bit of `flag` set |
| `bool has_any_flag(E value, E flags)` | at least one bit set |
| `E with_flag(E value, E flag)` / `E without_flag(E value, E flag)` | modified copies |
| `underlying to_underlying(E value)` | enumerator to integer |
| `uuid()` / `uuid(u64 high, u64 low)` | null identifier / from halves |
| `static uuid uuid::generate()` | random version-4 identifier, thread-safe |
| `static std::optional<uuid> uuid::parse(std::string_view)` | parse canonical text |
| `std::string uuid::to_string() const` | canonical lower-case text |
| `bool uuid::is_null() const` / `u64 high() const` / `u64 low() const` / `u64 hash() const` | accessors |
| `CONJURE_PLATFORM_WINDOWS/MACOS/LINUX/POSIX CONJURE_COMPILER_* CONJURE_ARCH_*` | always 0 or 1 |
| `CONJURE_PLATFORM_NAME` / `CONJURE_CONFIGURATION_NAME` | string literals (`"Windows"`, `"Debug"`,...) |
| `CONJURE_DEBUG CONJURE_DEVELOPMENT CONJURE_DISTRIBUTION` | exactly one is 1 |
| `CONJURE_ASSERTS_ENABLED CONJURE_DEV_TOOLS_ENABLED` | 0 only in Distribution |
| `CONJURE_API` / `CONJURE_PLUGIN_EXPORT` | symbol visibility (`conjure/core/api.h`) |
<!-- doc-content:functions:end -->

------------------------------------------

**See also:**

<!-- doc-content:related:start -->
- 📘 API: [Errors and results](errors-and-results.md), which uses `u16` enumerations as error codes
- 📘 API: [Strings and identifiers](strings-and-identifiers.md) for `string_id`, the hashed counterpart of `uuid`
- 📘 API: [Glossary](../glossary.md)
<!-- doc-content:related:end -->

<!-- doc-legacy-anchors:start -->
<a id="see-also"></a>
<!-- doc-legacy-anchors:end -->

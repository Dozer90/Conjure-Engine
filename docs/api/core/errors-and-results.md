# Errors and results

📘 **Public API wiki** · 🛠 Handbook: [Core module](../../handbook/modules/core.md)

Conjure never throws. A function that can fail at runtime (I/O, parsing, device creation, a lookup that may miss)
returns `result<T>`: either a value or an `error`. Include `conjure/core/result.h`; it brings in
`conjure/core/error.h`, which defines `error`, the generic `error_code` enumeration and `make_error`.

## At a glance

| You want to... | Use |
|---|---|
| Return a value or a failure | `result<T>`; return the value, or `make_error(code, "why {}", detail)` |
| Report success or failure with no value | `status` (an alias for `result<void>`); return `{}` or `status::success()` on success |
| Check a result | `if (outcome.has_value())` or `if (outcome)` |
| Read the value | `outcome.value()`, `*outcome`, `outcome->member` (only after checking) |
| Read the value with a default | `outcome.value_or(fallback)` |
| Read the failure | `outcome.get_error()`, then `.is(code)`, `.message()`, `.to_string()` |
| Pass a failure up to your caller | `CONJURE_TRY(expression);` or `CONJURE_TRY_ASSIGN(auto value, expression);` |
| Define error codes for your own system | an `enum class my_error : u16` plus `CONJURE_DECLARE_ERROR_CATEGORY` |

## Quick example

```cpp
#include "conjure/core/log.h"
#include "conjure/core/result.h"

using namespace conjure;
using namespace conjure::core;

result<int> divide(int numerator, int denominator) {
    if (denominator == 0) {
        return make_error(error_code::INVALID_ARGUMENT, "division by zero ({} / {})", numerator, denominator);
    }
    return numerator / denominator;
}

result<int> add_quotients(int firstNumerator, int firstDenominator, int secondNumerator, int secondDenominator) {
    CONJURE_TRY_ASSIGN(const int first, divide(firstNumerator, firstDenominator));
    CONJURE_TRY_ASSIGN(const int second, divide(secondNumerator, secondDenominator));
    return first + second;
}

void report(int numerator, int denominator) {
    const result<int> outcome = divide(numerator, denominator);
    if (!outcome) {
        CONJURE_LOG_WARN(LOG_CORE, "divide failed: {}", outcome.get_error());
        return;
    }
    CONJURE_LOG_INFO(LOG_CORE, "quotient {}", *outcome);
}
```

`LOG_CORE` is the engine's fallback log category; your own systems declare their own (see
[Logging and assertions](logging-and-assertions.md)).

## Returning a result

`result<T>` converts implicitly from a `T` and from an `error`, so a function returns either one directly:

```cpp
result<std::string> read_name(bool present) {
    if (!present) {
        return error(error_code::NOT_FOUND, "no name");
    }
    return std::string("player");
}
```

`T` may be move-only (`std::unique_ptr`, a file handle) and need not be default constructible. Two shapes are
rejected at compile time: `result<error>` (ambiguous) and `result<T &>` (no references; return a pointer or an
index instead).

`result<void>`, spelled `status`, carries no value. A default-constructed `status` (`return {};`) and
`status::success()` both mean success.

## Checking and reading a result

| Call | Behaviour |
|---|---|
| `has_value()` / `explicit operator bool` | true when a value is held |
| `value()`, `*outcome`, `outcome->member` | the value; moves it out when called on a temporary |
| `value_or(fallback)` | the value, or `fallback` converted to `T` |
| `get_error()` | the error; moves it out when called on a temporary |

> [!CAUTION]
> Calling `value()` (or `*`, `->`) on a result that holds an error, or `get_error()` on one that holds a value, is a
> programmer error: it asserts and then terminates the process, even when an assert handler asks to continue. Always
> check first, or use `value_or()`.

> [!TIP]
> `result` is `[[nodiscard]]`. If a call only reports success or failure and you genuinely do not care, say so
> explicitly with `CONJURE_UNUSED(file.close());` rather than ignoring the warning.

## Propagating failures

Most functions do not handle a failure themselves; they pass it to their caller. Two macros do that in one line.

- `CONJURE_TRY(expression);` evaluates a result-returning expression and, if it failed, returns its error from the
  enclosing function. Use it with `status` results and with results whose value you do not need.
- `CONJURE_TRY_ASSIGN(declaration, expression);` does the same and otherwise initialises `declaration` with the
  value, for example `CONJURE_TRY_ASSIGN(auto handle, open_file(path));`.

Both work in any function that returns a `result<U>` or `status`, whatever `U` is: only the error travels.

```cpp
status validate_positive(int value) {
    if (value <= 0) {
        return error(error_code::OUT_OF_RANGE, "value must be positive");
    }
    return status::success();
}

status validate_all_positive(int first, int second) {
    CONJURE_TRY(validate_positive(first));
    CONJURE_TRY(validate_positive(second));
    return {};
}
```

> [!WARNING]
> The macros have three deliberate limits. Pass the call expression itself, not a named result: a named result is
> moved from, so its error would be gone afterwards. Use each macro at most once per source line (the hidden
> variable is named after the line). A declaration with an unparenthesised comma (`std::pair<int, int> p`) needs a
> type alias first.

## What an error is

An `error` has three parts: a **category** (which system produced it, named like `"core"` or `"ecs"`), a **code**
(that system's enumerator) and an optional human-readable **message**. The category is stored as the FNV-1a 32 hash
of its name, so two systems can never collide on codes.

| Call | Returns |
|---|---|
| `is(code)` | true when the error carries exactly this code of this category |
| `is_category<my_error>()` | true when the error belongs to that category |
| `code()`, `code_name()` | the raw code and its enumerator name (`"NOT_FOUND"`, or `""` when unknown) |
| `category()`, `category_name()` | the category hash and name |
| `generic_code()` | the `error_code` when the category is `"core"`, otherwise `error_code::NONE` |
| `message()` | the detail text, possibly empty |
| `to_string()` | `"category.CODE: message"` (or `"category.CODE"`) |

`error` and `error_code` both have `std::formatter` specialisations, so `std::format("{}", failure)` and the
logging macros print them without extra work. Errors compare equal when category, code and message all match.

> [!TIP]
> Compare with `failure.is(my_error::SLOT_EMPTY)` rather than `generic_code()`: `generic_code()` is `NONE` for
> every error that is not a core error.

### Generic codes

Any system may return a generic code from `conjure/core/error.h`:

| Code | Use when |
|---|---|
| `INVALID_ARGUMENT`, `OUT_OF_RANGE` | the caller passed something unusable |
| `INVALID_STATE` | the call is not allowed right now (not initialised, already closed) |
| `NOT_FOUND`, `ALREADY_EXISTS` | a lookup missed, or a creation found a duplicate |
| `OUT_OF_MEMORY` | an allocation failed |
| `UNSUPPORTED`, `NOT_IMPLEMENTED` | the platform or build cannot do it / nobody wrote it yet |
| `IO_FAILURE`, `PARSE_FAILURE` | reading or writing failed / the data was malformed |
| `TIMEOUT`, `CANCELLED` | a wait expired / the operation was abandoned |
| `PERMISSION_DENIED` | the operating system refused |
| `BACKEND_FAILURE`, `DEPENDENCY_FAILURE` | a third-party library failed / something this call relies on failed |
| `UNKNOWN` | nothing better fits |

`to_string(error_code)` returns the upper-case name of a code.

### Building an error

`make_error` has three shapes:

```cpp
error describe_wait(int elapsedMilliseconds) {
    if (elapsedMilliseconds < 0) {
        return make_error(error_code::INVALID_ARGUMENT);                        // code only
    }
    if (elapsedMilliseconds == 0) {
        return make_error(error_code::NOT_FOUND, "nothing arrived {}");         // one fixed string, braces kept
    }
    return make_error(error_code::TIMEOUT, "after {} ms", elapsedMilliseconds); // std::format style
}
```

`error` also has direct constructors for the generic codes: `error(error_code::IO_FAILURE)` and
`error(error_code::IO_FAILURE, "disk full")`.

## Declaring your own error category

Each game system (or module) declares its own enumeration and binds it to a named category once, in a public
header. By engine convention the enumeration uses `u16`, starts at `NONE = 0` and ends with `COUNT`; the macro
itself only requires that the values fit in 32 bits. A small function returns each enumerator's name so errors
print readably.

```cpp
#include "conjure/core/error.h"

#include <string_view>

namespace game {
    enum class inventory_error : conjure::u16 {
        NONE = 0,
        SLOT_EMPTY,
        SLOT_FULL,
        COUNT
    };

    inline std::string_view inventory_error_name(inventory_error code) noexcept {
        switch (code) {
            case inventory_error::NONE: {
                return "NONE";
            }
            case inventory_error::SLOT_EMPTY: {
                return "SLOT_EMPTY";
            }
            case inventory_error::SLOT_FULL: {
                return "SLOT_FULL";
            }
            case inventory_error::COUNT: {
                return "";
            }
        }
        return "";
    }
}

CONJURE_DECLARE_ERROR_CATEGORY(game::inventory_error, "inventory", game::inventory_error_name);
```

From then on `make_error(game::inventory_error::SLOT_FULL, "slot {}", index)` builds an error whose `to_string()`
is `"inventory.SLOT_FULL: slot 3"` and which answers `is(game::inventory_error::SLOT_FULL)`. `error::from(code,
message)` is the lower-level form `make_error` uses.

> [!IMPORTANT]
> Expand `CONJURE_DECLARE_ERROR_CATEGORY` outside your own namespace (at global scope is simplest; it specialises a
> template inside `conjure::core`, so it cannot sit inside `namespace game`), exactly once per enumeration, in a
> header every user of the enumeration includes.

## API summary

| Signature (short) | Purpose |
|---|---|
| `result<T>(const T &)` / `result<T>(T &&)` / `result<T>(error)` | construct from a value or an error |
| `bool has_value() const` / `explicit operator bool() const` | holds a value |
| `T &value()` / `T &operator*()` / `T *operator->()` | the value; fatal on an error result |
| `T value_or(Fallback &&) const` | value or fallback |
| `const error &get_error() const` | the error; fatal on a value result |
| `using status = result<void>` | success or error |
| `static status status::success()` | explicit success |
| `CONJURE_TRY(expression);` | return the error of `expression` if it failed |
| `CONJURE_TRY_ASSIGN(declaration, expression);` | ...otherwise bind the value |
| `enum class error_code : u16` | generic codes, `NONE` .. `CANCELLED` |
| `std::string_view to_string(error_code)` | code name |
| `error()` / `error(error_code)` / `error(error_code, std::string)` | generic errors |
| `error make_error(E code)` / `make_error(E code, std::string_view)` / `make_error(E code, fmt, args...)` | build an error from any bound enumeration |
| `static error error::from(E code, std::string message = {})` | same, without formatting |
| `bool error::is(E code) const` / `bool is_category<E>() const` | classify |
| `u32 code() const` / `std::string_view code_name() const` / `error_code generic_code() const` | inspect the code |
| `u32 category() const` / `std::string_view category_name() const` | inspect the category |
| `const std::string &message() const` / `std::string to_string() const` | text |
| `CONJURE_DECLARE_ERROR_CATEGORY(enum_type, "name", name_function)` | bind an enumeration to a category |

## See also

- 📘 API: [Logging and assertions](logging-and-assertions.md): errors are for runtime failures, asserts for programmer errors
- 📘 API: [Data types](data-types.md) for `u16` and the enumeration conventions
- 📘 API: [Glossary](../glossary.md)

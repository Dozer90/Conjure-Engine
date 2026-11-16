# Coding standards

🛠 **Engineering handbook** · 📘 API: [Errors and results](../api/core/errors-and-results.md), [Logging and assertions](../api/core/logging-and-assertions.md)

How Conjure code is written. `docs/CODE_STYLE.cpp` is the authoritative style reference and `docs/CONVENTIONS.md`
adds the project rules on top; this chapter explains both with short right/wrong examples and shows the error,
logging and assertion model you must use. `tools/check_style.py` (CTest entry `style.check`) enforces the
mechanical part, and the review checklist in `.claude/skills/code-review-checklist/SKILL.md` is run before
every commit.

## Contents
- [1. Formatting and naming](#1-formatting-and-naming)
- [2. Layout of a class](#2-layout-of-a-class)
- [3. Language rules](#3-language-rules)
- [4. Header hygiene](#4-header-hygiene)
- [5. Error model](#5-error-model)
- [6. Assertions](#6-assertions)
- [7. Logging](#7-logging)
- [8. Windows macro names](#8-windows-macro-names)
- [9. Tests](#9-tests)
- [10. Review checklist and tooling](#10-review-checklist-and-tooling)
- [Related](#related)

## 1. Formatting and naming

Four spaces, no tabs, LF endings, at most 120 columns (aim for about 100), braces on every control statement,
early-out instead of nesting, one declaration per line, every variable initialised, `*` and `&` hugging the name.

| Thing | Style | Example |
|---|---|---|
| Namespaces, classes, structs, enums, functions, files | snake_case | `conjure::core::jobs`, `class job_system`, `void stop()`, `job_system.cpp` |
| Locals and parameters | camelCase | `const usize chunkIndex = 0;` |
| Class members | `m` prefix | `u32 mWorkerCount = 0;` |
| Plain aggregate struct fields | camelCase, no prefix (R31) | `u32 workerCount = 0;` in `job_system_desc` |
| Statics (members and file scope) | `s` prefix | `static std::atomic<int> sInstanceCount;` |
| Constants, enumerators, macros | ALL_CAPS; macros carry `CONJURE_` | `inline constexpr u32 MAX_WORKER_COUNT = 256;`, `job_priority::HIGH`, `CONJURE_ASSERT` |
| Template parameters | PascalCase | `template <typename ElementType>` |

```cpp
// Wrong: nested, unbraced, uninitialised, two declarations on one line.
int a, b;
if (ready)
    if (count > 0) { work(); }

// Right: early-out, braces, one initialised declaration per line.
int first = 0;
int second = 0;
if (!ready || count == 0) {
    return;
}
work();
```

Argument lists that do not fit on one line move *every* argument to its own line with two spaces of indent
and keep the closing parenthesis on its own line followed by the opening brace (or the semicolon):

```cpp
// Right (from conjure/core/jobs/job_system.h)
[[nodiscard]] job_handle schedule(
  std::span<const std::function<void()>> jobs,
  job_priority priority = job_priority::NORMAL
);

// Wrong: only some arguments wrapped
[[nodiscard]] job_handle schedule(std::span<const std::function<void()>> jobs,
  job_priority priority = job_priority::NORMAL);
```

Comments explain *why*; code explains *what*. Long files are split with the comment-only heading blocks of the
style guide (`/*====*/` for sections, `/*----*/` for sub-sections); `#region` / `#pragma region` are never used.

## 2. Layout of a class

Access modifier first, then variables, constructors/destructors, methods, each block starting with its own
access modifier. Constructors and destructors are defined in the `.cpp` even when empty, except `= default`,
`= delete` and `constexpr` constructors of value types, which stay in the header. Public methods carry a
`/// <summary>` block.

```cpp
class stopwatch {
    private:
        const clock *mClock = nullptr;
        f64 mStartSeconds = 0.0;

    public:
        stopwatch() noexcept;
        explicit stopwatch(const clock &source) noexcept;
        ~stopwatch();

    public:
        /// <summary>
        /// Seconds elapsed since the last start, restart or construction.
        /// </summary>
        [[nodiscard]] f64 elapsed_seconds() const noexcept;
};
```

Initialiser lists put the colon on the constructor line and one member per following line (`my_class_a() :
    my_class_b() {}`). Structs are plain data: simple methods are fine, inheritance and complex logic are not.

## 3. Language rules

| Rule | Right | Wrong |
|---|---|---|
| No owning raw pointers; `new`/`delete` only inside `core::memory`, `malloc` nowhere else | `std::unique_ptr<state> mState`, engine handles, value types | `state *mState = new state();` |
| `enum class` with explicit underlying type everywhere | `enum class log_level : u8 { TRACE = 0, ... };` | `enum log_level { TRACE, ... };` |
| Flag sets: `enum class <x>_flags`, `NONE = 0`, operators from `CONJURE_ENUM_FLAGS(type);` (`conjure/core/enum_flags.h`) | `enum class enumerate_flags : u8 { NONE = 0, RECURSIVE = 1 << 0 }; CONJURE_ENUM_FLAGS(enumerate_flags);` | plain `enum` flags, or hand-written operators |
| `[[nodiscard]]` on functions returning results, handles or values that must be checked | `[[nodiscard]] result<void> start(...)` | silently droppable `result` |
| `const`, `constexpr`, `noexcept` where they apply | `[[nodiscard]] u32 worker_count() const noexcept;` | mutable getters |
| Prefer `std::span`, `std::string_view`, `std::array`, `std::optional` | `std::span<const std::byte> data` | `const std::byte *data, usize size` |
| No `std::thread` outside `core::jobs`; no platform `#ifdef` outside `conjure/core/platform_detect.h` and backend files | `*_windows.cpp` / `*_posix.cpp` selected by CMake | `#ifdef _WIN32` in shared code |
| Engine code never throws or catches | `result<T>` for runtime failure, `CONJURE_ASSERT` for programmer error | `throw std::runtime_error` |
| No `using namespace` in headers (allowed in a class's own `.cpp`) | `using namespace conjure::core::jobs;` in `job_system.cpp` | the same line in a header |
| Members without a default initialiser only when no sensible default exists, with a comment saying why | `std::variant<ValueType, error> mStorage;   // set by every constructor` | an uninitialised `int mCount;` |

> [!TIP]
> Prefer `core::string_id` over `std::string` for names compared at runtime, and `slot_map` keys over raw
> pointers for anything that can be destroyed while referenced. [Data-oriented design](data-oriented-design.md)
> explains why.

## 4. Header hygiene

- `#pragma once` in every header; no include guards.
- Include order, each group separated by a blank line: own module headers, other Conjure headers, third-party,
  standard library. Conjure headers in quotes (`#include "conjure/core/log.h"`), everything else in angle
  brackets. Test files put `conjure/testing/doctest.h` first, then the helpers next to the file.
- Public headers live in `engine/include/conjure/<module>/` (core areas: `conjure/core/<name>.h` or
  `conjure/core/<area>/<name>.h`), compile on their own (`conjure_header_check` builds each one in its own
  translation unit as part of the default build) and never include a third-party header; glm is the single
  exception through `conjure/core/math/glm_types.h`. NVRHI, Jolt, GLFW, miniaudio, ImGui, yyjson and stb stay
  in `engine/src/`.
- One class per header where practical; small helper structs may share their owner's header.
- Every public header has `tests/<module>/test_<name>.cpp` (core: `tests/core/<area>/test_<name>.cpp`), or an
  entry with a reason in `tools/style_exceptions.json`.
- Classes with out-of-line members and free functions defined in a `.cpp` will carry `CONJURE_API`
  (`conjure/core/api.h`) once the shared umbrella exists; templates, `inline`, `constexpr`, enums and plain
  data structs are never annotated (`07_modules.md` D1).

## 5. Error model

Runtime failures (I/O, parsing, device creation, bad input) return `conjure::core::result<T>`; the void form is
`result<void>` with the alias `status` (`conjure/core/result.h`). An error is a category, a code and a message
(`conjure/core/error.h`). Core owns the generic `error_code` enum (`NONE`, `UNKNOWN`, `INVALID_ARGUMENT`,
`INVALID_STATE`, `NOT_FOUND`, `ALREADY_EXISTS`, `OUT_OF_RANGE`, `OUT_OF_MEMORY`, `UNSUPPORTED`,
`NOT_IMPLEMENTED`, `IO_FAILURE`, `PARSE_FAILURE`, `TIMEOUT`, `PERMISSION_DENIED`, `BACKEND_FAILURE`,
`DEPENDENCY_FAILURE`, `CANCELLED`, plus the `COUNT` sentinel; the header is the authority if this list and
`conjure/core/error.h` ever differ); every other module binds its own `enum class <module>_error : u16` to a
category once, in its public error header, and builds errors with the generic `make_error` (no per-module
overload, R63):

```cpp
// conjure/widgets/widgets_error.h
enum class widgets_error : u16 { NONE = 0, WIDGET_MISSING, WIDGET_JAMMED, COUNT };
std::string_view widgets_error_name(widgets_error code) noexcept;    // enumerator name, "" when unknown
CONJURE_DECLARE_ERROR_CATEGORY(widgets_error, "widgets", widgets_error_name);

// widgets.cpp
result<int> find_widget(bool present) {
    if (!present) {
        return make_error(widgets_error::WIDGET_MISSING, "widget {} not found", 7);
    }
    return 7;
}
```

> [!WARNING]
> `make_error(code, "text with {braces}")` with no further arguments picks the `std::string_view` overload and
> keeps the braces verbatim; the `std::format_string` overload is chosen only when at least one argument follows.

Propagation uses `CONJURE_TRY(expression)` and `CONJURE_TRY_ASSIGN(declaration, expression)`; check with
`has_value()` or `operator bool`, test a code with `error::is(code)`, read the detail with `message()`:

```cpp
status validate_all_positive(int first, int second) {
    CONJURE_TRY(validate_positive(first));
    CONJURE_TRY(validate_positive(second));
    return {};
}

result<std::string> describe_quotient(int numerator, int denominator) {
    CONJURE_TRY_ASSIGN(const int quotient, divide(numerator, denominator));
    return std::to_string(quotient);
}

if (const result<int> value = divide(1, 0); !value) {
    CONJURE_LOG_WARN(LOG_CORE, "divide failed: {}", value.get_error());   // "core.INVALID_ARGUMENT: ..."
}
```

> [!CAUTION]
> `result::value()` on an error and `get_error()` on a value assert and then `std::abort()`, even when the
> assertion handler asks to continue: there is nothing to dereference. Check first.

> [!WARNING]
> `CONJURE_TRY` binds its expression by forwarding reference and moves the error out of a named lvalue; pass the
> call expression directly. Two uses on one source line do not compile (the helper name comes from `__LINE__`).

## 6. Assertions

`conjure/core/assert.h` gives three macros plus a replaceable handler:

| Macro | Debug / Development | Distribution |
|---|---|---|
| `CONJURE_ASSERT(condition, fmt, ...)` | checks; failure goes to the handler | compiles out (arguments stay type-checked in an unevaluated operand) |
| `CONJURE_VERIFY(condition, fmt, ...)` | checks | still evaluates `condition`, drops the check; use it for calls with side effects |
| `CONJURE_UNREACHABLE()` | asserts, then `std::abort()` | `CONJURE_UNREACHABLE_HINT()` |
| `CONJURE_FATAL_ASSERT(condition, fmt, ...)` | checks in every configuration (memory corruption class of problems) | checks |

The handler (`set_assert_handler`, an atomic function pointer) returns `assert_action::BREAK`, `CONTINUE` or
`ABORT`; tests install `conjure::testing::expect_assert` / `CONJURE_CHECK_ASSERTS(statement)` instead of
breaking. Asserts are for programmer errors (contract violations); recoverable runtime failures return a
`result`. Where a continuing handler would otherwise corrupt state, assert *and* early-out (the containers do
this for position and range arguments).

## 7. Logging

Each module declares one category in `<module>/log_category.h` and defines it in one `.cpp`:

```cpp
CONJURE_DECLARE_LOG_CATEGORY(LOG_RENDERER);                // header
CONJURE_DEFINE_LOG_CATEGORY(LOG_RENDERER, "renderer");     // one .cpp

CONJURE_LOG_INFO(LOG_RENDERER, "Created device {} ({} MiB)", name, megabytes);
```

Levels are `log_level::TRACE, DEBUG, INFO, WARN, ERR, FATAL, OFF` (`conjure/core/log.h`); `ERR`, not `ERROR`,
because `ERROR` is a Windows macro. `CONJURE_LOG_TRACE` / `CONJURE_LOG_DEBUG` compile out of Distribution. The
message is formatted with `std::format` only when both the category level and `log::global_level()` allow it.
Sinks (`console_log_sink`, `file_log_sink`, `callback_log_sink`, or your own `log_sink`) are added with
`log::add_sink(std::shared_ptr<log_sink>)`, receive messages from any thread outside the logger lock and must
synchronise themselves. No `printf` / `std::cout` outside the console sink and tools. In tests
`conjure::testing::log_capture` fails the case on any `ERR` unless `allow_errors()` is called.

## 8. Windows macro names

Never name an enumerator or constant after a Windows macro; `tools/check_style.py` rule `windows-macro` fails
the build on the known offenders: `ERROR`, `DELETE`, `IN`, `OUT`, `OPTIONAL`, `TRUE`, `FALSE`, `NEAR`, `FAR`,
`OPAQUE`, `TRANSPARENT`, `ABSOLUTE`, `RELATIVE`, `DOMAIN`, `DIFFERENCE`, `IGNORE`, `INFINITE`, `TEXT`,
`interface`, `min`, `max`. Use `ERR`, `REMOVE` / `DEL`, `INPUT` / `OUTPUT`, `NEAR_PLANE` / `FAR_PLANE` instead
(`log_level::ERR` and `key::DEL` are the engine's own examples, R56).

## 9. Tests

doctest through `conjure/testing/doctest.h` only; one `tests/<module>/test_<name>.cpp` per public header,
opening with `TEST_SUITE("<module>/<name>")`; cases named `"<type> - <behaviour in plain English>"` and unique
per executable (each case is one CTest entry `<module>.<case>`); variants as `SUBCASE`s; `REQUIRE` only when
continuing would dereference an invalid result, `CHECK` otherwise; results through `CONJURE_REQUIRE_OK` /
`CONJURE_CHECK_OK` / `CONJURE_CHECK_ERROR(expression, errorCode)`; floats through `CONJURE_CHECK_APPROX`. Every
public type gets construction, happy path, failure path (each `error_code` the function can return) and edge
cases. The binding rules are `docs/CONVENTIONS.md` 6 and `docs/design/08_testing.md` 5.

## 10. Review checklist and tooling

Before every commit, run the checklist in `.claude/skills/code-review-checklist/SKILL.md` over the whole diff
(correctness, style, conventions, hygiene) and:

```
python tools/build.py --test                                 # Debug, warnings as errors, every test
python tools/check_style.py --tests-mirror-headers           # also the CTest entry style.check
dotnet format --verify-no-changes managed/<project>/<project>.csproj   # only for a change under managed/
```

`check_style.py` checks tabs, line length, CRLF, trailing whitespace, `#pragma once`, `using namespace` in
headers, Windows macro names, `enum class`, include order and grouping, third-party includes in public headers,
the doctest include, test case names, forbidden calls in tests, `std::thread` / `std::jthread` outside
`core/jobs` and snake_case file names. Commit subjects are `module: imperative summary`; every commit builds and
passes tests on its own. The `dotnet format` line is the C# layout gate of CI: run it for `Conjure.Managed`,
`Conjure.Bootstrap` and `Conjure.TestScripts` (rules in the `[*.cs]` section of `.editorconfig`,
`docs/design/08_testing.md` D17).

## Related
- `docs/CODE_STYLE.cpp` (authoritative style, customer supplied; apply R31 for struct fields, not its example),
  `docs/CONVENTIONS.md` (project rules), `AGENTS.md` section 2 (non-negotiable rules)
- design records: [`../design/08_testing.md`](../design/08_testing.md) 5 (doctest conventions),
  [`../design/09_cross_area_resolutions.md`](../design/09_cross_area_resolutions.md) R31, R56, R63,
  [`../design/10_core.md`](../design/10_core.md) C3-C6
- handbook: [Architecture](architecture.md), [Data-oriented design](data-oriented-design.md),
  [Core module](modules/core.md)

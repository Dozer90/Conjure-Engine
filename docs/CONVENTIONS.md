# Conjure Engine - Engineering Conventions

These rules apply to every file in the repository. They are enforced in code review and must be
followed by every contributor, human or AI. `docs/CODE_STYLE.cpp` is the authoritative style
reference; this document adds the project-specific conventions on top of it.

## 1. Naming

| Thing | Style | Example |
|---|---|---|
| Namespaces | snake_case | `conjure::ecs` |
| Classes / structs / enums / type aliases | snake_case | `class entity_world`, `struct aabb` |
| Functions / methods | snake_case | `void destroy_entity(entity id)` |
| Local variables / parameters | camelCase | `const int chunkIndex = 0;` |
| Class member variables | camelCase with `m` prefix | `int mEntityCount = 0;` |
| Plain struct (aggregate) fields | camelCase, no prefix (R31 of `docs/design/09_cross_area_resolutions.md`) | `u32 chunkIndex = 0;` |
| Static members and file-scope statics | camelCase with `s` prefix | `static int sInstanceCount;` |
| Constants / constexpr / enum values / macros | ALL_CAPS | `constexpr int MAX_COMPONENTS = 256;` |
| Template parameters | PascalCase | `template <typename ComponentType>` |
| Files | snake_case | `entity_world.h`, `entity_world.cpp` |
| Public headers | `engine/include/conjure/<module>/<name>.h` (core areas: `conjure/core/<name>.h`, or `conjure/core/<area>/<name>.h` for large areas) | `conjure/ecs/entity_world.h` |
| Private headers / sources | `engine/src/<module>/<name>.{h,cpp}` | |
| Tests | `tests/<module>/test_<name>.cpp` | `tests/ecs/test_entity_world.cpp` |
| CMake targets | `conjure_<module>`, tests `conjure_<module>_tests`; core areas `conjure_core_<area>` / `conjure_core_<area>_tests` | |
| Macros | `CONJURE_` prefix | `CONJURE_ASSERT(x)` |

Abbreviations: only widely understood ones (`id`, `uuid`, `rhi`, `ecs`, `gpu`, `json`, `glm`).
No Hungarian notation beyond the `m`/`s` prefixes.

## 2. Files and headers

- `#pragma once` in every header. No include guards.
- Include order: own module headers, other Conjure headers, third-party, then standard library.
  Use quotes for Conjure headers (`#include "conjure/core/log.h"`) and angle brackets for others.
- One class per header where practical. Small helper structs may share the header of their owner.
- Every public header must compile on its own (include what you use).
- Public headers must not include third-party headers (glm is the only exception, through
  `conjure/core/math.h`). NVRHI, Jolt, GLFW, miniaudio, ImGui, yyjson stay in `src/`.
- Use the heading comment blocks from the style guide to split large files into regions.

## 3. Language rules

- C++20. No compiler extensions.
- Engine code never throws and never catches (third-party code may). Return
  `conjure::core::result<T>` or `conjure::core::error` for recoverable failures and use
  `CONJURE_ASSERT` for programmer errors.
- No raw owning pointers. Use `std::unique_ptr`, `std::shared_ptr` (sparingly), engine handles,
  or value types. Raw pointers and references are non-owning views only.
- No `new`/`delete` outside allocators. No `malloc` outside `core::memory`.
- Prefer `std::span`, `std::string_view`, `std::array`, `std::optional`.
- All variables initialised at declaration. One declaration per line.
- `enum class` with explicit underlying type for every enumeration, including flag sets. Flag
  enumerations end in `_flags`, have a `NONE = 0` member and get their operators from
  `CONJURE_ENUM_FLAGS(type);` (the macro call takes a semicolon). This is the project-specific
  refinement of the style guide's optional plain-enum flags: scoped enums keep flag names out of
  the global namespace where Windows macros live.
- Constructors and destructors are defined in the `.cpp` (even when empty), except `= default`,
  `= delete` and `constexpr` constructors of value types and small helper classes, which stay in
  the header. Members without a default initialiser are allowed only when the type has no
  sensible default (e.g. a `std::variant` set by every constructor) and carry a comment saying why.
- The style guide's `#region` / `#endregion` markers are not standard C++; use the comment-only
  heading blocks shown in the guide (as the engine does) and never `#pragma region`.
- `[[nodiscard]]` on functions returning results or handles.
- `const` and `constexpr` everywhere they apply. `noexcept` on move operations and trivial getters.
- No `using namespace` in headers. `using namespace conjure::<module>;` is allowed in the `.cpp`
  of a class belonging to that module (per the style guide).
- Threading: no raw `std::thread` outside `core::jobs`. Shared state is documented and guarded.
- Platform-specific code lives behind backend interfaces; no `#ifdef _WIN32` outside
  `core/platform_detect.h` and backend implementation files.
- Never name an enumerator or constant after a Windows macro. Known offenders: `ERROR`, `DELETE`,
  `IN`, `OUT`, `OPTIONAL`, `TRUE`, `FALSE`, `NEAR`, `FAR`, `OPAQUE`, `TRANSPARENT`, `ABSOLUTE`,
  `RELATIVE`, `DOMAIN`, `DIFFERENCE`, `IGNORE`, `INFINITE`, `TEXT`, `WIN32` (CMake defines it on every
  Windows build), `interface`, `min`, `max`. Use `ERR`, `REMOVE`, `INPUT`/`OUTPUT`,
  `NEAR_PLANE`/`FAR_PLANE`, `WINDOWS`, etc. instead. `tools/check_style.py` knows the list.

## 4. Error handling and logging

- `CONJURE_LOG_TRACE/DEBUG/INFO/WARN/ERROR(category, fmt, ...)` using `std::format` syntax.
  Each module declares its own category in `<module>/log_category.h`.
- `CONJURE_ASSERT(condition, "message")` and `CONJURE_VERIFY(condition, ...)` both check in
  Debug and Development. In Distribution `CONJURE_ASSERT` compiles out entirely while
  `CONJURE_VERIFY` still evaluates its expression (use it for calls with side effects).
  `CONJURE_UNREACHABLE()` is fatal in every configuration.
- Functions that can fail at runtime (I/O, parsing, device creation) return `result<T>`.

## 5. Documentation

- Public methods carry `/// <summary>` blocks as in the style guide. Private ones only when useful.
- Comments explain *why*. The code explains *what*.
- Each module has `engine/src/<module>/README.md`: purpose, main types, threading model, how to
  test it.

## 6. Testing

The binding design is `docs/design/08_testing.md` (sections 5.1 and 7.4 in particular); this section
is its summary.

- doctest, included only through `conjure/testing/doctest.h`. Every module has a
  `conjure_<module>_tests` executable declared with
  `conjure_add_test_module(<module> SOURCES ... [DEPS ...] [LABELS ...] [TIMEOUT s])` from
  `cmake/ConjureTesting.cmake`; every `TEST_CASE` becomes one CTest entry named `<module>.<case>`
  (labels `unit;<module>;...`, `SKIP_RETURN_CODE 77`), so case names must be unique within the
  executable and must not contain `;` or unbalanced `[` / `]`.
- `tests/<module>/test_<name>.cpp` mirrors `engine/include/conjure/<module>/<name>.h` one to one
  (core areas: `tests/core/<area>/test_<name>.cpp`); a header without a test file fails
  `tools/check_style.py --tests-mirror-headers` unless listed with a reason in
  `tools/style_exceptions.json`. Each file opens with `#include "conjure/testing/doctest.h"` and
  `TEST_SUITE("<module>/<name>")`.
- `TEST_CASE("<type> - <behaviour in plain English>")`, e.g.
  `"slot_map - erase invalidates the handle generation"`. Variants are `SUBCASE`s, never copies.
  Loops use `CAPTURE(index)`. Name tags, filterable with `-tc` when running an executable directly:
  `[slow]` (> 1 s), `[parallel]` (asserts on multi-worker behaviour), `[scripting]`, `[window]`
  (needs a real window; guarded by `CONJURE_TEST_REQUIRE(precondition::REAL_WINDOW)`). Every bug fix
  adds `TEST_CASE("regression - <issue id> <summary>")`.
- `REQUIRE` only when continuing would dereference an invalid result; `CHECK` otherwise. Floats use
  `doctest::Approx` or `CONJURE_CHECK_APPROX`. Results use `CONJURE_REQUIRE_OK`, `CONJURE_CHECK_OK`
  and `CONJURE_CHECK_ERROR(expression, errorCode)`. Every fixture is a local in the case body
  (`TEST_CASE_FIXTURE` allowed; fixture inheritance not).
- Support types (`tests/support/include/conjure/testing/`, namespace `conjure::testing`):
  `log_capture` (fails the case on an ERROR unless `allow_errors()`), `expect_assert` and
  `CONJURE_CHECK_ASSERTS(statement)` for code that must assert, `memory_guard` (live allocations
  back to the baseline), `allocation_scope` and `CONJURE_CHECK_NO_ALLOCATIONS(statement)` for
  allocation gates, `temp_directory` for file I/O, `repo_asset()` / `downloaded_asset()`,
  `environment()` for the seed and thread count, `CONJURE_TEST_REQUIRE(precondition)` as the first
  statement of a case with a machine precondition (exits 77 under CTest, returns in a direct run).
- Forbidden in tests: `sleep_for`, wall-clock reads, `std::thread` (use `core::jobs`), randomness
  not derived from `environment().seed`, file I/O outside `temp_directory`, third-party headers
  (test through the public API), absolute paths, reading environment variables (only `test_main`
  does). `* doctest::skip()` requires a comment with the tracking issue; `[scripting]` cases carry
  `* doctest::timeout(60)`.
- Tests are headless: use the `null` backends. No test may require a GPU, window, or audio device.
- Every public type gets at least: construction/destruction, happy path, failure path (each
  `error_code` a `result` function can return), edge cases (empty, max, invalid or stale handle).
- Benchmarks live in `tests/benchmarks` and are not part of the default ctest run (the test presets
  exclude the labels `benchmark|golden|downloaded|slow|process`).
- `tools/check_style.py` runs as the CTest entry `style.check` (label `style`) and in CI; every
  public header also compiles alone through `conjure_header_check` (part of the default build).
- Run everything with: `cmake --preset windows-msvc-debug && cmake --build --preset windows-msvc-debug && ctest --preset windows-msvc-debug`.

## 7. CMake

- Modules are declared with `conjure_add_module(<name> SOURCES ... PUBLIC_DEPS ... PRIVATE_DEPS ...)`.
- Third-party dependencies are declared once in `cmake/Dependencies.cmake` (FetchContent, pinned
  tags/commits) and exposed as `conjure::thirdparty::<name>` interface targets.
- Warnings are errors. The warning set is in `cmake/CompilerWarnings.cmake`. Third-party targets
  have warnings suppressed via `SYSTEM` includes.
- No globbing for sources. List every file.

## 8. Git

- Conventional commit subjects: `core: add slot_map`, `renderer: implement cascaded shadows`.
- Every commit builds and passes tests. Code review before commit (see AGENTS.md).
- Never commit build outputs, downloaded assets, or IDE files (see `.gitignore`).

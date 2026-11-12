---
name: add-engine-module
description: Create a new Conjure engine module (static library + tests) with the correct layout, CMake registration, logging category, README and documentation pages. Use when adding a subsystem such as a new backend, importer family or gameplay system.
---

# Add an engine module

A module is a static library `conjure_<name>` (alias `conjure::<name>`) living in
`engine/src/<name>/` with public headers in `engine/include/conjure/<name>/` and tests in
`tests/<name>/`.

## Steps

1. **Register the name** in `engine/CMakeLists.txt` (`CONJURE_ENGINE_MODULES`, in dependency
   order) and `tests/CMakeLists.txt` (`CONJURE_TEST_MODULES`). Both lists are pre-populated for
   the planned modules; only edit them for a genuinely new module.

2. **Create the layout**

   ```
   engine/include/conjure/<name>/<type>.h          public headers, one class each
   engine/include/conjure/<name>/log_category.h    CONJURE_DECLARE_ENGINE_LOG_CATEGORY(LOG_<NAME>);
   engine/src/<name>/CMakeLists.txt
   engine/src/<name>/log_category.cpp              CONJURE_DEFINE_LOG_CATEGORY(LOG_<NAME>, "<name>");
   engine/src/<name>/<type>.cpp
   engine/src/<name>/README.md
   tests/<name>/CMakeLists.txt
   tests/<name>/test_<type>.cpp
   tests/<name>/test_log_category.cpp              LOG_<NAME> is registered as "<name>" and reaches the sinks
   docs/handbook/modules/<name>.md
   docs/api/<name>/index.md                        (+ topic pages)
   ```

   `log_category.h` declares the category inside `namespace conjure::<name>`; `log_category.cpp` includes
   `"conjure/<name>/log_category.h"` and defines it in the same namespace. The header is public and exported:
   `CONJURE_DECLARE_ENGINE_LOG_CATEGORY` declares the category `extern CONJURE_API` (`conjure/core/log.h`),
   so plugins and executables that link `conjure_shared` can log under it. Model the three files on
   `engine/include/conjure/ecs/log_category.h`, `engine/src/ecs/log_category.cpp` and
   `tests/ecs/test_log_category.cpp`.

3. **CMake** - `engine/src/<name>/CMakeLists.txt`:

   ```cmake
   if(NOT CONJURE_WITH_<FEATURE>)      # only for optional subsystems
       return()
   endif()

   conjure_add_module(<name>
       PUBLIC_HEADERS
           ${PROJECT_SOURCE_DIR}/engine/include/conjure/<name>/log_category.h
           ${PROJECT_SOURCE_DIR}/engine/include/conjure/<name>/<type>.h
       SOURCES
           log_category.cpp
           <type>.cpp
       PUBLIC_DEPS
           conjure::core
       PRIVATE_DEPS
           conjure::thirdparty::<lib>      # third-party stays private
   )
   ```

   `tests/<name>/CMakeLists.txt`:

   ```cmake
   if(NOT TARGET conjure::<name>)
       return()
   endif()

   conjure_add_test_module(<name>
       SOURCES
           test_log_category.cpp
           test_<type>.cpp
       LABELS <extra labels, optional>
       TIMEOUT 120
   )
   conjure_header_check(conjure_<name>)
   ```

   One `test_<type>.cpp` per public header, `log_category.h` included (`tools/check_style.py
   --tests-mirror-headers` enforces it); every `TEST_CASE` becomes its own CTest entry `<name>.<case>`.

4. **Backends.** If the module talks to hardware or an OS service, define an abstract backend
   interface in `engine/src/<name>/backend/<name>_backend.h`, implement the real one
   (`glfw_backend.cpp`, `miniaudio_backend.cpp`, ...) and a `null_backend.cpp` that behaves
   deterministically with no external resources. Tests use the null backend.

5. **Public API rules.** No third-party includes in public headers (glm excepted). Failures return
   `core::result<T>`. Handles over pointers. `<summary>` documentation on public methods. Every
   non-template public class that declares a member out of line, every namespace-scope function that is not
   a template, inline, constexpr, consteval or friend, and every `extern` variable carries `CONJURE_API`
   (`conjure/core/api.h`); `tools/check_exports.py` (CTest `exports.check_annotations` and the
   `style-and-headers` CI job) rejects one without it. Header-only code needs no annotation.

6. **Tests first or alongside.** Every public type: construction/destruction, happy path,
   failure path, edge cases, and any threading guarantees. Each file opens with
   `#include "conjure/testing/doctest.h"` and `TEST_SUITE("<name>/<type>")`; cases are named
   `TEST_CASE("<type> - <behaviour>")`. Use the `conjure::testing` helpers from
   `conjure/testing/*.h`: `log_capture`, `expect_assert` / `CONJURE_CHECK_ASSERTS`,
   `memory_guard`, `allocation_scope` / `CONJURE_CHECK_NO_ALLOCATIONS`, `temp_directory`,
   `CONJURE_REQUIRE_OK` / `CONJURE_CHECK_ERROR`, `CONJURE_CHECK_APPROX` (see
   `docs/CONVENTIONS.md` section 6).

7. **README.md** in `engine/src/<name>/`: purpose, main types, threading model, dependencies,
   how to test.

8. **Documentation.** Write `docs/handbook/modules/<name>.md` and the `docs/api/<name>/` pages, following
   `.claude/skills/write-docs/SKILL.md`. The wiki pages name every public header by include path,
   `conjure/<name>/<header>.h` (`log_category`, `*_error`, the umbrella header and anything under
   `detail/`, `internal/` or `generated/` are exempt). Register the pages in the `mkdocs.yml` nav and in the
   index tables (`docs/api/index.md`, `docs/handbook/index.md`). `tools/check_docs.py` (CTest `docs.check`)
   fails on a missing handbook page or an unreferenced public header, and `mkdocs build --strict` on a broken
   link or nav entry; a page missing from the nav only appears in the build's "not included in the nav"
   list, which write-docs treats as a mistake.

9. Build and test (`python tools/build.py --test -R <name>`), run `python tools/docs.py check`, run the
   review checklist, commit as `<name>: add module skeleton` (or a more specific subject).

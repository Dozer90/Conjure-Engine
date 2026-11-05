---
name: add-engine-module
description: Create a new Conjure engine module (static library + tests) with the correct layout, CMake registration, logging category and README. Use when adding a subsystem such as a new backend, importer family or gameplay system.
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
   engine/include/conjure/<name>/<type>.h        public headers, one class each
   engine/src/<name>/CMakeLists.txt
   engine/src/<name>/log_category.h               CONJURE_DECLARE_LOG_CATEGORY(LOG_<NAME>);
   engine/src/<name>/log_category.cpp             CONJURE_DEFINE_LOG_CATEGORY(LOG_<NAME>, "<name>");
   engine/src/<name>/<type>.cpp
   engine/src/<name>/README.md
   tests/<name>/CMakeLists.txt
   tests/<name>/test_<type>.cpp
   ```

3. **CMake** - `engine/src/<name>/CMakeLists.txt`:

   ```cmake
   if(NOT CONJURE_WITH_<FEATURE>)      # only for optional subsystems
       return()
   endif()

   conjure_add_module(<name>
       PUBLIC_HEADERS
           ${PROJECT_SOURCE_DIR}/engine/include/conjure/<name>/<type>.h
       SOURCES
           log_category.h
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
           test_<type>.cpp
   )
   ```

4. **Backends.** If the module talks to hardware or an OS service, define an abstract backend
   interface in `engine/src/<name>/backend/<name>_backend.h`, implement the real one
   (`glfw_backend.cpp`, `miniaudio_backend.cpp`, ...) and a `null_backend.cpp` that behaves
   deterministically with no external resources. Tests use the null backend.

5. **Public API rules.** No third-party includes in public headers (glm excepted). Failures return
   `core::result<T>`. Handles over pointers. `<summary>` documentation on public methods.

6. **Tests first or alongside.** Every public type: construction/destruction, happy path,
   failure path, edge cases, and any threading guarantees. Use `conjure_test.h` helpers
   (`log_capture`, `assert_capture`, `temp_directory`).

7. **README.md** in `engine/src/<name>/`: purpose, main types, threading model, dependencies,
   how to test.

8. Build and test (`python tools/build.py --test -R <name>`), run the review checklist, commit as
   `<name>: add module skeleton` (or a more specific subject).

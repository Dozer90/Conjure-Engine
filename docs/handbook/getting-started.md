# Getting started

🛠 **Engineering handbook**

This chapter takes a fresh machine to a green `ctest` run: what to install, how `tools/build.py` drives
CMake, which presets and configurations exist, how to run and filter tests, how to keep a private build
directory when several builds run at once, how the repository behaves in an IDE, and how to build the
documentation site.

## Contents

- [Prerequisites](#1-prerequisites)
- [Clone and first build](#2-clone-and-first-build)
- [Presets](#3-presets)
- [Configurations](#4-configurations)
- [Running tests](#5-running-tests)
- [Private build directories](#6-private-build-directories)
- [Build options](#7-build-options)
- [IDE notes](#8-ide-notes)
- [Documentation toolchain](#9-documentation-toolchain)

## 1. Prerequisites

| Tool | Version | Notes |
|---|---|---|
| CMake | 3.28+ | `CMakePresets.json` is schema version 8. The `windows-vs` preset generates for "Visual Studio 18 2026" and needs the CMake bundled with VS 2026 or CMake 4.1+. |
| Ninja | any recent | Used by every preset except `windows-vs`. On Windows `tools/build.py` adds the Ninja and CMake bundled with Visual Studio to `PATH` when the shell has none. |
| C++20 compiler | MSVC 19.40+ (VS 2022 17.10 or VS 2026), GCC 13+, Clang 16+ | Warnings are errors on every compiler (`cmake/ConjureWarnings.cmake`). |
| C++ standard library | libstdc++ 10+, MSVC 19.28+, libc++ 18+ (Xcode 16+) | `core::jobs::dedicated_thread` needs `std::jthread` and `std::stop_token`; `engine/src/core/jobs/CMakeLists.txt` checks at configure time. |
| .NET SDK | 8.0+ | Needed for scripting (`CONJURE_WITH_SCRIPTING=ON`, the default); `cmake/FindNetHost.cmake` locates `nethost.h` and `libnethost`. A machine without the host pack still configures: the root `CMakeLists.txt` turns scripting and the managed build off for that tree with a CMake warning (the configure summary shows `Scripting ... OFF`). Configure with `-DCONJURE_WITH_SCRIPTING=OFF` to make that explicit and silent. |
| Python | 3 | `tools/build.py`, `tools/check_style.py`, `tools/check_docs.py`, `tools/docs.py`, `tools/download_test_assets.py`. |

Platform packages, as installed by `.github/workflows/ci.yml`:

- Ubuntu 24.04: `sudo apt-get install -y ninja-build xorg-dev libwayland-dev libxkbcommon-dev libgl1-mesa-dev libasound2-dev libpulse-dev`
  (GLFW needs the X11 and Wayland headers; `-DCONJURE_PLATFORM_GLFW=OFF` yields a null-only platform library for servers).
- macOS 14: `brew install ninja`.
- Windows: Visual Studio with the "Desktop development with C++" workload. `tools/build.py` finds it through
  `vswhere.exe` and imports `vcvars64.bat` itself, so PowerShell, cmd and Git Bash all work.

## 2. Clone and first build

```bash
git clone <repository-url> ClaudeGameEngine
cd ClaudeGameEngine
python tools/build.py --test
```

`tools/build.py` picks the default preset for the host (`windows-msvc-debug`, `linux-gcc-debug`,
`macos-debug`), runs `cmake --preset <preset>`, `cmake --build --preset <preset>` and `ctest --preset <preset>`,
printing every command it runs with a leading `+`. The configure step ends with the summary printed by
`conjure_print_configuration_summary()` (build type, compiler, which subsystems and sanitizers are on).

Build products land in predictable places so executables find each other without an install step:

| Path | Content |
|---|---|
| `build/<preset>/bin/` | test executables, helper programs (`conjure_process_child`), later the editor, runtime, shaders and managed assemblies |
| `build/<preset>/lib/` | static libraries `conjure_core_<area>.lib` (MSVC) / `libconjure_core_<area>.a` (GCC, Clang) |
| `build/<preset>/generated/include/conjure/core/build_info.h` | generated from `engine/src/core/foundation/build_info.h.in` |
| `build/<preset>/_deps/` | FetchContent sources and builds of the pinned third-party libraries |
| `build/<preset>/compile_commands.json` | `CMAKE_EXPORT_COMPILE_COMMANDS` is on for every preset |

The first configure downloads every pinned dependency (`third_party/CMakeLists.txt`), which takes a few
minutes; later configures reuse `_deps` (`FETCHCONTENT_UPDATES_DISCONNECTED ON`).

> [!TIP]
> Useful variants of the one command:
>
> ```bash
> python tools/build.py --preset windows-msvc-dev --test                     # Development configuration
> python tools/build.py --target conjure_core_math_tests --test -R core_math  # one test module
> python tools/build.py --no-configure                                        # skip the configure step
> python tools/build.py --clean                                               # wipe build/<preset> first
> python tools/build.py --configure-only -- -DCONJURE_WITH_PHYSICS=OFF        # pass CMake cache variables
> python tools/build.py --test -L style                                       # only the style check
> python tools/build.py -j 8                                                  # cap parallel build jobs (0 = auto)
> ```

## 3. Presets

Configure presets live in `CMakePresets.json`. Every single-config configure preset has a build preset of
the same name; test presets exist for every single-config preset except `macos-dist`. The multi-config
`windows-vs` preset builds through `windows-vs-debug` / `windows-vs-dev` and tests through `windows-vs-debug`
only. Binary directories are `build/<presetName>`. In-source builds are rejected by the root `CMakeLists.txt`.

| Preset | Platform | Compiler | Configuration | Extras |
|---|---|---|---|---|
| `windows-msvc-debug` | Windows | `cl` (Ninja) | Debug | default on Windows |
| `windows-msvc-dev` | Windows | `cl` | Development | |
| `windows-msvc-dist` | Windows | `cl` | Distribution | asserts and TRACE/DEBUG logging compiled out |
| `windows-msvc-asan` | Windows | `cl` | Debug | `CONJURE_ENABLE_ASAN=ON` |
| `windows-vs` | Windows | Visual Studio 18 2026 | multi-config | build presets `windows-vs-debug`, `windows-vs-dev`; test preset `windows-vs-debug` only; see [IDE notes](#8-ide-notes) |
| `linux-gcc-debug` / `-dev` / `-dist` | Linux | `gcc` / `g++` | Debug / Development / Distribution | default on Linux |
| `linux-clang-debug` | Linux | `clang` / `clang++` | Debug | |
| `linux-clang-sanitize` | Linux | `clang++` | Debug | `CONJURE_ENABLE_ASAN=ON`, `CONJURE_ENABLE_UBSAN=ON` |
| `linux-clang-tsan` | Linux | `clang++` | Debug | `CONJURE_ENABLE_TSAN=ON`, `CONJURE_WITH_SCRIPTING=OFF`; nightly |
| `macos-debug` / `-dev` / `-dist` | macOS | `clang++`, deployment target 14.0 | Debug / Development / Distribution | default on macOS; `macos-dist` has no test preset (build only) |

Test presets inherit `base-test`: output on failure, four parallel jobs, `noTestsAction: error`, and the
label filter `exclude benchmark|golden|downloaded|slow|process`. The hidden `nightly-test` preset excludes
only `benchmark` (used by `windows-msvc-debug-nightly`, `linux-gcc-debug-nightly` and `linux-clang-tsan`,
which also restricts itself to the labels `core|ecs|scene|engine|integration`). Two workflow presets,
`windows-msvc-debug` and `linux-gcc-debug`, chain configure, build and test for `cmake --workflow`.

## 4. Configurations

`cmake/ConjureConfigs.cmake` is included before `project()` so the custom configurations exist for every
target, including fetched third-party ones. Single-config generators default to `Development` when no
`CMAKE_BUILD_TYPE` is given; every preset sets one explicitly.

| Configuration | Optimisation | Asserts | `CONJURE_LOG_TRACE/DEBUG` | Editor | Macro | Flags derived from |
|---|---|---|---|---|---|---|
| `Debug` | none, full debug info | on | compiled in | yes | `CONJURE_DEBUG=1` | CMake's `Debug` |
| `Development` | optimised + debug info | on (`CONJURE_ASSERT` and `CONJURE_VERIFY`) | compiled in | yes | `CONJURE_DEVELOPMENT=1` | `RelWithDebInfo` |
| `Distribution` | full | `CONJURE_ASSERT` compiles out, `CONJURE_VERIFY` still evaluates its expression | compiled out | no | `CONJURE_DISTRIBUTION=1` | `Release` |

The macros come from `conjure::platform_defines` (`cmake/ConjureWarnings.cmake`); `conjure/core/platform_detect.h`
turns them into `CONJURE_ASSERTS_ENABLED` and `CONJURE_DEV_TOOLS_ENABLED`, and `conjure/core/assert.h` and
`conjure/core/log.h` read those. With a single-config generator a `Distribution` build sets
`CONJURE_BUILD_EDITOR` to `OFF`. The multi-config `windows-vs` preset offers `Debug` and `Development` only: the
editor is never part of a Distribution build and a multi-config tree builds every configuration from one set of
targets, so `cmake/ConjureConfigs.cmake` adds `Distribution` to such a tree only when it is configured with
`-DCONJURE_BUILD_EDITOR=OFF`. Build what ships with the single-config `*-dist` presets.

> [!IMPORTANT]
> Run the Distribution test preset before claiming a change is done when it touches assertions or
> logging: `CONJURE_CHECK_ASSERTS` compiles to a no-op there, and a test that relied on an assert or a
> TRACE message to fire will behave differently. `python tools/build.py --preset windows-msvc-dist --test`.

## 5. Running tests

Every doctest `TEST_CASE` is one CTest entry named `<module>.<case>` (for example
`core_math.transform - to_matrix equals translate * rotate * scale`), labelled `unit;<module>;...`
(`cmake/ConjureTesting.cmake`). So `ctest -R` filters by module or by case text and `ctest -L` by label:

```bash
python tools/build.py --test -R core_foundation                     # one test executable's cases
python tools/build.py --test -R "slot_map - erase"                  # cases by name fragment
python tools/build.py --test -L style                               # the style check only
ctest --preset windows-msvc-debug -N                                # list entries without running
```

Running an executable directly gives doctest's detailed report and the log captured during a failing case:

```bash
build/windows-msvc-debug/bin/conjure_core_foundation_tests.exe -ts="core/result"   # one suite
build/windows-msvc-debug/bin/conjure_core_foundation_tests.exe -tc="uuid - *"      # cases by pattern
build/windows-msvc-debug/bin/conjure_core_foundation_tests.exe -s                  # show successful asserts
build/windows-msvc-debug/bin/conjure_core_jobs_tests.exe --conjure-threads=0 --conjure-seed=0x1234
```

Every run prints `seed=<n> threads=<n>` to stderr first, except for doctest's listing and query options
(`--list-test-cases`, `--list-test-suites`, `--count`, `--version`, `--help`, ...), which print nothing but
their result so machine-readable listings stay clean. Passing tests print nothing else because the
console log sink is replaced by a ring buffer that is dumped only when a case fails. An unmet machine
precondition exits with code 77 and CTest reports the entry as *Not Run*. The [Testing](testing.md)
chapter lists every `--conjure-*` flag, environment variable and precondition, and how to read failures.

## 6. Private build directories

Builds in the same directory must not overlap. When another agent or a second shell may be building, pass
a private directory; the configure preset still supplies the generator and compiler:

```bash
python tools/build.py --build-dir build/core_math --target conjure_core_math_tests --test -R core_math
python tools/build.py --build-dir build/core_math --no-configure --target conjure_core_math_tests --test -R core_math
```

With `--build-dir`, `tools/build.py` runs `cmake --preset <preset> -B <dir>`, `cmake --build <dir>` and
`ctest --test-dir <dir> --output-on-failure` instead of the preset forms. Everything under `build/` is
ignored by git. `--clean` removes the directory first, clearing the read-only bit git leaves on fetched
pack files on Windows.

## 7. Build options

Options are declared in the root `CMakeLists.txt` (`CONJURE_ENABLE_TSAN` in `cmake/ConjureSanitizers.cmake`,
`CONJURE_TEST_GOLDEN_IMAGES` in `cmake/ConjureTesting.cmake`) and passed through `tools/build.py` after `--`:

| Option | Default | Effect |
|---|---|---|
| `CONJURE_WITH_RENDERER` / `_PHYSICS` / `_AUDIO` / `_SCRIPTING` | ON | build the subsystem and fetch its dependency (NVRHI, Jolt, miniaudio, nethost); a disabled subsystem also disables its tests |
| `CONJURE_BUILD_EDITOR` / `_RUNTIME` / `_TESTS` / `_BENCHMARKS` / `_SAMPLES` / `_MANAGED` | ON | build the executables, tests, benchmarks, samples and C# assemblies (the `samples/` directory skips itself until wave 5 adds its sources) |
| `CONJURE_RHI_D3D12` | ON on Windows, forced OFF elsewhere | D3D12 backend; the renderer needs at least one of D3D12 / Vulkan |
| `CONJURE_RHI_VULKAN` | OFF on Windows, ON elsewhere | Vulkan backend, fetches glslang |
| `CONJURE_PLATFORM_GLFW` | ON | GLFW window backend; required by `CONJURE_BUILD_EDITOR` |
| `CONJURE_WARNINGS_AS_ERRORS` | ON | `/WX` or `-Werror` on every Conjure target |
| `CONJURE_ENABLE_ASAN` / `_UBSAN` / `_TSAN` | OFF | sanitizers; TSan is exclusive with ASan and needs GCC or Clang |
| `CONJURE_TEST_GOLDEN_IMAGES` | OFF | gates the golden-image tests of later waves |

## 8. IDE notes

- **Visual Studio.** `cmake --preset windows-vs` writes `build/windows-vs/Conjure.sln` with the `Debug` and
  `Development` configurations (`Distribution` appears only with `-DCONJURE_BUILD_EDITOR=OFF`, section 5).
  Targets are grouped by the `FOLDER` property (`Engine`, `Engine/core`, `ThirdParty`,
  `Tests`, `Tests/HeaderChecks`, `Executables`) and sources by `source_group(TREE ...)`. Opening the
  repository folder directly also works because Visual Studio reads `CMakePresets.json`.
- **VS Code / CLion.** Both read `CMakePresets.json`; pick a configure preset and the matching build and
  test presets. `compile_commands.json` is generated in `build/<preset>` for clangd.
- **Multi-config caveat.** doctest 2.4.11 discovers tests in a post-build step and writes one
  configuration-independent list per target, so after building `Development` on top of `Debug` in
  `build/windows-vs`, `ctest -C Debug` runs the Development binaries. Single-config presets are unaffected.
- **Formatting.** `.clang-format` approximates `docs/CODE_STYLE.cpp` and is a checker, not an
  auto-formatter: the style guide wins where they disagree (notably the two-space continuation indent).
  `.editorconfig` sets UTF-8, LF, four spaces and 120 columns; `tools/check_style.py` enforces the rest.

## 9. Documentation toolchain

The documentation is Markdown under `docs/`, published as one MkDocs Material site (`mkdocs.yml`), and
checked structurally by `tools/check_docs.py`. `tools/docs.py` wraps everything:

```bash
python tools/docs.py install      # creates .venv-docs and installs tools/docs_requirements.txt
python tools/docs.py serve        # live-reloading preview at http://127.0.0.1:8000
python tools/docs.py build        # mkdocs build --strict into site/ (broken links fail)
python tools/docs.py check        # tools/check_docs.py: badges, links, page length, coverage
```

`tools/docs_requirements.txt` pins `mkdocs-material==9.6.14` and `mkdocs-github-admonitions-plugin==0.0.3`.
`.venv-docs/` and `site/` are ignored by git. `check` needs no virtual environment; it runs
`python -I tools/check_docs.py` and reports one line per problem: a page without its badge on the first
content line after the title, a relative link whose target does not exist, a page over 400 lines, a
to-do, fix-me or triple-X marker (the exact words are the tuple in `check_page()` of `tools/check_docs.py`), a
public header under `engine/include/conjure/<module>/` that no wiki page of that module references by
include path, or an engine module without `docs/handbook/modules/<module>.md`.

## Related

- design record: `docs/design/08_testing.md` (presets, labels, skip discipline: sections 3.2, 3.8, 7.3)
- design record: `docs/design/16_documentation.md` (documentation toolchain, section 4)
- `docs/ARCHITECTURE.md` section 9 (build and configuration matrix)
- `.claude/skills/build-and-test/SKILL.md` (the same commands as an agent skill)
- [Build pipeline](build-pipeline.md), [Testing](testing.md), [Continuous integration](ci.md)

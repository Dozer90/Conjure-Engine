# Getting started

🛠 **Engineering handbook**

This chapter takes a fresh machine to a green `ctest` run: what to install, how `tools/build.py` drives
CMake, which presets and configurations exist, how to run and filter tests, how to keep a private build
directory when several builds run at once, how the repository behaves in an IDE, how to build the
documentation site and how to start the editor and the runtime you built.

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
- [Running the editor and the runtime](#10-running-the-editor-and-the-runtime)
- [Licence and third-party notices](#11-licence-and-third-party-notices)

## 1. Prerequisites

| Tool | Version | Notes |
|---|---|---|
| CMake | 3.28+ | `CMakePresets.json` is schema version 8. The `windows-vs` preset generates for "Visual Studio 18 2026" and needs the CMake bundled with VS 2026 or CMake 4.1+. |
| Ninja | any recent | Used by every preset except `windows-vs`. On Windows `tools/build.py` adds the Ninja and CMake bundled with Visual Studio to `PATH` when the shell has none. |
| C++20 compiler | MSVC 19.40+ (VS 2022 17.10 or VS 2026), GCC 13+, Clang 16+ | Warnings are errors on every compiler (`cmake/ConjureWarnings.cmake`). |
| C++ standard library | libstdc++ 10+, MSVC 19.28+, libc++ 18+ (Xcode 16+) | `core::jobs::dedicated_thread` needs `std::jthread` and `std::stop_token`; `engine/src/core/jobs/CMakeLists.txt` checks at configure time. |
| .NET SDK | 8.0+ | Needed for scripting (`CONJURE_WITH_SCRIPTING=ON`, the default); `cmake/FindNetHost.cmake` locates `nethost.h` and `libnethost`. A machine without the host pack still configures: the root `CMakeLists.txt` turns scripting and the managed build off for that tree with a CMake warning (the configure summary shows `Scripting ... OFF`). Configure with `-DCONJURE_WITH_SCRIPTING=OFF` to make that explicit and silent. |
| Python | 3 | `tools/build.py`, `tools/prune_builds.py`, `tools/check_style.py`, `tools/check_docs.py`, `tools/docs.py`, `tools/download_test_assets.py`. |

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
| `build/<preset>/bin/` | `conjure_editor`, `conjure_runtime`, `conjure_shared`, the test executables and helper programs (`conjure_process_child`); `managed/` holds the C# assemblies; `THIRD_PARTY_NOTICES.txt` holds the licence notices of what the tree links (section 11); a `windows-msvc-dist` tree also holds the Visual C++ runtime DLLs (`vcruntime140*.dll`, `msvcp140*.dll`) that an export ships |
| `build/<preset>/bin/shaders/` | compiled shaders and `index.json` (target `conjure_shaders`), next to the executables that open them |
| `build/<preset>/lib/` | the static library of every engine module and core area (`conjure_<module>.lib`, `conjure_core_<area>.lib` on MSVC; `lib*.a` on GCC and Clang) |
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

A private directory is temporary. A full Debug tree is 11 to 13 GB, so reuse its name across rounds of work
and remove it once no further round will use it; `tools/prune_builds.py` lists and removes build trees:

```bash
python tools/prune_builds.py                          # every build tree: size, last change, action
python tools/prune_builds.py core_math --delete       # remove named trees (core_math or build/core_math)
python tools/prune_builds.py --older-than 3 --delete  # remove trees unchanged for three days
python tools/prune_builds.py --all --keep core_math --delete
```

It only touches directories directly under `build/` that hold a `CMakeCache.txt` and no `CMakeLists.txt`, and
whose recorded source directory exists on this host outside the tree. The comparison is by file identity, so an
in-source scratch project stays safe however its path is spelled (a junction, a symbolic link, a `subst`
drive), and a tree configured from WSL is listed only in WSL, a tree configured on Windows only on Windows. It
never follows a link or junction, and it keeps the presets' binary directories (`build/<preset>`, including
those of `CMakeUserPresets.json` and of included preset files, with inheritance and the macros resolved as CMake
does; a macro it cannot know, such as `$vendor{}`, stands for any text in its place, so `$vendor{x}-dev`
protects every name ending in `-dev`) unless `--include-presets` is given. Without `--delete` it only reports.
A tree's last change is the newest modification of any file or directory in it, whatever the generator.

To remove a tree it first renames it to a private name next to it (`NAME.prune-<process id>`) and checks that
what it renamed is still the directory it listed, so another agent that recreates or relinks `build/NAME`
meanwhile cannot redirect the removal; whenever the removal stops, the tree is renamed back. On Windows removal
works through extended-length paths, so files past 260 characters go too, and deletes read-only files (git
leaves fetched pack files read-only) without clearing the read-only bit their other hard links share. It leaves
a tree untouched while a file in it is held open, a process works in it, or a program or library runs from it.
On Linux and macOS it makes read-only, unreadable and unsearchable directories inside the tree accessible,
refuses to start on a tree when `build/` itself is not writable, and keeps a tree that is or holds a mount
point; it cannot tell that a tree is in use there, so run `--older-than` or `--all` only when no one else is
building. `CMakeCache.txt` goes last: a tree that could not be removed completely stays listed, and running the
command again retries it.

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
| `CONJURE_PHYSICS_CROSS_PLATFORM_DETERMINISTIC` | OFF | build Jolt with `CROSS_PLATFORM_DETERMINISTIC` (bit-identical physics across platforms; slower) |
| `CONJURE_TEST_GOLDEN_IMAGES` | OFF | gates the golden-image tests of later waves |

## 8. IDE notes

- **Visual Studio.** `cmake --preset windows-vs` writes `build/windows-vs/Conjure.sln` with the `Debug` and
  `Development` configurations (`Distribution` appears only with `-DCONJURE_BUILD_EDITOR=OFF`, section 4).
  Targets are grouped by the `FOLDER` property (`Engine`, `Engine/core`, `ThirdParty`,
  `Tests`, `Tests/HeaderChecks`, `Executables`) and sources by `source_group(TREE ...)`. Opening the
  repository folder directly also works because Visual Studio reads `CMakePresets.json`.
- **VS Code / CLion.** Both read `CMakePresets.json`; pick a configure preset and the matching build and
  test presets. `compile_commands.json` is generated in `build/<preset>` for clangd.
- **Multi-config caveat.** doctest 2.4.11 discovers tests in a post-build step and writes one
  configuration-independent list per target, so after building `Development` on top of `Debug` in
  `build/windows-vs`, `ctest -C Debug` runs the Development binaries. Single-config presets are unaffected.
- **Formatting.** `tools/check_style.py` is the only C++ style gate. `.clang-format` approximates
  `docs/CODE_STYLE.cpp` and is an optional local aid that no CI step runs: the style guide wins where they
  disagree (notably the two-space continuation indent), so do not reformat whole files with it.
  `.editorconfig` sets UTF-8, LF, four spaces and 120 columns, and its `[*.cs]` section holds the C# layout
  rules that CI enforces: `dotnet format --verify-no-changes managed/<project>/<project>.csproj` must pass for
  `Conjure.Managed`, `Conjure.Bootstrap` and `Conjure.TestScripts`. CI judges with the formatter of the 8.0 SDK
  its `actions/setup-dotnet` step installs, which the step selects through a `global.json` it writes into the
  checkout; a local `dotnet` runs the newest SDK installed, which may format differently
  ([Continuous integration](ci.md#2-the-matrix)).

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

## 10. Running the editor and the runtime

Both executables land in `build/<preset>/bin/` (with `.exe` on Windows). The editor exists in Debug and
Development builds (`CONJURE_BUILD_EDITOR`); the runtime is built in every configuration and is the only one a
Distribution build ships.

```bash
cp -r tests/assets/projects/minimal build/minimal_project      # a scratch copy, see the warning below
build/<preset>/bin/conjure_editor --project build/minimal_project
build/<preset>/bin/conjure_editor --project build/minimal_project --trust-project --headless \
    --exec <commands.json> --out <replies.ndjson>
build/<preset>/bin/conjure_editor --help                       # every option and the exit codes
build/<preset>/bin/conjure_runtime --project build/minimal_project
build/<preset>/bin/conjure_runtime --project build/minimal_project --headless --frames 60
```

A windowed start opens `shaders/index.json` next to the executable, which is where `conjure_shaders` builds it
(`build/<preset>/bin/shaders/`), so both executables run from a fresh single-configuration build tree as they are.
The multi-configuration `windows-vs` tree is the exception: its executables land in `bin/<Config>/` and do not find
the shaders (code sweep 2026-11-12, F329).

The first editor line opens a window over the project; the second runs the requests of a
JSON array or NDJSON file without a window and exits with 0 when every command succeeded and 1 when one failed
(`--keep-going` continues past a failure), writing the responses and events to the `--out` file instead of standard
output. The editor opens a project only after somebody answered for it: the window asks once whether you trust
the copied project, and the headless run answers with `--trust-project` (without it, and without an earlier answer
in the window, the run exits with 2). The runtime runs the same project as a game with no editor code;
`--headless --frames 60` is the smoke test without a window. A headless run executes no C#; add `--scripting` to
smoke-test a scripted game with its scripts running (06 D12). Driving the editor as an agent is the
`editor-ai-control` skill (`.claude/skills/editor-ai-control/SKILL.md`).

> [!WARNING]
> Open a copy of a fixture project, never `tests/assets/projects/` itself: the asset database writes `.meta`
> sidecars and its cache next to the project's sources, and the `.meta` files are deliberately not ignored by
> git.

Every option and what each executable needs next to it: 📘 API: [The editor](../api/editor/index.md),
📘 API: [The runtime](../api/runtime/index.md).

## 11. Licence and third-party notices

Conjure is licensed under the Apache License, Version 2.0 (design record `06_engine_loop.md` D15). Four files carry
that and the licences of the software it builds on:

| File | Content | Written by |
|---|---|---|
| `LICENSE`, `NOTICE` | the Apache licence text, unchanged; the copyright holders, "the Conjure contributors" | tracked, not generated |
| `THIRD_PARTY_NOTICES.md` | every third-party dependency of every platform with its pin and licence, and the licence text of each pinned one | `tools/third_party_notices.py`, tracked |
| `build/<tree>/bin/THIRD_PARTY_NOTICES.txt` | `NOTICE`, `LICENSE` and the licence text of what this tree links into `conjure_shared` and `conjure_runtime` | the configure writes it, building `conjure_runtime` places it; an export that copies the runtime copies it into the game |

A dependency is registered once, beside its pin in `third_party/CMakeLists.txt`, with
`conjure_third_party_notice(<id> NAME ... VERSION <the GIT_TAG> LICENSE_ID ... SCOPE RUNTIME|EDITOR|BUILD FETCH <name>
LICENSE_FILES <files in the fetched source>)` (`cmake/ConjureNotices.cmake` documents every argument). The
configure fails when `VERSION` is not the `GIT_TAG`, when a licence file of software the tree uses is missing, and
when a `FetchContent_Declare` of that file has no notice naming it as `FETCH`. Software that is not fetched (a file
tracked under someone else's licence, code that follows a published implementation), and code a fetched source
embeds under a copyright of its own (jsmn in `cgltf.h`), keep their licence text in `third_party/licenses/`; a
notice with `FETCH` names such a file with an absolute path, since its relative ones are read in the fetched source.
libjxl and the three libraries it builds are pinned by archive SHA-256 in `third_party/jxl.cmake`, which
`third_party/CMakeLists.txt` includes; neither the `GIT_TAG` comparison nor the declaration scan reaches that file,
and its own probes check its four notices ([libjxl](libjxl.md#7-licences)).

After changing a pin or a notice, regenerate the tracked document and commit it with the change:

```bash
python tools/third_party_notices.py --build-dir build/<tree>            # write THIRD_PARTY_NOTICES.md
python tools/third_party_notices.py --build-dir build/<tree> --check    # what the CTest entry notices.up_to_date runs
```

The tool rewrites the sections of the software the tree uses and keeps the others, so any tree can bring its own
sections up to date; it stops when a kept section was written for another pin or licence than the tree registers,
and then a tree that fetches that software has to run it. A Windows tree
configured with `-DCONJURE_RHI_VULKAN=ON` (`python tools/build.py --build-dir build/<name> --configure-only --
-DCONJURE_RHI_VULKAN=ON`) fetches every pinned dependency at once, the Direct3D 12 headers included, which no
Linux or macOS tree has. `notices.tool` is the self-test of the tool.

> [!NOTE]
> .NET is found on the build machine instead of pinned, so its licence text is in no tracked file: each build
> copies `LICENSE.txt` and `ThirdPartyNotices.txt` of the .NET installation it links into its
> `THIRD_PARTY_NOTICES.txt`, and the configure warns when it finds neither.

## Related

- design record: `docs/design/08_testing.md` (presets, labels, skip discipline: sections 3.2, 3.8, 7.3)
- design record: `docs/design/16_documentation.md` (documentation toolchain, section 4)
- `docs/ARCHITECTURE.md` section 9 (build and configuration matrix)
- `.claude/skills/build-and-test/SKILL.md` (the same commands as an agent skill)
- [Build pipeline](build-pipeline.md), [Testing](testing.md), [Continuous integration](ci.md)

# Setup and First Run
<!-- doc-template:chapter -->
[Handbook](../index.md) → [Getting Started](index.md)

<!-- doc-content:purpose:start -->
<a id="getting-started"></a>

Install the prerequisites, make the first build, run the tests, and start the editor or runtime. The Development section contains detailed presets, configurations, options, and build-tree maintenance.
<!-- doc-content:purpose:end -->

<!-- doc-content:body:start -->
<a id="1-prerequisites"></a>

## Prerequisites

| Tool | Version | Notes |
|---|---|---|
| CMake | 3.28+ | `CMakePresets.json` is schema version 8. The `windows-vs` preset generates for "Visual Studio 18 2026" and needs the CMake bundled with VS 2026 or CMake 4.1+. |
| Ninja | any recent | Used by every preset except `windows-vs`. On Windows `tools/build.py` adds the Ninja and CMake bundled with Visual Studio to `PATH` when the shell has none. |
| C++20 compiler | MSVC 19.40+ (VS 2022 17.10 or VS 2026), GCC 13+, Clang 16+ | Warnings are errors on every compiler (`cmake/ConjureWarnings.cmake`). |
| C++ standard library | libstdc++ 10+, MSVC 19.28+, libc++ 18+ (Xcode 16+) | `core::jobs::dedicated_thread` needs `std::jthread` and `std::stop_token`; `engine/src/core/jobs/CMakeLists.txt` checks at configure time. |
|.NET SDK | 8.0+ | Needed for scripting (`CONJURE_WITH_SCRIPTING=ON`, the default); `cmake/FindNetHost.cmake` locates `nethost.h` and `libnethost`. A machine without the host pack still configures: the root `CMakeLists.txt` turns scripting and the managed build off for that tree with a CMake warning (the configure summary shows `Scripting ... OFF`). Configure with `-DCONJURE_WITH_SCRIPTING=OFF` to make that explicit and silent. |
| Python | 3 | `tools/build.py`, `tools/prune_builds.py`, `tools/check_style.py`, `tools/check_docs.py`, `tools/docs.py`, `tools/download_test_assets.py`. |

Platform packages, as installed by `.github/workflows/ci.yml`:

- Ubuntu 24.04: `sudo apt-get install -y ninja-build xorg-dev libwayland-dev libxkbcommon-dev libgl1-mesa-dev libasound2-dev libpulse-dev`
  (GLFW needs the X11 and Wayland headers; `-DCONJURE_PLATFORM_GLFW=OFF` yields a null-only platform library for servers).
- macOS 14: `brew install ninja`.
- Windows: Visual Studio with the "Desktop development with C++" workload. `tools/build.py` finds it through
  `vswhere.exe` and imports `vcvars64.bat` itself, so PowerShell, cmd and Git Bash all work.

<a id="2-clone-and-first-build"></a>

## Clone and first build

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
> python tools/build.py --preset windows-msvc-dev --test # Development configuration
> python tools/build.py --target conjure_core_math_tests --test -R core_math # one test module
> python tools/build.py --no-configure # skip the configure step
> python tools/build.py --clean # wipe build/<preset> first
> python tools/build.py --configure-only -- -DCONJURE_WITH_PHYSICS=OFF # pass CMake cache variables
> python tools/build.py --test -L style # only the style check
> python tools/build.py -j 8 # cap parallel build jobs (0 = auto)
> ```

<a id="5-running-tests"></a>

## Running tests

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
(`--list-test-cases`, `--list-test-suites`, `--count`, `--version`, `--help`,...), which print nothing but
their result so machine-readable listings stay clean. Passing tests print nothing else because the
console log sink is replaced by a ring buffer that is dumped only when a case fails. An unmet machine
precondition exits with code 77 and CTest reports the entry as *Not Run*. The [Testing and Benchmarks](../development/testing-and-benchmarks.md)
chapter lists every `--conjure-*` flag, environment variable and precondition, and how to read failures.

<a id="8-ide-notes"></a>

## IDE notes

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
  ([Continuous Integration](../development/continuous-integration.md#2-the-matrix)).

<a id="10-running-the-editor-and-the-runtime"></a>

## Running the editor and the runtime

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
the shaders.

The first editor line opens a window over the project; the second runs the requests of a
JSON array or NDJSON file without a window and exits with 0 when every command succeeded and 1 when one failed
(`--keep-going` continues past a failure), writing the responses and events to the `--out` file instead of standard
output. The editor opens a project only after somebody answered for it: the window asks once whether you trust
the copied project, and the headless run answers with `--trust-project` (without it, and without an earlier answer
in the window, the run exits with 2). The runtime runs the same project as a game with no editor code;
`--headless --frames 60` is the smoke test without a window. A headless run executes no C#; add `--scripting` to
smoke-test a scripted game with its scripts running. See the [AI control protocol](../../api/editor/ai-protocol.md) for command arguments and responses.

> [!WARNING]
> Open a copy of a fixture project, never `tests/assets/projects/` itself: the asset database writes `.meta`
> sidecars and its cache next to the project's sources, and the `.meta` files are deliberately not ignored by
> git.

Every option and what each executable needs next to it: 📘 API: [The editor](../../api/editor/index.md),
📘 API: [The runtime](../../api/runtime/index.md).

<a id="contents"></a>
<a id="related"></a>
<!-- doc-content:body:end -->

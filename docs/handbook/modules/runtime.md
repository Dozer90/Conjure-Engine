# Runtime

🛠 **Engineering handbook** · 📘 API: [The runtime](../../api/runtime/index.md)

`conjure_runtime` (`runtime/src/main.cpp`, `runtime/src/runtime_platform.h`, `runtime/CMakeLists.txt`) is the
standalone game executable of wave 4: one thin `main` over `engine::application` that maps outcomes to exit codes,
makes sure the message behind an outcome reaches someone, and links `conjure::shared` and nothing else from the
engine (R17). Every decision about configuration, startup, the frame and shutdown is the engine's (🛠 Handbook:
[Engine module](engine.md)); this chapter covers what the executable adds, its build, the Distribution stripping
check and its tests. The design is `06_engine_loop.md` 4.2 and 2.12, `ARCHITECTURE.md` 8 and 11, `04_editor.md`
8; `runtime/README.md` carries the finer notes.

## Contents

- [1. What main.cpp does](#1-what-maincpp-does)
- [2. Exit codes](#2-exit-codes)
- [3. Diagnostics without a console](#3-diagnostics-without-a-console)
- [4. Build and linkage](#4-build-and-linkage)
- [5. Distribution stripping check](#5-distribution-stripping-check)
- [6. Tests](#6-tests)
- [7. Deviations from the design](#7-deviations-from-the-design)
- [Related](#related)

## 1. What main.cpp does

1. `runtime::utf8_arguments(argc, argv)` (the arguments as UTF-8; on Windows re-read from the wide command line
   with `GetCommandLineW` + `CommandLineToArgvW` and converted with `core::str::from_utf16`), then
   `engine::parse_command_line`. `--help` prints the runtime's usage line plus `engine::command_line_help()` on
   standard output and exits 0 before anything is loaded; when nothing reads standard output (the console-less
   Distribution build, double-clicked or started from a terminal) it also shows that text in an information box.
2. Refuses the unknown options the engine left for the executable (the runtime consumes none; the editor owns
   `--exec` / `--export`).
3. Without `--project` it runs the `project.json` next to the executable (`runtime::executable_directory()`, the
   double-click launch of an exported folder); only when there is none does it exit 3 with the usage text.
4. In Distribution, without `--log-file`, the log file defaults to `conjure_runtime.log` in the project directory
   (when it exists) or next to the executable.
5. `application_desc::from_options(options, engine_config::defaults())`: the whole merge of defaults < `--headless`
   < `project.json` < flags, validated once (06 D9). A project error exits 2; an `INVALID_CONFIG` exits 2 when the
   project's own settings fail the same validation without any flag (the merge of `--project` and the `--headless`
   mode alone), else 3 with the help hint. `--headless` leaves scripting off whatever the project says and
   `--scripting`, one of the flags, turns it on again (06 D12): the runtime adds nothing to that rule.
6. `application app(desc)`; `add_subsystem(bindings::scripting_bindings_subsystem)` when the bindings module is
   built (`CONJURE_RUNTIME_WITH_BINDINGS`, startup step 16); `layers().push_layer(game_layer)` (quits on the
   window's close event).
7. `initialise()`; on failure the engine has logged the step and the runtime exits 1 with its summary.
8. `return app.run()`; a non-zero result is named through the same channel before the code is returned.

Messages the runtime prints itself go to standard error as `conjure_runtime: <message>`; everything else is the
engine's console log. Section 3 says where they go when there is no console.

## 2. Exit codes

| Code | Name | Cause |
|---|---|---|
| 0 | `OK` | `run()` finished (`--frames` reached, window closed, `--help`) |
| 1 | `INIT_FAILURE` | `application::initialise()` failed (startup unwound; the error names the step) |
| 2 | `RUNTIME_FAILURE` | a frame failed, or `project.json` is missing, malformed, invalid or too new (`ARCHITECTURE.md` 11), or the project's own settings do not validate |
| 3 | `BAD_ARGUMENTS` | an unknown flag, a flag without its value, a bad value, a merged configuration a flag made invalid, no `--project` and no `project.json` next to the executable |
| 4 | `DEVICE_LOST` | the graphics device was lost during a frame (no recovery in v1) |

## 3. Diagnostics without a console

`WIN32_GUI` hides the console in Distribution, so a player who double-clicks an exported game would otherwise see
an exit code and nothing else. `main.cpp` therefore routes every runtime message through three channels, and the
`--help` text through a fourth:

| Channel | When | What |
|---|---|---|
| standard error | always | the plain `conjure_runtime: <message>` line (what scripts, CI and the tests read) |
| the log file | `--log-file`, or in Distribution the default `<project dir>/conjure_runtime.log` (else `<executable dir>/conjure_runtime.log`) | the same line logged through `core::log` (category `runtime`, level ERROR). The engine's `file_log_sink` lives only between startup step 1 and `stop_core`, so the runtime opens its own sink in append mode for the line; a failed start leaves the engine's lines of the failed step followed by the runtime's summary, and a failure before the application exists (a missing `project.json`, a bad flag) still leaves a file naming it |
| a message box | `runtime::has_standard_error()` is false: no console, no pipe, no redirection (the console-less Distribution build on Windows, double-clicked or started from a terminal) | `MessageBoxW` with the message and the log file's path; a process spawned by `core::process` (a pipe), a redirected one, or a Debug/Development build has a standard error and never sees it |
| an information box (`--help` only) | `runtime::has_standard_output()` is false: the Distribution build on Windows, double-clicked or started from a terminal (a GUI-subsystem process cannot write to the terminal's console) | `runtime::show_info_dialog` shows the usage text with `MessageBoxW` and `MB_ICONINFORMATION`; a piped `--help` (the tests, scripts) never opens it |

The platform-specific pieces (`utf8_arguments`, `executable_directory`, `has_standard_error`,
`has_standard_output`, `show_error_dialog`, `show_info_dialog`) live behind `runtime/src/runtime_platform.h`, with
`runtime_platform_windows.cpp` and `runtime_platform_posix.cpp` selected by `runtime/CMakeLists.txt` the way
`core/system` selects its backends (`CONVENTIONS.md` 3: no platform `#if` outside backend files). The POSIX
backend probes both streams with `fcntl` and has no dialogs.

On Windows the executable also embeds `runtime/src/conjure_runtime.manifest`, whose `activeCodePage` makes UTF-8
the process code page (Windows 10 1903+): the engine turns the UTF-8 arguments into `std::filesystem::path`
through the narrow conversion, which uses that code page, so a project directory named in characters outside the
legacy code page loads instead of failing with `PROJECT_NOT_FOUND`.

## 4. Build and linkage

`conjure_add_executable(conjure_runtime WIN32_GUI SOURCES src/main.cpp src/runtime_platform.h <backend> DEPS
conjure::shared ...)`: the umbrella shared library and nothing else, so neither the editor libraries nor Dear
ImGui can reach it except through `conjure_shared`, and in Distribution the editor is off and ImGui is not
fetched. The Windows backend adds `user32` (`MessageBoxW`), `shell32` (`CommandLineToArgvW`) and the manifest.
`WIN32_GUI` hides the console window in Distribution only. The run path is the executable's own directory
(`$ORIGIN` / `@loader_path`) because `export_pipeline` step 5 places `conjure_shared` next to it. The target is
skipped when `conjure::shared` or `conjure::engine` is missing.

An MSVC Distribution build also places the Visual C++ runtime next to the executable (06 D14): a `POST_BUILD`
step of `conjure_runtime` copies every `vcruntime140*.dll` and `msvcp140*.dll` of the toolchain's redistributable
folder into `bin/`, where export step 5 picks them up, so an exported game does not need the Microsoft Visual
C++ Redistributable. `conjure_msvc_find_crt_libraries()` (`runtime/cmake/ConjureMsvcRuntime.cmake`, probed on every
platform by the CTest entry `runtime.msvc_runtime_folder_rules`) asks `InstallRequiredSystemLibraries` for the
folder and accepts it only when its version (`.../Redist/MSVC/<major>.<minor>.<build>/<arch>/Microsoft.VC<n>.CRT`)
is not lower than that of the folder under `VCToolsRedistDir`, which `vcvars` and `tools/build.py` set, or than
the compiler's toolset (`14.<minor>` for `MSVC_VERSION` `19<minor>`) without one: CMake 3.31 answers a v145
compiler with the VC143 folder of a Visual Studio 2022 installed beside it, and an older library next to the
executable would be loaded in preference to the installed one. The fallback is the `VCToolsRedistDir` folder,
taken without comparing it with the toolset (Visual Studio versions the redistributable apart from the tools, so
its minor can be lower) as long as its path shows a version. When no folder passes, the configure prints a CMake
warning, nothing is copied and exports from that build warn in turn. Debug and Development copy nothing, the
Universal CRT is left to Windows, and `concrt140.dll` and `vccorlib140.dll` are not copied (nothing in the engine
imports them). The step runs when `conjure_runtime` links, so a library deleted from `bin/` by hand comes back
with the next link of the executable, not with the next build. The target property `CONJURE_MSVC_CRT_LIBRARIES`
lists the files for `tests/runtime`.

Every build, in every configuration, places `THIRD_PARTY_NOTICES.txt` next to the executable (06 D15):
`conjure_ship_third_party_notices(conjure_runtime)` declares the copy as a build output the target depends on, so
the file is there after `conjure_runtime` was built and returns with the next build when it was deleted. Its
content is written at configure time by `conjure_write_third_party_notices()` from the notices registered in
`third_party/CMakeLists.txt`: Conjure's `NOTICE` and `LICENSE`, then every `RUNTIME` entry the tree uses and, in a
tree that builds the editor, the `EDITOR` entries, because that tree's `conjure_shared` holds Dear ImGui and
ImGuizmo. A Distribution tree names neither. Export step 5 copies the file with the runtime (🛠 Handbook:
[Getting started](../getting-started.md) section 11 for the tracked `THIRD_PARTY_NOTICES.md` and how to regenerate it).

## 5. Distribution stripping check

`tools/check_distribution_symbols.py` lists the symbols of the Distribution `conjure_runtime` (symbol table and
imports) and of the `conjure_shared` library next to it (exports and symbol table) with `dumpbin /SYMBOLS
/IMPORTS /EXPORTS` or `llvm-nm` / `nm`, and fails when any names `conjure::editor`, a `conjure_editor*` binary,
the `ImGui` / `ImGuizmo` namespaces or an ImGui type. The `conjure::editor` rules match the fully spelled
namespace and the compressed forms of a parameter or return type inside a non-editor symbol: MSVC's
back-reference (`...editor_layer@editor@3@...`) and Itanium's substitution (`...NS_6editor12editor_layerE`); the
self-test fixtures carry real MSVC and clang manglings of both, and the self-test prints the number of
expectations it ran. The engine's own `imgui_layer` / `set_imgui_draw_data` are not findings. Exit codes: 0
clean, 1 findings, 2 usage or tool failure, 77 when the directory is not a Distribution build.
`tests/runtime/CMakeLists.txt` registers `runtime.check_distribution_symbols_tool` (the self-test, always) and
`runtime.distribution_symbols` (label `process`) when the build directory is a Distribution one, a multi-config
build, or `CONJURE_DISTRIBUTION_BUILD_DIR` names a built Distribution directory. The nightly `distribution-export`
job runs it after building `windows-msvc-dist` and `linux-gcc-dist`.

```
python tools/check_distribution_symbols.py --build-dir build/windows-msvc-dist
```

## 6. Tests

`tests/runtime/` (`conjure_runtime_tests`, labels `unit;runtime;process`) spawns the built executable through
`core::process`; `runtime_test_support.{h,cpp}` generates a tiny project (one authored startup scene, one data
blob) into a `temp_directory` and exports it with `engine::export_pipeline`, and a scripted variant whose scene
carries one `Conjure.TestScripts.DestroyProbe` (authored through the script runtime on the null host, so it needs
the scripting module but no .NET). Every case spawns a process, so the
module carries the `process` label of 08_testing.md 6.3 item 8: the default presets skip it and CI runs
`-L "downloaded|process"` in its own step (🛠 Handbook: [Continuous integration](../ci.md)). The files follow
08_testing.md 5.2:

- `test_runtime_boot.cpp`: the exported folder boots headless for 60 frames with exit 0, with the build's runtime
  and with the copy the pipeline placed into the folder; that copy also runs without `--project` (and in
  Distribution leaves the default log file next to the project); a development project boots directly;
  `--log-level`, `--width` / `--height` (the platform subsystem's window line echoes the size) and `--jobs
  0|auto|abc|300` reach the configuration or are refused with the right code on standard error; a project path
  outside the active code page loads; `--help` -> 0 with every option line on the piped standard output and no
  information box (a box would block the child until the case times out; the box itself needs a desktop and is not
  automated). The `--scripting` cases (06 D12): a scripted project whose game assembly is missing boots with plain
  `--headless` and logs the `--scripting` hint; with `--headless --scripting` the same project fails the start in
  the scripting subsystem with exit 1 (which of `SCRIPT_RUNTIME_NOT_FOUND`, `SCRIPT_RUNTIME_INIT_FAILED` or
  `SCRIPT_ASSEMBLY_LOAD_FAILED` depends on what the machine has; with the child's `DOTNET_ROOT` pointed at an empty
  directory it is `SCRIPT_RUNTIME_NOT_FOUND`, the start on a machine without .NET, and a host that finds a runtime
  all the same makes that subcase report a `MESSAGE` and return), and `--scripting --no-scripting` exits 3; the
  `[dotnet]` case runs the probe on the real CLR and finds its `OnDestroy` line on the output. That case is compiled
  only in a build with the scripting module, the managed SDK and the test scripts
  (`CONJURE_RUNTIME_TEST_HAS_DOTNET`), so a build without them has no entry to skip; where it is compiled it is
  skipped only without `precondition::DOTNET_RUNTIME` (no .NET runtime installed, or `managed/` missing next to the
  runtime). The two others report a `MESSAGE` and return in a build without the scripting module. The F790 case
  holds an MSVC Distribution build to 06 D14: the build's `bin/` holds `vcruntime140.dll` and `msvcp140.dll`, the
  exported folder holds every `vcruntime140*.dll` and `msvcp140*.dll` of `bin/` at full size, and the exported
  runtime boots; in every other build it reports a `MESSAGE` and returns (the copy itself is covered on all
  platforms by `tests/engine/test_export.cpp`). The F200 case holds every build to 06 D15: `THIRD_PARTY_NOTICES.txt`
  lies next to the runtime, opens with Conjure's notice and licence, carries the licence text of yyjson (linked by
  every build), names Jolt and Dear ImGui exactly when the build links them and never doctest, and an export with
  the runtime carries the same bytes while one without it carries no file.
- `test_missing_project.cpp`: missing, malformed, invalid and too-new `project.json` -> 2 named on standard error
  without the help hint; a project whose own settings do not validate -> 2; `--log-file` names a missing project.
- `test_exit_codes.cpp`: bad arguments -> 3 with `conjure_runtime:` plus the usage text or the help hint on
  standard error; a failed startup -> 1; `--log-file` holds the engine's failed step and the runtime's summary.

`tests/integration/test_export_roundtrip.cpp` exports `tests/assets/projects/minimal` and boots the result the
same way. The runtime has no public headers, so there is no header-mirroring test file.

```
python tools/build.py --build-dir build/mod_runtime --target conjure_runtime_tests --test -R runtime
```

## 7. Deviations from the design

- 06 4.2 says "on error return `BAD_ARGUMENTS`" for a failed `from_command_line`; `ARCHITECTURE.md` 11 wants a
  missing `project.json` to exit 2, so the four project errors and an `INVALID_CONFIG` the project alone causes
  map to `RUNTIME_FAILURE`, everything else to `BAD_ARGUMENTS`.
- 06 4.2 does not say what a bare `conjure_runtime` does: it runs the `project.json` next to the executable and
  exits 3 with the usage text only when there is none.
- 06 3.4 step 1 leaves `logFile` empty by default; in Distribution the runtime fills it (`conjure_runtime.log`),
  logs its own messages into it and falls back to a message box, because `WIN32_GUI` leaves a shipped build
  without a console. Debug and Development keep the design's behaviour unless `--log-file` is given.
- 08 5.2 names `test_runtime_boot` and `test_missing_project`; both exist, the remaining exit codes live in
  `test_exit_codes.cpp` and the fixtures in `runtime_test_support.{h,cpp}`.
- The Windows build embeds an application manifest (`activeCodePage` UTF-8); no design document mentions the
  process code page.

## Related

- design record: [`06_engine_loop.md`](../../design/06_engine_loop.md) 4.2, 2.12; [`04_editor.md`](../../design/04_editor.md) 8; [`08_testing.md`](../../design/08_testing.md) 5.2, 6.3
- README: `runtime/README.md`
- 📘 API: [The runtime](../../api/runtime/index.md), [Export and diagnostics](../../api/engine/export-and-diagnostics.md)
- 🛠 Handbook: [Engine module](engine.md), [Build pipeline](../build-pipeline.md), [Continuous integration](../ci.md), [Testing](../testing.md)

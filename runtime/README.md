# conjure_runtime

The standalone game runtime (`docs/design/06_engine_loop.md` 4.2 and 2.12, `docs/ARCHITECTURE.md` 8 and 11,
`docs/design/04_editor.md` 8 "Distribution stripping"). It is one thin `src/main.cpp` over `conjure::engine`
plus the small platform backend of `src/runtime_platform.h`: every decision about configuration, startup, the
frame and shutdown lives in `engine::application`; the runtime maps outcomes to process exit codes and makes
sure the message behind an outcome reaches someone, console or not.

## What main.cpp does

1. `runtime::utf8_arguments(argc, argv)`: the arguments as UTF-8 (on Windows re-read from the wide command line,
   see "Paths and encodings"), then `engine::parse_command_line`. `--help` prints the runtime's usage line plus
   `engine::command_line_help()` on standard output and exits 0 before anything is loaded; when nothing reads
   standard output (`runtime::has_standard_output()`: the console-less Distribution build, double-clicked or
   started from a terminal) it also shows that text in an information box (`runtime::show_info_dialog`).
2. Refuses what the engine left for the executable: unknown options (the runtime consumes none; the editor owns
   `--exec` / `--export`).
3. Without `--project` it looks for `project.json` next to the executable (`runtime::executable_directory()`),
   which is how an exported folder starts from a double-click; only when neither is there does it exit 3 with
   the usage text.
4. In Distribution, without `--log-file`, it sets the log file to `conjure_runtime.log` in the project directory
   (when that exists) or next to the executable (see "Diagnostics").
5. `application_desc::from_options(options, engine_config::defaults())`: the whole merge of defaults <
   `--headless` < `project.json` < flags, validated once (06 D9). A project error is exit 2; an
   `INVALID_CONFIG` is exit 2 when the project's own settings fail the same validation without any flag (the
   merge of `--project` and the `--headless` mode alone), else exit 3 with the help hint.
6. `application app(desc)`; `add_subsystem(bindings::scripting_bindings_subsystem)` when the bindings module is
   built (startup step 16; compiled in through `CONJURE_RUNTIME_WITH_BINDINGS` when the `conjure::bindings`
   target exists); `layers().push_layer(game_layer)` (quits on the window's close event).
7. `initialise()`; on failure the engine has logged the step and the runtime exits 1 with its summary.
8. `return app.run()`; a non-zero result (a failed frame, a lost device) is named through the same channel as
   every other failure before the code is returned.

## Command line

Everything `engine::command_line` recognises, in particular `--project <dir>` (optional when `project.json`
sits next to the executable), `--headless`, `--scripting`, `--frames <n>`, `--width <n>`, `--height <n>`,
`--jobs <n|auto>`, `--log-level <level>`, `--log-file <path>`, `--scene`, the backend switches and the time
options. An exported folder runs as `conjure_runtime --project <dir>` or, from inside the folder, as a bare
`conjure_runtime`; the headless smoke test of an export is `conjure_runtime --project <dir> --headless --frames
60`.

A headless run starts without the scripting subsystem whatever `project.json` says (06 D12), so that smoke test
needs no .NET runtime and runs no C#. The smoke test of a scripted game, and a server, add `--scripting`:
`conjure_runtime --project <dir> --headless --scripting --frames 60`. With `scripting.required` at its default
the start then exits 1 with `SCRIPT_RUNTIME_NOT_FOUND` where no .NET runtime is found. `--scripting` with
`--no-scripting` exits 3. The rule is the engine's (`application_desc::from_options`, `apply_command_line`); the
runtime adds nothing to it.

## Exit codes (`engine::exit_code`, 06 2.12)

| Code | Name | Cause |
|---|---|---|
| 0 | `OK` | `run()` finished (`--frames` reached, window closed, `--help`) |
| 1 | `INIT_FAILURE` | `application::initialise()` failed (startup unwound; the error names the step) |
| 2 | `RUNTIME_FAILURE` | a frame failed, or `project.json` is missing, malformed, invalid or too new (`ARCHITECTURE.md` 11: "missing `project.json` -> exit 2"), or the project's own settings do not validate (`INVALID_CONFIG` with no flag involved) |
| 3 | `BAD_ARGUMENTS` | an unknown flag, a flag without its value, a bad value, a merged configuration a flag made invalid, no `--project` and no `project.json` next to the executable |
| 4 | `DEVICE_LOST` | the graphics device was lost during a frame (no recovery in v1) |

## Diagnostics

Every message the runtime prints itself goes to standard error as `conjure_runtime: <message>`; everything else
is the engine's console log (`--log-level` filters it). Because `WIN32_GUI` hides the console in Distribution,
the runtime adds three more ways out:

- **The log file.** The runtime's own line is also logged through `core::log` (category `runtime`, level
  ERROR) into the log file when there is one: `--log-file <path>`, or in Distribution the default
  `<project dir>/conjure_runtime.log` (the exported folder) when the project directory exists, else
  `<executable dir>/conjure_runtime.log`. The engine's `file_log_sink` exists only while the application
  runs (startup step 1 to `stop_core`), so the runtime opens its own sink in append mode for the one line: a
  failed start leaves a file that holds the engine's lines of the failed step followed by
  `[runtime] conjure_runtime: initialisation failed: ...`, and a failure before the application exists (a
  missing `project.json`, a bad flag) still leaves a file naming it. The console sink is paused for that one
  line so standard error carries the plain line only.
- **A message box.** When nothing reads standard error (`runtime::has_standard_error()`: no console, no pipe,
  no redirection, which is the console-less Distribution build on Windows, double-clicked or started from a
  terminal), the message plus the log file's path is shown with `MessageBoxW`. A process started by
  `core::process` (a pipe), a redirected one, or a Debug/Development build has a standard error and never sees
  the box, so the tests cannot trigger it.
- **An information box for `--help`.** The usage text goes to standard output; when nothing reads it
  (`runtime::has_standard_output()`, the same probe on the output handle) `runtime::show_info_dialog` shows it
  with `MessageBoxW` and `MB_ICONINFORMATION`. A Distribution build on Windows needs it both double-clicked and
  started from a terminal, because a GUI-subsystem process cannot write to the terminal's console. A piped
  `--help` never opens it: the tests read the text from the pipe, and a box would make them time out.

## Paths and encodings

On Windows `main(int, char **)` receives `argv` in the active code page, which cannot hold every path.
`runtime_platform_windows.cpp` re-reads the wide command line (`GetCommandLineW` + `CommandLineToArgvW`) and
converts it with `core::str::from_utf16`, so the argument vector is always UTF-8. The engine then turns those
strings into `std::filesystem::path` through the narrow conversion, which uses the process code page: the
embedded manifest `src/conjure_runtime.manifest` sets `activeCodePage` to UTF-8 (Windows 10 1903+), so every
narrow string <-> path conversion in the process agrees with the UTF-8 arguments. A project directory named in
Latin-1, Greek and CJK characters at once therefore loads (`tests/runtime/test_runtime_boot.cpp`). POSIX passes
the bytes of `argv` through.

## Build

`conjure_add_executable(conjure_runtime WIN32_GUI SOURCES src/main.cpp src/runtime_platform.h <backend> DEPS
conjure::shared ...)`: the executable links the umbrella shared library and nothing else from the engine (R17),
so neither the editor libraries nor Dear ImGui can reach it except through `conjure_shared`, and in Distribution
the editor is off and ImGui is not fetched. The platform backend is chosen in `CMakeLists.txt` the way
`core/system` chooses its backends: `src/runtime_platform_windows.cpp` (plus `user32` for `MessageBoxW`,
`shell32` for `CommandLineToArgvW` and the manifest) on Windows, `src/runtime_platform_posix.cpp` elsewhere.
`WIN32_GUI` hides the console window in Distribution only. The run path is the executable's own directory
(`$ORIGIN` / `@loader_path`) because `export_pipeline` step 5 places `conjure_shared` next to it.

### The licence notices next to the runtime (06 D15, code sweep F200)

Every build, in every configuration, places `THIRD_PARTY_NOTICES.txt` next to `conjure_runtime`, and export step 5
copies it into the game folder with the runtime.

- `conjure_ship_third_party_notices(conjure_runtime)` (`cmake/ConjureNotices.cmake`) declares the copy as a build
  output the target depends on: the file is there after the executable was built and comes back with the next
  build when it was deleted.
- The content is written at configure time from the notices registered in `third_party/CMakeLists.txt`: Conjure's
  `NOTICE` and `LICENSE`, then the licence text of every `RUNTIME` entry the tree uses and, in a tree that builds
  the editor, of the `EDITOR` entries (that tree's `conjure_shared` holds Dear ImGui and ImGuizmo). A
  Distribution tree names no editor library, and no tree names a `BUILD` entry (doctest, glslang).
- .NET is found on the build machine, so its entry carries `LICENSE.txt` and `ThirdPartyNotices.txt` of that
  installation (the configure warns when they are not there). The Visual C++ runtime entry of an MSVC build names
  Microsoft's terms without a text.
- The file has the line ends of the platform that builds it, and it lists the engine's software only.
- The tracked list for all platforms is `THIRD_PARTY_NOTICES.md` at the repository root; after a pin or a notice
  changed, regenerate it with `python tools/third_party_notices.py --build-dir <tree>` (the CTest entry
  `notices.up_to_date` fails until then; `docs/handbook/getting-started.md` section 11).
- `tests/runtime` holds a build to it with `regression - code sweep F200 the build writes the licence notices
  next to the runtime and an export ships them`.

### The Visual C++ runtime next to a Distribution runtime (06 D14, code sweep F790)

An MSVC build imports `vcruntime140.dll`, `vcruntime140_1.dll`, `msvcp140.dll` and `msvcp140_atomic_wait.dll`.
In a Distribution build with MSVC a `POST_BUILD` step of `conjure_runtime` copies every `vcruntime140*.dll` and
`msvcp140*.dll` of the toolchain's redistributable folder into `bin/`; export step 5 ships whatever matches the
same two patterns, so the exported folder starts without the Microsoft Visual C++ Redistributable.

- The folder comes from `include(InstallRequiredSystemLibraries)` and is accepted only when the version in its
  path is not lower than that of the folder under `VCToolsRedistDir` (set by `vcvars` and `tools/build.py`), or
  than the compiler's toolset (`14.<minor>`) when that variable is unset. CMake 3.31 answers a v145 compiler with
  the VC143 folder of another Visual Studio installation; an older library next to the executable would win over
  the installed one, so that answer is rejected and the `VCToolsRedistDir` folder is taken instead. That folder
  is paired with the active toolset by `vcvars`, and Visual Studio versions the redistributable apart from the
  tools, so it is taken without comparing it with the toolset as long as its path shows a version.
- The rule is `runtime/cmake/ConjureMsvcRuntime.cmake` (`conjure_msvc_toolset_version`,
  `conjure_msvc_crt_directory_version`, `conjure_msvc_accept_crt_directory`, `conjure_msvc_select_crt_libraries`,
  `conjure_msvc_find_crt_libraries`). `runtime/cmake/ConjureMsvcRuntimeProbe.cmake` holds it to fake folders in
  script mode, on every platform and without a compiler: the CTest entry `runtime.msvc_runtime_folder_rules`
  (labels `runtime;build`).
- When no folder passes, the configure prints a CMake warning and copies nothing; an export from that build
  warns that the game needs the redistributable.
- The step runs when `conjure_runtime` links. A library deleted from `bin/` by hand comes back with the next
  link of the executable, not with the next build.
- Debug and Development copy nothing: they are developer builds, which run where Visual Studio installed the
  runtime, and the debug runtime a Debug build imports must not be redistributed. A build against the static
  runtime copies nothing either. Export step 5 keeps the same line: a debug runtime library placed in a
  distribution directory by hand is left there and named in a warning of the export report.
- Not copied: the Universal CRT (part of Windows 10 and later), `concrt140.dll`, `vccorlib140.dll` and the
  OpenMP runtime, none of which the engine imports. A game module that imports one ships it itself.
- The copies are not serviced by Windows Update: a fixed runtime reaches players with a new export from a
  rebuilt Distribution.
- The target property `CONJURE_MSVC_CRT_LIBRARIES` lists the files (empty when nothing is copied);
  `tests/runtime` reads it, and its case `regression - code sweep F790 a Windows Distribution build ships the
  Visual C++ runtime DLLs with the exported game` checks the build's `bin/`, the exported folder and a boot from
  it. In every other build that case reports that it has nothing to check.

## Distribution stripping check

`tools/check_distribution_symbols.py` lists the symbols of the Distribution `conjure_runtime` (symbol table
and imports) and of the `conjure_shared` library next to it (exports and symbol table) with
`dumpbin /SYMBOLS /IMPORTS /EXPORTS` or `llvm-nm` / `nm`, and fails when any names `conjure::editor`, a
`conjure_editor*` binary, the `ImGui` / `ImGuizmo` namespaces or an ImGui type. The `conjure::editor` rules
cover the fully spelled namespace and the compressed forms both ABIs use for a parameter or return type of a
non-editor symbol: MSVC's back-reference (`...editor_layer@editor@3@...`) and Itanium's substitution
(`...NS_6editor12editor_layerE`); the self-test fixtures carry real MSVC and clang manglings of both. The
engine's own lower-case `imgui_layer` / `set_imgui_draw_data` are not findings. Exit codes: 0 clean, 1 findings,
2 usage or tool failure, 77 when the directory is not a Distribution build (CTest reads it as "skipped").

`tests/runtime/CMakeLists.txt` registers it twice: `runtime.check_distribution_symbols_tool` (the embedded
self-test, always) and `runtime.distribution_symbols` with the label `process`, which the default test presets
exclude (`CMakePresets.json` `base-test`). The latter is registered when the build directory is a
Distribution one, in a multi-config build (skipping unless `ctest -C Distribution`), or when the cache
variable `CONJURE_DISTRIBUTION_BUILD_DIR` points at a built Distribution directory. CI runs it on the
Distribution leg after the build:

```
cmake --preset windows-msvc-dist
cmake --build --preset windows-msvc-dist
ctest --test-dir build/windows-msvc-dist -L process --output-on-failure
```

or directly `python tools/check_distribution_symbols.py --build-dir build/windows-msvc-dist`.

## Tests

```
python tools/build.py --build-dir build/mod_runtime --target conjure_runtime_tests --test -R runtime
```

`tests/runtime/` spawns the built executable through `core::process` (`runtime_test_support.{h,cpp}`: a tiny
project with one authored startup scene and one data blob is generated into a `temp_directory` and exported with
`engine::export_pipeline`; `write_scripted_project` writes the same project with scripting enabled and one
`Conjure.TestScripts.DestroyProbe` in the scene, authored through the script runtime on the null host). Every case
spawns a process, so `conjure_runtime_tests` carries the labels
`unit;runtime;process` (08_testing.md 6.3 item 8): the default presets skip it and CI runs `-L process` in its
own step. The files follow 08_testing.md 5.2:

- `test_runtime_boot.cpp`: the exported folder boots headless for 60 frames with exit 0, with the build's runtime
  and with the copy the pipeline placed into the folder; that copy also runs without `--project` (the
  `project.json` next to it; in Distribution the default log file appears next to the project); a development
  project boots directly; `--log-level`, `--width` / `--height` (the platform subsystem's window line echoes the
  size), `--jobs 0|auto|abc|300` reach the configuration or are refused with the right code and stream; a project
  path outside the active code page loads; `--help` -> 0 with every option line on the piped standard output and
  no information box (a box would block the child until the case times out). `--scripting`: a scripted project
  with a missing game assembly boots with plain `--headless` and logs the hint that names the flag; `--headless
  --scripting` on it fails in the scripting subsystem with exit 1 (the error depends on whether the machine has
  .NET and the managed SDK); with the child's `DOTNET_ROOT` pointed at an empty directory, which is a machine
  without .NET to the host, that error is `SCRIPT_RUNTIME_NOT_FOUND` and the plain `--headless` boot still exits 0
  (the subcase reports a `MESSAGE` and returns where the host finds a runtime all the same); `--scripting
  --no-scripting` exits 3, and the `[dotnet]` case runs the probe on the real CLR (compiled only in a build with
  the scripting module and the managed SDK, and skipped only where no .NET runtime is installed; the two others
  report a `MESSAGE` and return in a build without the scripting module). The F790 case (06 D14) runs in an MSVC
  Distribution build: `bin/` holds `vcruntime140.dll` and `msvcp140.dll`, the exported folder holds every
  `vcruntime140*.dll` and `msvcp140*.dll` of `bin/` at full size, and the exported runtime boots; every other
  build reports a `MESSAGE` and returns. The F200 case (06 D15) runs in every build: the notices next to the
  runtime open with Conjure's own, carry yyjson's licence text, name Jolt and Dear ImGui exactly when the build
  links them and never doctest, and an export with the runtime carries the same bytes.
- `test_missing_project.cpp`: missing, malformed, invalid and too-new `project.json` -> 2 with the cause on
  standard error and no help hint; a project whose own settings do not validate -> 2; `--log-file` names a
  missing project in the file.
- `test_exit_codes.cpp`: no project, a missing value, a bad number, an unknown option, a flag-induced invalid
  merge -> 3 with `conjure_runtime:` and the usage text or the help hint on standard error; a missing startup
  scene or a cooked project without its pack -> 1; `--log-file` holds the engine's failed step and the runtime's
  summary.

The runtime has no public headers, so there is no header-mirroring test file.

## Deviations from the design (recorded for review)

- 06 4.2 says "on error return `BAD_ARGUMENTS`" for a failed `from_command_line`; the task and
  `ARCHITECTURE.md` 11 want a missing `project.json` to exit 2, so the four project errors
  (`PROJECT_NOT_FOUND`, `PROJECT_PARSE_FAILED`, `PROJECT_INVALID`, `PROJECT_SCHEMA_TOO_NEW`) and an
  `INVALID_CONFIG` the project alone causes map to `RUNTIME_FAILURE`; everything else to `BAD_ARGUMENTS`.
- 06 4.2 does not say what a bare `conjure_runtime` does; it runs the `project.json` next to the executable
  (the exported folder) and exits 3 with the usage text only when there is none.
- 06 3.4 step 1 installs "sinks (console + logFile)" with `logFile` empty by default; the runtime fills it in
  Distribution (default `conjure_runtime.log`), logs its own messages into it and falls back to a message box,
  because `WIN32_GUI` leaves a shipped build with no console. Debug and Development keep the design's
  behaviour unless `--log-file` is given.
- 08 5.2 names `test_runtime_boot` and `test_missing_project`; both exist, and the remaining exit codes live in
  a third file, `test_exit_codes.cpp`, with the fixtures in `runtime_test_support.{h,cpp}`.
- `bindings::scripting_bindings_subsystem` is added only when the `conjure::bindings` target exists
  (`engine/src/bindings` skips the module when `CONJURE_WITH_SCRIPTING` is off); the include path and class name
  follow `15_bindings.md` 4.
- The Windows build embeds an application manifest (`activeCodePage` UTF-8) so the engine's narrow-string
  path conversions decode the UTF-8 argument vector; no design document mentions the process code page.

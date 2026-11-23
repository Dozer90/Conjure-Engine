# The runtime
<!-- doc-template:wiki-index -->
[API](../index.md)

<!-- doc-content:purpose:start -->
`conjure_runtime` is the executable that ships with a game: one `main.cpp` over the engine's `application`
(📘 API: [Running the engine](../engine/running-the-engine.md)). It parses the engine command line, loads
`project.json`, adds the scripting bindings, pushes the `game_layer` that quits when the window closes, starts the
application and returns its exit code. Nothing of the editor is linked: a Distribution build has no editor and no
Dear ImGui, and `tools/check_distribution_symbols.py` proves it on the binaries.
<!-- doc-content:purpose:end -->

<!-- doc-content:body:start -->
## At a glance

| You want to... | Run |
|---|---|
| Play an exported game | double-click `conjure_runtime` in the exported folder, or `conjure_runtime --project <exported folder>` |
| Run a development project without the editor | `conjure_runtime --project <project dir>` |
| Smoke-test a build without a window | `conjure_runtime --project <dir> --headless --frames 60` |
| Smoke-test it with its C# scripts running | `conjure_runtime --project <dir> --headless --scripting --frames 60` |
| Run a server | `conjure_runtime --project <dir> --headless --audio off --fixed-frame-delta 0 --target-hz 30`, plus `--scripting` when the game has C# scripts (it then needs a .NET runtime) |
| See every option | `conjure_runtime --help` (a shipped build without a console shows the list in a dialog) |

## Command line

Everything `engine::command_line` recognises: `--project <dir>`, `--scene <path>`, `--headless`,
`--frames <n>`, `--rhi auto|d3d12|vulkan|null`, `--audio auto|miniaudio|null|off`, `--no-physics`,
`--scripting`, `--no-scripting`, `--no-render`, `--width <n>`, `--height <n>`, `--fullscreen`, `--no-vsync`,
`--fixed-hz <n>`, `--fixed-frame-delta <s>`, `--target-hz <n>`, `--time-scale <f>`, `--jobs <n|auto>`,
`--log-level <level>`, `--log-file <path>`, `--help`. The merge order is defaults, then `--headless`, then
`project.json`, then the flags. The runtime consumes no option of its own: an unknown flag is `BAD_ARGUMENTS`
with the usage text, and `--exec` / `--export` belong to the editor. Without `--project` the runtime runs the
`project.json` next to the executable, which is how an exported folder starts from a double-click; only when
there is none does it exit 3. Paths may contain any Unicode characters: on Windows the arguments are read from
the wide command line and the process runs with the UTF-8 code page.

> [!IMPORTANT]
> A `--headless` run does not execute C# scripts, whatever `project.json` says: it starts without the scripting
> subsystem, so it needs neither a .NET runtime nor the game assembly, and for a project that names a game assembly
> the log says `scripting: disabled; a headless run leaves the project's scripts off unless --scripting is given`.
> Add `--scripting` to run them. The start then fails with exit code 1 when no.NET runtime is found
> (`SCRIPT_RUNTIME_NOT_FOUND`) or the game assembly does not load, unless the project sets `"scripting": {
> "required": false }`. `--scripting` with `--no-scripting` is `BAD_ARGUMENTS`. A windowed run follows
> `project.json` and needs neither flag.

## Exit codes

| Code | Name | Cause |
|---|---|---|
| 0 | `OK` | `--frames` reached, the window closed, `--help` printed |
| 1 | `INIT_FAILURE` | `application::initialise()` failed; the log names the step (a missing startup scene, a cooked project without its pack, a required module that did not load, a scripted game without a .NET runtime) |
| 2 | `RUNTIME_FAILURE` | a frame failed, or `project.json` is missing, malformed, invalid or of a newer schema, or its own settings do not validate (a zero window size, a frame delta cap below the fixed step) |
| 3 | `BAD_ARGUMENTS` | an unknown flag, a flag without its value, a bad value, a configuration a flag made invalid, no `--project` and no `project.json` next to the executable |
| 4 | `DEVICE_LOST` | the graphics device was lost during a frame (no recovery in v1) |

The runtime's own messages go to standard error as `conjure_runtime: <message>`; everything else is the
engine's console log, filtered by `--log-level`. A shipped (Distribution) build has no console window, so it
writes the engine log and the runtime's messages to `conjure_runtime.log` next to `project.json` (next to the
executable when no project directory exists) unless `--log-file <path>` names another file, and when nothing reads
standard error at all (a shipped Windows build, double-clicked or started from a terminal) it also shows the
message in a dialog with the log's path. Debug and Development builds keep the console and write a log file only
with `--log-file`.

`--help` prints the option list on standard output; when nothing reads standard output (a shipped Windows build,
double-clicked or started from a terminal, which cannot write to the terminal's console) it also shows the list
in an information dialog.

> [!NOTE]
> After a game exits with a non-zero code, read `conjure_runtime.log` in the game's folder: the last lines name
> the failed startup step or frame, followed by the runtime's `conjure_runtime: ...` summary.

## Development project or exported folder

A development project (`project.json` with `"cooked": false`) is scanned and imported at start-up: the asset
database writes `.meta` sidecars next to the sources and the cooked cache under `<assets root>/.conjure/cache`,
exactly as the editor does. An exported folder (`"cooked": true`, the output of 📘 API: [Export and
diagnostics](../engine/export-and-diagnostics.md)) mounts only `engine://` and `cooked://` and reads
`cooked/content.cpak`; nothing is written. The exported folder is self-contained when the export copied the
runtime: `conjure_runtime`, `conjure_shared`, `shaders/` and (with scripting) `managed/` sit next to
`project.json`. On Windows the Visual C++ runtime DLLs (`vcruntime140*.dll`, `msvcp140*.dll`) sit there too when
the export copied the runtime of an MSVC Distribution build, and the game then needs no Microsoft Visual C++
Redistributable on the player's machine; an export whose report warned that it found none does. The debug
runtime is never among them.

`THIRD_PARTY_NOTICES.txt` in that folder holds the licence notices of the engine files: Conjure's own (Apache
License 2.0) and those of the third-party libraries the runtime was built with. A build writes it next to
`conjure_runtime` and the export copies it with the runtime. Ship it with the game, and add the notices of what
your game brings on its own (📘 API: [Export and diagnostics](../engine/export-and-diagnostics.md#licence-notices)).

> [!TIP]
> `--headless --frames N` is the whole smoke test of an export: it boots the cooked project on the null window,
> runs `N` frames and exits 0. The integration suite and the CI export smoke run exactly that. It does not run the
> game's scripts; for a scripted game `--headless --scripting --frames N` does, on a machine with .NET.

> [!WARNING]
> The runtime loads `conjure_shared` from its own directory (`$ORIGIN` / `@loader_path`). Copy the executable
> together with the shared library, never alone; on Windows keep `nethost.dll` and the Visual C++ runtime DLLs
> of the exported folder with it as well.

## See also

- 📘 API: [Engine: the application and the frame](../engine/index.md), [Running the engine](../engine/running-the-engine.md)
- 📘 API: [The editor](../editor/index.md) (which exports what the runtime runs)
- 🛠 Handbook: [Runtime](../../handbook/engine/runtime.md), [Building and Configuration](../../handbook/development/building-and-configuration.md)
<!-- doc-content:body:end -->

# conjure_editor_core and conjure_editor_ui

The editor module (`docs/design/04_editor.md`, ARCHITECTURE 7): everything an AI agent or a person needs to edit
a project, as two static libraries and one executable.

| Target | Contents | Public headers |
|---|---|---|
| `conjure::editor_core` | commands, undo, the newline-delimited JSON protocol (TCP, stdio, loopback), the dispatcher, play mode, picking, screenshots, the headless runner and `editor_application` | `engine/include/conjure/editor/*.h`, `.../protocol/*.h` |
| `conjure::editor_ui` | the Dear ImGui docking workspace, the panels, the property drawers and the ImGuizmo gizmos | `engine/include/conjure/editor/ui/**.h` |
| `conjure_editor` | `editor/src/main.cpp`: command line, `editor_application`, the UI layer | - |

`editor_core` never includes ImGui or sockets in a public header; `editor_ui` keeps ImGui in `src/`. Nothing here
is built in Distribution.

## Layout

```
core/                  editor_context, command_dispatcher, command_registry, undo_stack, play_controller, picker,
                       screenshot_service, headless_runner, editor_application, editor_module_consumer, ...
core/commands/         one file per command group (session, editor, history, console, project, scene, entity,
                       component, asset, prefab, script, play, input, view, physics, selection, reflection,
                       modules, profiler) plus command_support.{h,cpp}
core/component_access  component JSON in protocol form (entity refs as guid strings, aliases, suffix lookup)
core/protocol/         protocol_message, line_splitter, protocol_server, transports (tcp, stdio, loopback)
core/net/              socket.h + socket_win32.cpp / socket_posix.cpp (the only platform-specific files)
ui/                    editor_layer, editor_workspace, editor_panel, hierarchy_cache, property_drawer,
                       gizmo_controller, imgui_frame_scope, ui_support, editor_icon (+ generated
                       editor_icon_data.cpp, see docs/handbook/brand.md), panels/
```

## How a command runs

1. A transport yields a line; `protocol_server::poll` parses it (`PARSE_ERROR`, `INVALID_REQUEST`,
   `MESSAGE_TOO_LARGE` answered immediately) and caps requests per frame; the backlog carries over.
   `line_splitter` frames the bytes with a per-line limit and reports an oversized line as a marker at its
   position in the stream; the server marks the client `closing` on `MESSAGE_TOO_LARGE` or a failed token
   handshake and ignores the client's remaining lines of that poll. A line that is not valid UTF-8 is a
   `PARSE_ERROR` and keeps the client connected (04_editor.md table 6 lists `IO_ERROR` + disconnect; the
   robustness tests pin the gentler behaviour, so the design table is the one to update).
2. `command_dispatcher::dispatch`: unknown name -> `UNKNOWN_COMMAND` with `didYouMean`; the compiled JSON Schema
   of the arguments -> `INVALID_ARGS` naming the path; `REQUIRES_PROJECT`, `REQUIRES_SCENE`, `EDITING_ONLY`,
   `PLAYING_ONLY` -> the matching code before the handler runs.
3. The handler returns a `command_outcome`: result, inverse (mandatory for `MUTATES` without `NO_UNDO`;
   `skipRecording` for a run that changed nothing undoable, such as `asset.import` of a file already inside
   the project), an optional normalised replay and a label. The dispatcher records undo, bumps the revision,
   appends `revision` to the result and emits `scene.changed` and `command.executed`. A mutating handler
   that returns no inverse is a logged `INTERNAL` error, never an abort. Module commands (`command_desc`)
   cannot return an inverse, so their `MUTATES` is registered as `MUTATES | NO_UNDO`.
4. `ASYNC` handlers park an `async_reply`; `editor_context::tick` resolves completions once per frame and
   delivers them to the client's sink or transport. At most 8 pending per client (`BUSY`), with the
   `--timeout-seconds` deadline (`TIMEOUT`). A reply completed inside its own handler is matched by ticket
   and returned synchronously. `command_dispatcher::cancel_all` answers everything pending with `CANCELLED`:
   `editor_context::rebind` (project.open) and the context's destructor call it before dropping the frame
   tasks, and a frame task destroyed early (exec_file, wait_frames, compile, module build) releases its
   sink and event client and fails its own reply with `CANCELLED`.
5. `editor.batch` runs children atomically (inverses rolled back on failure) into one undo entry;
   `editor.begin_transaction` / `end_transaction` group commands per client.

Undo: `undo_stack` stores `{label, undo[], redo[], coalesceKey, clientId}`; `history.undo` dispatches the inverses
with recording suppressed. Coalescing merges consecutive single-command entries of equal key and client (the
inspector's drags), and inside an open transaction merges consecutive records of equal key and command (the
gizmo's drag session is one step however many frames it lasted). Inverses that restore several subtrees
(`entity.destroy_many`) are one `editor.batch` whose children run forwards in ascending sibling order, so
surviving siblings keep their places. Play mode freezes the stack (`D9`): `play.start` captures a
`scene_snapshot`, `play.stop` restores it byte for byte and remaps the selection by guid.

## Frame integration

`editor_application` pushes a core layer (`editor-core`) whose `on_update` calls `editor_context::tick`: poll
transports, cancel the requests of disconnected clients, resolve completions, dispatch inbound requests, tick play
/ screenshots / frame tasks (waits, `editor.exec_file`, compiles, module builds, tap releases), forward console
records as `log` events and flush. The UI layer (`editor-ui`) only draws; it is pushed after the engine's
`imgui_layer`, which owns the single ImGui context.

## Headless

`--headless` runs the null platform, rhi and audio with the script runtime on the null host, so every command
works without a window or .NET (`--dotnet` keeps the real .NET host for play tests that run C#; in every mode
`editor_application::start_engine` adds `bindings::scripting_bindings_subsystem` before `initialise()`, startup
step 16, when the bindings module is built: `CONJURE_EDITOR_WITH_BINDINGS`); `view.screenshot` and
`view.debug_draw` answer `NOT_AVAILABLE` on the null device
and `script.compile` / `module.build` spawn `dotnet` / `cmake` when present (`module.build` always runs the
`cmake` on the PATH; `config` and `target` must be plain names, and only MSVC / GCC / Clang / CMake diagnostic
lines count as errors or warnings). `--offscreen` asks for a real device without a swapchain; when the engine
configuration refuses it the editor logs a warning and falls back to the null device. A screenshot is captured on
the main thread and encoded and written on a `core::jobs` job that posts the completion back. `--exec file.json`
runs a JSON array or NDJSON of requests through `headless_runner` (exit 0 / 1 / 2 / 3). `--ai-stdio` reads stdin
in 64 KiB chunks through a `line_splitter` on a reader thread whose state is shared through a `shared_ptr`: the
thread is left blocked at exit (documented) and never touches the destroyed transport.

Units: `view.set_camera` / `view.get_camera` carry `fov` in degrees; `input.inject {"text"}` is UTF-8 and
injects one text event per code point (`INVALID_ARGS` for malformed bytes).

## Modules

`editor_module_consumer` attaches the `editor.command` and `editor.panel` extension points of loaded modules and
vetoes unloads while playing. The engine loads its static modules before the editor exists and the module manager
refuses a new consumer while anything is loaded, so `editor_context::install_module_consumer` unloads, adds the
consumer and loads again; a pre-load hook on the modules subsystem would remove that dance.

## Tests

`tests/editor` (`conjure_editor_tests`): one `test_<header>.cpp` per public header, the command catalogue fixtures
under `tests/editor/data/commands/`, the loopback end-to-end session, the headless runner, the Grid Sweeper
command file, the private helpers (`test_command_support.cpp`: base64, the build diagnostic parser) and the UI
panels drawn inside `imgui_frame_scope` on the null rhi. Every fixed bug has a `regression - ...` case next to
the behaviour it pins.

```
python tools/build.py --build-dir build/mod_editor --target conjure_editor_tests --test -R editor
```

`docs/generated/editor_protocol.json` is the `editor.schema` dump; regenerate it with `--conjure-update-golden`
when the catalogue changes, then refresh `.claude/skills/editor-ai-control/SKILL.md`.

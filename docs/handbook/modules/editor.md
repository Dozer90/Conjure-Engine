# Editor module

🛠 **Engineering handbook** · 📘 API: [The editor](../../api/editor/index.md), [The AI control protocol](../../api/editor/ai-protocol.md), [Headless mode and the command model](../../api/editor/headless-and-commands.md), [The workspace UI](../../api/editor/workspace-ui.md)

The editor is two wave-4 static libraries and one executable under `engine/src/editor` and `editor/`:
`conjure_editor_core` (alias `conjure::editor_core`: commands, undo, the newline-delimited JSON protocol over
TCP, stdio and the loopback, the dispatcher, play mode, picking, screenshots, the headless runner,
`editor_application`) and `conjure_editor_ui` (alias `conjure::editor_ui`: the Dear ImGui docking workspace, the
panels, the property drawers, the ImGuizmo gizmos), plus `editor/src/main.cpp`. Both libraries are declared from
`engine/CMakeLists.txt` (`editor` is the last entry of `CONJURE_ENGINE_MODULES`), join `conjure_shared` and are
absent from Distribution; `conjure_editor` links `conjure::shared` and nothing else from the engine. Like
`conjure_runtime`, the executable reads its arguments as UTF-8 (`editor/src/editor_arguments.h`, the Windows
backend re-reads the wide command line) and on Windows embeds an `activeCodePage` UTF-8 manifest
(`editor/src/conjure_editor.manifest`), so a project path outside the legacy code page survives; it keeps the
console subsystem in every configuration it is built in, and its run path is its own directory, where
`conjure_shared` sits. This chapter
is for people changing the editor or adding commands and panels; the binding design is
[`04_editor.md`](../../design/04_editor.md), the cross-area decisions R11-R15, R35 and R40 of
[`09_cross_area_resolutions.md`](../../design/09_cross_area_resolutions.md), and `engine/src/editor/README.md`
carries the finer notes.

## Contents

- [1. Shape of the module](#1-shape-of-the-module)
- [2. How a command runs](#2-how-a-command-runs)
- [3. Undo and play mode](#3-undo-and-play-mode)
- [4. The protocol](#4-the-protocol)
- [5. Frame integration and headless mode](#5-frame-integration-and-headless-mode)
- [6. The workspace](#6-the-workspace)
- [7. Modules](#7-modules)
- [8. Threading model](#8-threading-model)
- [9. Extension points](#9-extension-points)
- [10. Tests](#10-tests)
- [11. Open items](#11-open-items)
- [Related](#related)

## 1. Shape of the module

`engine/src/editor/CMakeLists.txt` declares `editor_core` (`PUBLIC_DEPS conjure::core conjure::ecs
conjure::platform conjure::assets conjure::scene conjure::modules conjure::engine`, private renderer, rhi,
physics, scripting and `ws2_32` on Windows) and `editor_ui` (`PUBLIC_DEPS conjure::editor_core`, private ImGui
and renderer), guarded by `CONJURE_BUILD_EDITOR` and the ImGui target. `editor_core` never includes ImGui or a
socket header in a public header; `core/net/socket_win32.cpp` / `socket_posix.cpp` is the one platform-specific
file pair.

| Area | Files | 📘 Wiki page |
|---|---|---|
| process object | `editor_application`, `editor_options`, `headless_runner` | [Headless mode and the command model](../../api/editor/headless-and-commands.md), [The AI control protocol](../../api/editor/ai-protocol.md) |
| services | `editor_context`, `undo_stack`, `selection_set`, `play_controller`, `picker`, `screenshot_service`, `editor_camera`, `console_buffer`, `event_broadcaster` | [Headless mode and the command model](../../api/editor/headless-and-commands.md) |
| command model | `command_dispatcher`, `command_registry`, `editor_command`, `builtin_commands`, `schema_validator`, `entity_ref`, `reflection_path`, `core/commands/*` (one file per group), `core/component_access.{h,cpp}` | same |
| module records | `command_desc`, `panel_desc`, `editor_module_consumer` | same |
| protocol | `protocol/protocol_message`, `protocol_server`, `protocol_transport`, `loopback_transport`, `line_splitter`; private `stdio_transport`, `tcp_transport`, `net/socket` | [The AI control protocol](../../api/editor/ai-protocol.md) |
| workspace | `ui/editor_layer`, `editor_workspace`, `editor_panel`, `property_drawer`, `gizmo_controller`, `hierarchy_cache`, `imgui_frame_scope`, `ui/panels/*`, private `ui_support` | [The workspace UI](../../api/editor/workspace-ui.md) |
| `editor_error.h`, `log_category.h` | `protocol_error_code` (21 codes), `LOG_EDITOR` | not user-facing |

## 2. How a command runs

1. A transport yields a line; `protocol_server::poll` parses it (`PARSE_ERROR`, `INVALID_REQUEST`,
   `MESSAGE_TOO_LARGE` answered at once) and caps requests per frame, carrying the backlog over. `line_splitter`
   frames the bytes with the 16 MiB limit and reports an oversized line as a marker; the server marks the client
   `closing` on `MESSAGE_TOO_LARGE` or a failed token handshake. A line that is not valid UTF-8 is a
   `PARSE_ERROR` that keeps the client connected (the design's table said `IO_ERROR` plus disconnect; the
   robustness tests pin the gentler behaviour).
2. `command_dispatcher::dispatch`: unknown name -> `UNKNOWN_COMMAND` with `didYouMean` (none for a name over 128
   bytes, whose edit distance would stall the frame; the message quotes at most 128 bytes, cut on a code point
   boundary); the compiled JSON schema
   (`schema_validator`: `type`, `properties`, `required`, `additionalProperties`, `enum`, `items`, bounds) ->
   `INVALID_ARGS` naming the path; `REQUIRES_PROJECT`, `REQUIRES_SCENE`, `EDITING_ONLY`, `PLAYING_ONLY` -> the
   matching code before the handler runs.
   Missing files answer one way everywhere (04_editor.md D20): handlers build the error with
   `support::unusable_path(path, reason)` for a path on disk (`PATH_NOT_FOUND` when nothing is there, `IO_ERROR`
   when something is but cannot be read) and `support::read_failure(path, error)` for a failed read through the
   virtual file system; `classify_error` maps core `NOT_FOUND` and engine `PROJECT_NOT_FOUND` to `PATH_NOT_FOUND`.
   `ASSET_NOT_FOUND` is kept for an asset reference that names no asset. A new command that reads a file uses the
   two helpers instead of choosing a code itself; one that hands a registered asset to the importer looks at the
   source first (`asset.reimport`), because an import failure carries no code and would come out as
   `INVALID_ARGS`.
3. The handler returns a `command_outcome`: result, inverse (mandatory for `MUTATES` without `NO_UNDO`;
   `skipRecording` for a run that changed nothing undoable), an optional normalised replay and a label. The
   dispatcher records undo, bumps the revision, appends `revision` to the result and emits `scene.changed` and
   `command.executed`. A mutating handler without an inverse is a logged `INTERNAL` error, never an abort.
4. `ASYNC` handlers park an `async_reply`; `editor_context::tick` resolves completions once per frame and
   delivers them to the client's sink or transport; at most 8 pending per client (`BUSY`), the
   `--timeout-seconds` deadline on the engine clock (`TIMEOUT`); `cancel_all` answers everything pending with
   `CANCELLED` on `project.open` and at shutdown.
5. `editor.batch` runs children atomically (inverses rolled back on failure) into one undo entry;
   `editor.begin_transaction` / `end_transaction` group a client's commands.

Entities cross the protocol as guids or hierarchy paths (`entity_ref`); component fields as reflection paths
(`reflection_path`: alias, unique suffix or registered name, then fields and array indices) read and written
through `ecs::component_to_json` / `component_from_json` (R40) and `component_access` (entity refs as guid
strings, the alias table `editor.schema` publishes). `component_access::write_component` sends the `layer`
component through `scene::set_layer`, so the editor never stores an index the scene's captures refuse.

## 3. Undo and play mode

`undo_stack` stores `{label, undo[], redo[], coalesceKey, clientId}`, depth 1024 and 256 MiB of serialised
arguments (oldest evicted first); `history.undo` dispatches the inverses with recording suppressed. Coalescing
merges consecutive single-command entries of equal key and client (inspector drags) and, inside an open
transaction, consecutive records of equal key and command (one gizmo drag is one step). Inverses that restore
several subtrees (`entity.destroy_many`) are one `editor.batch` whose children run in ascending sibling order.
Play mode (D9): `play.start` captures a `scene_snapshot`, freezes the stack and runs the engine; `play.stop`
restores the snapshot byte for byte (guids preserved) and remaps the selection, its primary included (the backup
names the primary last). While the stack is frozen `command_dispatcher::record_outcome` does not drop what a
handler returned: a scene edit goes to `play_controller::note_unrecorded_edit` (the restore discards it), an
`ON_DISK` command's inverse and replay go to `keep_file_edit`, and `play_controller::stop` pushes the kept ones as
one undo entry, `File changes during play` (D21, code sweep F089). A batch run while playing records which child
was an `ON_DISK` command (`batch_state::fileCommands`) and hands those over once it has succeeded. A new command
that writes project files gets the flag only when its inverse and replay touch no scene state (`prefab.apply`
replays as an `asset.restore` while playing for that reason); otherwise it is `EDITING_ONLY`, as `prefab.create`,
`scene.save` and `scene.save_as` are.
`play.step` runs exactly `n` fixed steps through `application::step_fixed` and answers once they ran (R13), and a
`play.stop` before then calls `application::cancel_fixed_steps`, because a forced step runs even while PAUSED. No play session crosses an engine restart: `play.start` answers `BUSY` while a
`project.open` is pending, and `apply_pending_project_open` stops play before it shuts the old engine down.
`editor_context::selection_payload` is the one builder of `{guids, primary}` (`primary` null for an empty
selection): every `selection.changed` carries it through `emit_selection_changed`, the selection's `on_changed`
handler and `play.stop` alike, and the `selection.*` answers add `entities` and `count` to it.

## 4. The protocol

`protocol_server::start(options, projectRoot)` opens TCP on `first_port()` (nine fallback ports when the port
was not explicit; the chosen port is written to `<project>/.conjure/ai_port`, and
`editor_application::apply_pending_project_open` moves the file with `set_project_root` when `project.open`
switches projects) and stdio with `--ai-stdio`; `add_transport` plugs in a `loopback_transport` for tests. When
the stdio transport reports its client closed, `stdio_closed()` turns true and a headless stdio-only editor (no
TCP server) quits with 0. Clients are polled on the main thread
(`transport_kind::TCP` and `LOOPBACK`). `tcp_transport` serves at most `MAX_CLIENTS` (8) connections: a further
client is accepted, sent one `BUSY` line with id null (best effort) and closed. A port in a Windows excluded range
(`WSAEACCES`) counts as busy for the fallback search (`net::is_port_unavailable_error`), and on POSIX every socket
is close-on-exec so `dotnet` and `cmake` children never inherit the listener. With `--ai-token` no event reaches a
client before its valid `session.hello`. The stdio reader is the one `core::jobs::dedicated_thread` of the process
(R38), reading 64 KiB chunks through a `line_splitter` whose state is shared through a `shared_ptr`, left blocked
at exit by design. `event_broadcaster` fans events out with per-client subscriptions and a minimum log level; log
events are shed under backlog pressure (`events.dropped`).

## 5. Frame integration and headless mode

`editor_application` pushes a core layer (`editor-core`) whose `on_update` calls `editor_context::tick`: poll
transports, cancel disconnected clients' requests, resolve completions, dispatch inbound requests, tick play,
screenshots and frame tasks (waits, `editor.exec_file`, compiles, module builds, tap releases), forward console
records as `log` events, flush. The UI layer (`editor-ui`) only draws and is pushed after the engine's
`imgui_layer`, which owns the one ImGui context (R9). `--headless` runs the null platform, rhi and audio with the
script runtime on the null host; `view.screenshot` and `view.debug_draw` answer `NOT_AVAILABLE`; `script.compile`
and `module.build` spawn `dotnet` / `cmake` when present. `--offscreen` asks for a real device without a
swapchain; the engine configuration refuses it today and the editor falls back to the null device with a warning.
`--exec` runs a JSON array or NDJSON of requests through `headless_runner` (a leading UTF-8 byte order mark
skipped, `${project}` expanded per request to the project open at the time, `--keep-going`); an asynchronous
request is bounded by the dispatcher's `--timeout-seconds` in wall time, not by a frame count, and `editor.quit`
ends the run with its non-zero exit code. The command file is read and parsed by `editor_application::initialise`
(`headless_runner::load_command_file`) after `make_engine_desc` checked the arguments and the project and before
`boot_engine` starts the engine, so an unusable file is `BAD_ARGUMENTS` without an engine start; `run_exec` hands
the parsed requests to `headless_runner::run` (04_editor.md D22). A startup scene the engine could not load is
kept by `engine::application::startup_scene_failure()`; `run_requests` writes it as the first output line,
`warning {code: "STARTUP_SCENE_FAILED", message}`, through `editor_application::startup_scene_warning()`, and
`apply_pending_project_open` emits the same event to every client after a restart. Neither changes the exit code.
`editor/src/main.cpp` maps the outcome to the exit code: 0 ok, 1 a
command (or `--export`) failed, 2 a startup or project failure (a failed `initialise()`, unless the arguments are
at fault, or a `project.open` whose restart failed for both projects), 3 bad arguments (a bad editor flag, an
option neither the editor nor the engine knows, which `make_engine_desc` refuses as `BAD_ARGUMENTS` before the
merge loads the project, a flag that makes the merged configuration `INVALID_CONFIG` while the project alone
validates, or an `--exec` file that is missing or does not parse), 4 a lost graphics device; a response stream that fails mid-run (`--out` on a full disk) also ends
`--exec` with 2. When `editor_options::stdout_carries_protocol()` (`--ai-stdio`, `--exec`
without `--out`, `--export`), main replaces the console log sink with one that writes every line to standard
error, so standard output holds protocol lines only. `--export <dir>` sends `project.export {directory}`, plus
`distributionDirectory` when `--distribution <dir>` names the folder `conjure_runtime`, `conjure_shared` and
`shaders/` come from (the parser refuses `--distribution` without `--export`); without it the reply warns that the
runtime was not copied.

## 6. The workspace

`editor_workspace` is the full-window dockspace with a pass-through central node (the scene renders underneath),
the menu bar, the panel instances and `<project>/.conjure/editor_layout.ini`. Panels (`editor_panel` subclasses
in `ui/panels/`) own only UI state and submit commands through the dispatcher: hierarchy (flat rows from
`hierarchy_cache`, one pre-order walk per structural change, a list clipper so 10,000 entities cost the visible
rows, D11), inspector (`component.get` per component, `property_drawer_registry` per field kind, `component.set`
with the drag session as coalesce key; the float drawers read and write the `"inf"` / `"-inf"` / `null` encoding of
10_core.md C15, so a non-finite value is shown as such, survives an edit next to it and is never added by a typed
`inf`), viewport (the scene through the editor camera as an image, orbit camera,
`view.pick`, `gizmo_controller` over ImGuizmo dispatching `component.set_many` inside a transaction), asset
browser (its Delete goes through `request_delete`: the file behind the open scene, which
`editor_context::is_open_scene` tells by file and not by spelling, raises a modal confirmation and only "Close
and Delete" sends `asset.delete {closeOpenScene: true}`, disabled while playing or paused, when the command would
answer `WRONG_PLAY_STATE`, 04_editor.md D20), console, history, toolbar,
profiler, AI session, export dialog (output directory only; an export is always Distribution, D19). The viewport's
picture comes from a
`scene_view` (`core/scene_view.h`, private): a render target the size of the panel in framebuffer pixels and the
editor camera's view, submitted every frame with `renderer::submit_view`; the target's colour texture is
registered once with `imgui_renderer::register_texture` (its id survives a resize) and drawn under the input
surface, over an opaque backdrop because the target is transparent where the scene drew nothing. The panel is
destroyed by `editor_layer::on_detach`, before the renderer goes. `imgui_frame_scope` lets the UI tests draw every panel on the null RHI without the engine's
layer. `editor_layer::on_attach` also sets the window icon from `ui/editor_icon` (the logo's PNGs, generated into
`ui/editor_icon_data.cpp`); the [Logo and application icon](../brand.md) chapter covers the icon end to end.

## 7. Modules

`editor_module_consumer` attaches the `editor.command` (`command_desc`) and `editor.panel` (`panel_desc`)
extension points of loaded modules: commands enter the registry with `owner = <module>` and `MUTATES | NO_UNDO`
(a module command cannot return an inverse), panels enter the workspace through `sync_module_panels`; it vetoes
unloads while playing and strips both on detach. The engine loads its static modules before the editor exists
and the module manager refuses a new consumer while anything is loaded, so `editor_context::install_module_consumer`
unloads, adds the consumer and loads again (section 11).

## 8. Threading model

Main thread only, except the stdio reader thread (R38) and the screenshot encode job: a capture is taken on the
main thread and encoded and written on a `core::jobs` job that posts the completion back (`screenshot_service`);
without a job system the encode runs inline. Sockets are non-blocking and polled on main.

## 9. Extension points

- Commands: a `command_descriptor` in `core/commands/<group>.cpp` with a JSON schema, flags and a handler
  returning `command_outcome` (plus a fixture under `tests/editor/data/commands/<name>.json`, D8c); modules add
  `command_desc` records.
- Panels: an `editor_panel` subclass registered in `panel_registry`; modules add `panel_desc` records and link
  `conjure::thirdparty::imgui_interface` (`EDITOR_UI`).
- Property drawers: `property_drawer_registry::set(kind, drawer)`.
- Screenshot backends: `screenshot_backend` (`memory_screenshot_backend` for tests, the renderer backend over a
  `scene_view` and `read_back`): `begin` when the request is accepted, `prepare_frame` on every frame it waits,
  `frame_rendered` to tell the service a frame was drawn, `capture`, and `abandon` when the engine restarts or
  shuts down under a waiting request (the renderer may be gone, so nothing may be destroyed through it).
- Transports: a `protocol_transport` implementation added through `protocol_server::add_transport`.

## 10. Tests

`tests/editor/` (`conjure_editor_tests`, label `editor`): one `test_<header>.cpp` per public header of both
libraries, the command catalogue fixtures under `tests/editor/data/commands/` (`test_command_catalogue.cpp`
runs the inverse round trip of every mutating command, D8c), `test_batch_command`, the per-area command files,
`test_protocol_robustness` (the corpus of broken lines), `test_ai_protocol_e2e` (a loopback session),
`test_headless_runner`, `test_ai_builds_grid_sweeper` (the Grid Sweeper command file, also the integration
scenario `tests/integration/test_ai_protocol_e2e.cpp`), the UI panels drawn inside `imgui_frame_scope` on the
null rhi, and `test_editor_process` (label `process`, excluded from the default presets) spawning the real
`conjure_editor --headless --exec`. A case whose name carries `[slow]` gets the label `slow` from the same side
file of `tests/editor/CMakeLists.txt`, so only the nightly presets run it. Every undoable fixture must change
the snapshot (the serialised scene and the project files) before its undo round trip counts; a fixture whose
effect the snapshot cannot see (an empty folder) says `"expectChange": false` with a `_comment` naming the case
that checks it instead. `editor_test_support.h` provides `headless_editor` (a headless `editor_application` over a
scratch project with an in-process client, `send` / `ok` / `fail`, snapshots for the inverse round trips), the
fixture file format and the `LOOPBACK_TCP` precondition probe. `docs/generated/editor_protocol.json` is the
`editor.schema` dump; regenerate it with `conjure_editor_tests --conjure-update-golden` when the catalogue
changes, then refresh `.claude/skills/editor-ai-control/SKILL.md`.

```
python tools/build.py --build-dir build/mod_editor --target conjure_editor_tests --test -R editor
```

## 11. Open items

From `09_cross_area_resolutions.md` 4 item 15: off-screen rendering (`--offscreen`, `view.screenshot` with
pixels) has its renderer view hook (`renderer::submit_view`, code sweep F104) but still needs an engine
configuration that allows a real backend without a window; the scene cameras still draw into the back buffer
behind the editor's panels until the Scene / Game split of `04_editor.md` D18 exists; a
pre-load consumer hook on `modules_subsystem` would remove the unload / load dance of
`install_module_consumer`; a public `importer_registry` accessor on `asset_database` would replace the importer
inference of `reflection.asset_types`; `scene::root_index_of` and an id-independent `ecs::world_hash` would
simplify `entity.restore` and the restore comparisons.

## Related

- design record: [`04_editor.md`](../../design/04_editor.md), [`17_demo_games.md`](../../design/17_demo_games.md) (Grid Sweeper), [`09_cross_area_resolutions.md`](../../design/09_cross_area_resolutions.md) (R11-R15, R35, R40)
- README: `engine/src/editor/README.md`; skill: `.claude/skills/editor-ai-control/SKILL.md`
- 📘 API: [The editor](../../api/editor/index.md)
- 🛠 Handbook: [Engine module](engine.md), [Modules module](modules.md), [Testing](../testing.md)

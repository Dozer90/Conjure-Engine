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
core/protocol/         protocol_message, line_splitter, protocol_server, access_token, audit_log, transports (tcp,
                       stdio, loopback)
core/net/              socket.h + socket_win32.cpp / socket_posix.cpp
core/user_paths*       the per-user state folder: user_paths.{h,cpp} + user_paths_windows.cpp /
                       user_paths_posix.cpp (with the socket pair, the only platform-specific files)
ui/                    editor_layer, editor_workspace, editor_panel, hierarchy_cache, property_drawer,
                       gizmo_controller, imgui_frame_scope, ui_support, editor_icon (+ generated
                       editor_icon_data.cpp, see docs/handbook/brand.md), panels/
```

## How a command runs

1. A transport yields a line; `protocol_server::poll` parses it (`PARSE_ERROR`, `INVALID_REQUEST`,
   `MESSAGE_TOO_LARGE` answered immediately) and caps requests per frame; the backlog carries over.
   `line_splitter` frames the bytes with a per-line limit and reports an oversized line as a marker at its
   position in the stream; the server marks the client `closing` on `MESSAGE_TOO_LARGE`, on a failed token
   handshake and on a first line that is not a request ("Who may connect"), and ignores the client's remaining
   lines of that poll. After a client's first request a line that is not valid UTF-8 is a `PARSE_ERROR` and
   keeps the client connected (04_editor.md table 6 lists `IO_ERROR` + disconnect; the
   robustness tests pin the gentler behaviour, so the design table is the one to update).
2. `command_dispatcher::dispatch`: unknown name -> `UNKNOWN_COMMAND` with `didYouMean`; a command the client's
   token does not open -> `UNAUTHORISED` (see "Who may connect"); the compiled JSON Schema
   of the arguments -> `INVALID_ARGS` naming the path; `REQUIRES_PROJECT`, `REQUIRES_SCENE`, `EDITING_ONLY`,
   `PLAYING_ONLY` -> the matching code before the handler runs.
   A file or folder that is not there is `PATH_NOT_FOUND` in every command and one that exists but cannot be
   read is `IO_ERROR` (`support::unusable_path` for a path on disk, `support::read_failure` for a read through
   the virtual file system; engine `PROJECT_NOT_FOUND` classifies as `PATH_NOT_FOUND`). `ASSET_NOT_FOUND` is an
   asset reference that names no asset (code sweep F656, 04_editor.md D20).
3. The handler returns a `command_outcome`: result, inverse (mandatory for `MUTATES` without `NO_UNDO`;
   `skipRecording` for a run that changed nothing undoable, such as `asset.import` of a file already inside
   the project), an optional normalised replay and a label. The dispatcher records undo, bumps the revision,
   appends `revision` to the result and emits `scene.changed` and `command.executed`. The inverses and replays
   that `history.undo` / `redo` dispatch neither bump nor emit: the history command does both once per step
   (labelled `Undo X` / `Redo X`, or `... (failed)` after a step that broke off half-way). A mutating handler
   that returns no inverse is a logged `INTERNAL` error, never an abort. Module commands (`command_desc`)
   cannot return an inverse, so their `MUTATES` is registered as `MUTATES | NO_UNDO`.
   `asset.delete` dispatches a second command: for the file behind the open scene, and only with
   `closeOpenScene: true`, it deletes the file and then runs `scene.close {discard: true}` (D20). The delete comes
   first because it is the step that can fail: a file the file system will not remove leaves the scene open
   with its unsaved changes and its history (and should the close fail, the file is put back through the
   delete's own inverse). Closing clears the undo history, so that path is refused inside a batch, an open
   transaction, an undo or redo step and during play; the asset browser's confirmation dialog is the UI's way
   to send the flag, and it disables its "Close and Delete" while playing or paused instead of sending a request
   that would be refused. Both ask `editor_context::is_open_scene`, which compares files rather than spellings
   (`names_asset`: a scene opened as `Scenes/Main` on a volume that ignores case is the asset registered as
   `scenes/main`). Deleting the project's startup scene emits `warning {code: "STARTUP_SCENE_DELETED"}`.
4. `ASYNC` handlers park an `async_reply`; `editor_context::tick` resolves completions once per frame and
   delivers them to the client's sink or transport. At most 8 pending per client (`BUSY`), with the
   `--timeout-seconds` deadline (`TIMEOUT`). A reply completed inside its own handler is matched by ticket
   and returned synchronously. `command_dispatcher::cancel_all` answers everything pending with `CANCELLED`:
   `editor_context::rebind` (project.open) and the context's destructor call it before dropping the frame
   tasks, and a frame task destroyed early (exec_file, wait_frames, compile, module build) releases its
   sink and event client and fails its own reply with `CANCELLED`.
5. `editor.batch` runs children atomically (inverses rolled back on failure) into one undo entry; the children's
   inverses are collected even while playing or while the batch is itself an undo / redo step, so the rollback
   always works, and only then is it decided whether the entry reaches the stack.
   `editor.begin_transaction` / `end_transaction` group commands per client; `history.undo`, `redo` and `clear`
   are refused (`INVALID_REQUEST`) while any client's transaction is open. `editor.exec_file` runs its file as a
   private client: a transaction the file leaves open is committed when the task ends, and the task stops once
   its own request was cancelled or timed out. An `editor.wait_frames` whose request is gone leaves at once.

Undo: `undo_stack` stores `{label, undo[], redo[], coalesceKey, clientId}`; `history.undo` dispatches the inverses
with recording suppressed. Coalescing merges consecutive single-command entries of equal key and client (the
inspector's drags), and inside an open transaction merges consecutive records of equal key and command (the
gizmo's drag session is one step however many frames it lasted). Inverses that restore several subtrees
(`entity.destroy_many`) are one `editor.batch` whose children run forwards in ascending sibling order, so
surviving siblings keep their places. Play mode freezes the stack (`D9`): `play.start` captures a
`scene_snapshot`, `play.stop` restores it byte for byte and remaps the selection by guid.

Edits while playing (`D21`, code sweep F089). The restore takes back the scene and nothing on disk, so the
dispatcher sorts what a handler returned by the command's flags while the stack is frozen:

- no `ON_DISK`: a scene edit. Not recorded; `play_controller::note_unrecorded_edit` warns once per session
  (`UNRECORDED_PLAY_EDIT`).
- `ON_DISK` (`asset.import`, `asset.create_folder`, `asset.delete`, `asset.create_material`, `asset.set`,
  `asset.restore`, `prefab.apply`, `project.set_settings`, `script.create`, `script.write`, `script.delete`): a
  file edit. `play_controller::keep_file_edit` collects the inverse and replay (warning `PLAY_FILE_EDIT` once per
  session), and `play_controller::stop` pushes them as one undo entry, `File changes during play`; `play.stop`
  answers the count as `fileChanges`. A batch marks its `ON_DISK` children in `batch_state::fileCommands` and
  hands them over only when it did not roll back.
- `prefab.create`, `scene.save` and `scene.save_as` are `EDITING_ONLY`: they tie a file to scene state the
  restore removes.

`ON_DISK` promises that the inverse and the replay touch no scene state (they run after the restore); the registry
refuses it without `MUTATES` or with `NO_UNDO`, and `editor_module_consumer` drops it from module commands.
`prefab.apply` keeps the promise by replaying as an `asset.restore` of the bytes it wrote when it ran while
playing.

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
configuration refuses it the editor logs a warning and falls back to the null device. A screenshot draws the scene
through its camera into a render target (`core/scene_view.h`: `scene_view`, `make_editor_camera_view`,
`resolve_camera_view`; the viewport panel shows the editor camera the same way), is captured on the main thread
once a frame has been rendered into the target, and is encoded and written on a `core::jobs` job that posts the
completion back; screenshots asked for together are taken one at a time, in request order. `--exec file.json`
runs a JSON array or NDJSON of requests through `headless_runner` (exit 0 / 1 / 2 / 3). The file is read and
parsed in `editor_application::initialise` (`headless_runner::load_command_file`), after the arguments and the
project were checked and before `boot_engine` starts the engine, so a missing or malformed file exits 3 without an
engine start. When the engine could not load the startup scene (`engine::application::startup_scene_failure()`),
`run_requests` writes `warning {code: "STARTUP_SCENE_FAILED", message}` as the first output line and the exit code
stays the requests' own; after a `project.open` restart `apply_pending_project_open` emits the same event to every
client (04_editor.md D22). `--ai-stdio` reads stdin
in 64 KiB chunks through a `line_splitter` on a reader thread whose state is shared through a `shared_ptr`: the
thread is left blocked at exit (documented) and never touches the destroyed transport. Once standard input
ends, `protocol_server::stdio_closed()` turns true and a headless `--ai-stdio` editor without a TCP server quits
with 0 (`editor_application::run`). Whenever standard output carries protocol data (`--ai-stdio`, `--exec`
without `--out`, `--export`: `editor_options::stdout_carries_protocol`), `main` replaces the console sink with
one that writes every log line to standard error, so each stdout line parses. The session files
`<project>/.conjure/ai_port`, `ai_token` and `ai_token_inspect` follow `project.open`, and so does the audit log
`ai_audit.log`: `apply_pending_project_open` moves them with `protocol_server::set_project_root`. `initialise` failures exit 3 for `BAD_ARGUMENTS` (an option neither the
editor nor the engine knows, refused by `make_engine_desc` before the project loads) or an `INVALID_CONFIG` a
flag caused, otherwise 2.

Selection: `selection_set::primary()` is the guid named last by the `set` / `add` that last changed the
selection (the lowest remaining guid once it leaves, null when empty); `play.start` names it last in its backup,
so `play.stop` restores it. `editor_context::selection_payload` is the one builder of `{guids, primary}`:
`selection.get` / `set` / `clear` and every `selection.changed` (`emit_selection_changed`) start from it, so all
of them carry `primary` as null for an empty selection, never `""`.

Units: `view.set_camera` / `view.get_camera` carry `fov` in degrees; `input.inject {"text"}` is UTF-8 and
injects one text event per code point (`INVALID_ARGS` for malformed bytes).

## Who may connect

A protocol client with the full-control token has the reach of the user running the editor, so the connection
is what is guarded (04_editor.md D23, D24; the handbook page, section 4.1, has the table of the pieces). A TCP
client must open with `session.hello {token}`. Two tokens are generated when the TCP transport starts
(`protocol/access_token.h`), compared in one place (`protocol_server::access_of`, constant time; the guards that look for a token inside client text use `constant_time_contains` too) and written to
`<project>/.conjure/ai_token` and `ai_token_inspect` through `core::vfs::write_private_file`, readable by their
owner only, before `ai_port` is written beside them. `--ai-token` and `--ai-inspect-token` replace them and gate
stdio and the loopback too. A client whose first line is not a request is disconnected on every transport,
which ends an HTTP request at its request line. Never log a token, put it in an event or quote it in an error;
the tests search for both. A path argument becomes an on-disk path in `support::project_relative_path` only:
relative means relative to the project root.

Scoped tokens and the audit log (04_editor.md D25, D26; handbook section 4.2). The inspect-only token opens the
commands flagged `command_flags::READ_ONLY` and no others: `required_access(flags)` is the rule and
`command_dispatcher::dispatch_internal` applies it before the arguments are validated. Flag a new command only
if it changes nothing clients share, starts no program and loads no code, and add it to the pinned list in
`tests/editor/test_command_catalogue.cpp`. Every request of a transport client is appended to
`<project>/.conjure/ai_audit.log` (`protocol/audit_log.h`: one JSON line, tokens kept out, at most 4 MiB per
file and one previous file); the editor's own requests are not recorded.

## Which projects are opened

Opening a project runs its native modules and scripts, so the editor opens one only after somebody answered for
it (04_editor.md D27; handbook section 5.1). `project_trust` holds the answers: for the session in memory,
remembered in the per-user trust store outside every project (`default_trust_store_file()`, or `--trust-store`).
`editor_application::require_trusted` is the one gate, asked by `initialise` (`--project`),
`request_open_project` (`project.open`) and the restart itself; it answers `PROJECT_NOT_TRUSTED`. Answers are
always explicit: the window's question (`editor_context::trust_prompt`, drawn by the workspace),
`--trust-project` for one run, or `project.trust`. A headless, `--exec` or `--export` session cannot ask and
fails with exit code 2. Do not add a command that trusts as a side effect (`project.create` does so only for a
folder it made itself), and route every new way of opening a project through `require_trusted`. The
library never names the user's store by itself: `editor/src/main.cpp` does, so tests remember nothing.

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
the behaviour it pins. `headless_editor` trusts its scratch project for the session and names no trust store;
`headless_editor::trust_project` answers for a second hand-written project, and the process cases give the real
executable a `--trust-store` in a scratch folder.

```
python tools/build.py --build-dir build/mod_editor --target conjure_editor_tests --test -R editor
```

`docs/generated/editor_protocol.json` is the `editor.schema` dump; regenerate it with `--conjure-update-golden`
when the catalogue changes, then refresh `.claude/skills/editor-ai-control/SKILL.md`.

# Headless mode and the command model
<!-- doc-template:wiki -->
[API](../index.md) → [Editor](index.md)

<!-- doc-content:purpose:start -->
The editor from the C++ side: the process object (`conjure/editor/editor_application.h`,
`conjure/editor/editor_options.h`), which projects it opens (`conjure/editor/project_trust.h`), the service
table (`conjure/editor/editor_context.h`), the command model
(`conjure/editor/command_dispatcher.h`, `conjure/editor/command_registry.h`, `conjure/editor/editor_command.h`,
`conjure/editor/builtin_commands.h`, `conjure/editor/schema_validator.h`), the services commands drive
(`conjure/editor/undo_stack.h`, `conjure/editor/selection_set.h`, `conjure/editor/play_controller.h`,
`conjure/editor/picker.h`, `conjure/editor/screenshot_service.h`, `conjure/editor/editor_camera.h`,
`conjure/editor/console_buffer.h`), addressing (`conjure/editor/entity_ref.h`, `conjure/editor/reflection_path.h`),
events (`conjure/editor/event_broadcaster.h`) and what a module adds (`conjure/editor/command_desc.h`,
`conjure/editor/panel_desc.h`, `conjure/editor/editor_module_consumer.h`).
<!-- doc-content:purpose:end -->

|---|---|
| Name | Headless mode and the command model |
| Module | Editor |
| `#include` | `conjure/editor/builtin_commands.h`, `conjure/editor/command_desc.h`, `conjure/editor/command_dispatcher.h`, `conjure/editor/command_registry.h`, `conjure/editor/console_buffer.h`, `conjure/editor/editor_application.h`, `conjure/editor/editor_camera.h`, `conjure/editor/editor_command.h`, `conjure/editor/editor_context.h`, `conjure/editor/editor_module_consumer.h`, `conjure/editor/editor_options.h`, `conjure/editor/entity_ref.h`, `conjure/editor/event_broadcaster.h`, `conjure/editor/panel_desc.h`, `conjure/editor/picker.h`, `conjure/editor/play_controller.h`, `conjure/editor/project_trust.h`, `conjure/editor/reflection_path.h`, `conjure/editor/schema_validator.h`, `conjure/editor/screenshot_service.h`, `conjure/editor/selection_set.h`, `conjure/editor/undo_stack.h` |
| Engineering Handbook | [Engineering handbook](../../handbook/editor/editor.md) |

## Syntax

<!-- doc-content:syntax:start -->
Usage and complete examples are described in the reference sections below.
<!-- doc-content:syntax:end -->

## At a glance

<!-- doc-content:at_a_glance:start -->
| You want to... | Use |
|---|---|
| Run the editor headless from a test or tool | `editor_options { headless = true, projectPath, aiPort = 0 }`, `editor_application::initialise(options, {})`, `pump_frame()` |
| Open a project the editor did not create | answer for it first: `options.trustProject = true` for this run, or `application.trust().trust(root, remember, origin)`; see [Project trust](#project-trust) |
| Send a command from C++ | `context.dispatcher().dispatch(request)` (a response, or `nullopt` for an asynchronous one that answers through a sink) |
| Register a command | `context.registry().add(command_descriptor)`; from a module, a `command_desc` extension record |
| Add a panel from a module | a `panel_desc` extension record; `editor_module_consumer` attaches it |
| Resolve `"Board/Cell_3"` or a guid to an entity | `entity_ref::resolve(json, scene)`, `entity_ref::guid_of`, `entity_ref::path_of` |
| Read or write one field of a component by path | `reflection_path::parse("transform.position.x")`, `read_at`, `write_at` |
| Undo and redo programmatically | `context.undo()`: `pop_for_undo()`, `pop_for_redo()`, `begin_transaction`, `end_transaction` |
| Switch play state | `context.play()`: `start()`, `stop()`, `pause()`, `resume()`, `step(n, reply)` |
| Pick what is under a viewport point | `gather_pick_candidates(scene)`, `pick(ray, candidates, context, mode)` |
| Save a screenshot as lossless JPEG XL instead of PNG | `view.screenshot` with a `.jxl` path or `"format": "jxl"`; `screenshot_request::format = screenshot_format::JPEG_XL` from C++ |
<!-- doc-content:at_a_glance:end -->

## Variables and Properties

<!-- doc-content:variables:start -->
Types, fields, and state are described in the reference sections below.
<!-- doc-content:variables:end -->

<!-- doc-content:body:start -->
## The process object

`editor_options` is the parsed command line (`parse_editor_options(args)`): `projectPath`, `execFile`,
`outputFile`, `exportDirectory`, `exportDistribution` (`--distribution <dir>`, only with `--export`: the folder
`conjure_runtime`, `conjure_shared`, `shaders/` and `THIRD_PARTY_NOTICES.txt` are copied from, with
`nethost.dll` and the Visual C++ runtime DLLs when it holds them; without it the export holds the pack and the
project files only and its reply warns that the runtime was not copied; a folder without the notices, or on
Windows without the runtime DLLs, exports with a warning, and debug runtime DLLs in it are left behind with a
warning), `aiToken` and `aiInspectToken`
(`--ai-token <t>`, `--ai-inspect-token <t>`; left empty, the server generates the tokens for its TCP port; an
empty value on the command line is refused, and so is the same text for both; see
[Access tokens and the audit log](access-and-audit.md#who-may-connect-and-what-a-client-may-do)), `aiPort`
(`0` disables the server), `timeoutSeconds` (a positive
finite number; `0` is refused), `headless`, `offscreen`, `keepGoing`, `aiStdio`, `noScripting`, `dotnetHost`
(`--dotnet`: a headless session keeps the real.NET script host), `trustProject` and `trustStoreFile`
(`--trust-project`, which needs `--project`, and `--trust-store <file>`; see [Project trust](#project-trust)),
`showHelp` and `engineArguments` (everything the
editor did not recognise goes to the engine's parser; an option the engine does not know either fails with
`engine_error::BAD_ARGUMENTS`, exit code 3, before the project is loaded). `--scripting` is one of those engine
flags: a headless session already runs scripting (null host, or.NET with `--dotnet`), so it only changes a
windowed session of a project whose `scripting.enabled` is false; together with `--no-scripting` it is
`engine_error::BAD_ARGUMENTS`, exit code 3. `editor_options_help()` is the usage
text `--help` prints: the editor's options followed by the engine's (`engine::command_line_help()`, without the
`--project`, `--headless`, `--no-scripting` and `--help` lines the editor describes itself, and without the
engine's note that unknown options are passed on). `stdout_carries_protocol()` is true for `--ai-stdio`, `--exec`
without `--out` and `--export`; `conjure_editor` then sends every log line to standard error.
`editor_application::initialise(options, uiFactory)` builds the
engine (headless: null platform, rhi and audio, the script runtime on the null host so `script.*` works without
.NET), adds
`bindings::scripting_bindings_subsystem` before `initialise()` so every native API slot the C# facades call is
filled (📘 [The native table](../bindings/native-table.md)),
creates the context, pushes the frame layer and, with a factory, the UI layer, and starts the protocol server.
Before the engine starts it checks what needs none: the arguments, `project.json` and the merged configuration,
then the command file of `--exec`, which it reads and parses (`headless_runner::load_command_file`). A missing
or malformed file fails `initialise` with `engine_error::BAD_ARGUMENTS` (exit code 3) without a window, a device
or an asset scan. The last of these checks is whether the project may be opened at all: a session nobody can
be asked in fails with `PROJECT_NOT_TRUSTED` (exit code 2) for a project nobody answered for
([Project trust](#project-trust)). `startup_scene_warning()` is the `{code: "STARTUP_SCENE_FAILED", message}` of a startup scene
the engine could not load (`engine::application::startup_scene_failure()`), which an `--exec` output opens with
(📘 [The AI control protocol](ai-protocol.md#the-headless-runner)).
`run()` exports (`--export`), executes (`--exec`) or loops frames; `pump_frame()` runs one engine frame and
applies a pending `project.open`; `request_open_project(dir)` restarts the engine on another project at the next
frame boundary (the AI port file moves with it; `PROJECT_NOT_TRUSTED`, and no restart, for a project nobody
answered for), stopping a play session first, and `play.start` answers `BUSY`
while such a restart is pending; `make_engine_desc(options, projectPath)` shows the merged engine
configuration. A headless `--ai-stdio` session without a TCP server ends `run()` with 0 once standard input
ends.

```cpp
editor::editor_options options = {};
options.headless = true;
options.aiPort = u16{0};
options.projectPath = scratch.path();
options.trustProject = true;        // your own project: answered for this run, remembered nowhere
options.engineArguments = {"--log-level", "warn"};
editor::editor_application app = {};
CONJURE_TRY(app.initialise(options, {}));

editor::protocol_request request = {};
request.id = editor::make_id(i64{1});
request.command = "entity.create";
request.args = core::json::object({{"name", "Player"}});
const std::optional<editor::protocol_response> response = app.context().dispatcher().dispatch(request);
// a synchronous command answers at once; an asynchronous one answers through the client's response sink
```

> [!IMPORTANT]
> `--offscreen` asks for a real device without a window so `view.screenshot` has pixels. The engine configuration
> refuses that today (headless rendering means the null device), the editor logs a warning and falls back to the
> null device, and `view.screenshot` answers `NOT_AVAILABLE`.

## Project trust

Opening a project runs code from its folder with your rights: the engine loads the project's native modules and
its game assembly while it starts, and `script.compile`, `module.build` and `play.start` run more of it. The
editor therefore opens a project only after somebody answered for it. There is one gate, at the opening, and an
untrusted project is not opened at all; there is no mode that opens it without its code.

| Where | A project nobody answered for | The explicit answer |
|---|---|---|
| The editor window | the editor starts without it and asks "Do you trust the authors of this project?" | *Trust and Open*, or *Don't Open* (nothing is recorded) |
| `--headless`, `--exec`, `--export` | `initialise` fails with `PROJECT_NOT_TRUSTED` before the engine starts (exit code 2) | `--trust-project` for this run, or an earlier `project.trust` |
| `project.open` | `PROJECT_NOT_TRUSTED` with `details {path, state, prompt}`; nothing is scheduled, and a window asks the person at it | `project.trust {path}` first |
| `request_open_project(dir)` | `PROJECT_NOT_TRUSTED` | `application.trust().trust(dir, remember, origin)` |

`project_trust` keeps the answers; `editor_application::trust()` is the one the application asks, and
`editor_context::trust()` points at it. `trust(root, remember, origin)` records an answer and returns the
`trust_state` the project now has: `REMEMBERED` when the trust store took it, `SESSION` when it holds until the
editor exits (not asked to remember, no store, or a store that could not be written or did not show the answer
right after it was written, which is a WARN and not an error). `state_of(root)` and `is_trusted(root)` ask, `revoke(root)` takes an answer back, `remembered()` lists the
store. A project is `project_key(root)`: the resolved absolute path of its root folder.

The trust store is one JSON file per user of the machine, outside every project, so that a project cannot bring
its own answer. `default_trust_store_file()` names it: `trusted_projects.json` under `%LOCALAPPDATA%/Conjure`,
under `$XDG_STATE_HOME/conjure` or `~/.local/state/conjure`, or under `~/Library/Application Support/Conjure`.
`--trust-store <file>` (`editor_options::trustStoreFile`) names another file. Only `conjure_editor` fills in the
default: an `editor_application` you start with an empty `trustStoreFile` remembers nothing, so a test or a tool
never writes to the user's folder. The file is written for its owner alone and is ignored, with a WARN, when
another account could have written it, when it does not parse or when it is larger than 4 MiB; it keeps the
newest 1024 answers. That check and the read are two steps on the path, so a file swapped in between them is
believed: keep the store in a folder other accounts cannot write to (the default folder is one).

The commands are `project.trust {path, remember?}`, `project.revoke_trust {path}` and `project.trust_state
{path?}` (the last one is `read_only`; the first two cannot run in a batch). `project.create` trusts the project
it writes into a folder that did not exist (`result.trust`); a project created in an existing folder, even an
empty one, is asked about like any other. The window's question is `editor_context::trust_prompt()`, which
`editor.state` reports as `trustPrompt`; `raise_trust_prompt`, `dismiss_trust_prompt` and `close_trust_prompt_for`
move it, and `editor_application::can_ask_about_trust(options)` says whether a session has a window to ask in.

> [!IMPORTANT]
> A script that runs `conjure_editor --headless --project <dir> ...` must pass `--trust-project`, or trust the
> project once with `project.trust`. Without an answer the run exits with 2 and says so on standard error.

> [!WARNING]
> Trust belongs to the folder's path, not to its files. What is later put into a trusted folder runs without a
> question, a project that is moved or copied is asked about again, and revoking does not close the project that
> is open. A command file is commands: one that came with a project can answer for it, so read it before you
> pass it to `--exec` or `editor.exec_file`.

## The service table

`editor_context` holds references into the running engine (`engine()`, `engine_context()`, `scene()`,
`scene_manager()`, `world()`, `components()`, `filesystem()`, `assets()`, `asset_database()`, `input()`,
`renderer()`, `physics()`, `scripting()`, `modules()`, `frame_index()`; the pointers are null for a disabled
module) and owns the editor's own services: `registry()`, `dispatcher()`, `undo()`, `events()`, `console()`,
`selection()`, `camera()`, `play()`, `screenshots()`, `module_consumer()`; `trust()` is the application's
`project_trust` (null for a context built over a bare engine). `has_project()`, `has_scene()`,
`scene_path()`, `scene_dirty()`, `headless()`, `offscreen()` describe the state; `rebind(application)` follows a
project restart. `normalise_scene_path(text)` turns a scene argument into the canonical asset path the database
registers (forward slashes, no `.` or `..` segments, `.scene.json` matched without case) and answers `""` for a
path that leaves its mount, which `scene.new`, `scene.open` and `scene.save_as` refuse with `INVALID_ARGS`.
`is_open_scene(id)` says whether an asset is the file behind the open scene and `names_asset(path, id)` whether
an asset path names an asset's source file; both compare files, not spellings, so a scene opened as
`Scenes/Main` on a volume that ignores case is the asset registered as `scenes/main`.
`viewport_aspect()` is the width / height the viewport panel last reported (16:9 until then, and headless):
`view.pick` builds its ray with it when the request names no `aspect`. `script_compile_pending()` is true while a
`script.compile` runs (a second one answers `BUSY`), and `last_script_configuration()` names the configuration of
the last successful compile, whose `scripts/bin/<configuration>/Game.dll` `script.reload` loads by default. Main
thread only.

## The command model

A `command_descriptor` is `name`, `summary`, `argsSchema` and `resultSchema` (a hand-validated JSON Schema
subset: `type`, `properties`, `required`, `additionalProperties`, `enum`, `items`, `minimum`, `maximum`),
`flags` (`MUTATES`, `REQUIRES_PROJECT`, `REQUIRES_SCENE`, `EDITING_ONLY`, `PLAYING_ONLY`, `ASYNC`, `UI_ONLY`,
`NOT_IN_BATCH`, `NO_UNDO`, `ON_DISK`, `READ_ONLY`), `owner` and the `command_handler`. The handler receives a `command_invocation`
(context, args, clientId, an `async_reply` for `ASYNC` commands) and returns a `command_outcome`: `result`,
the `inverse` (mandatory for `MUTATES` without `NO_UNDO`; `skipRecording` when nothing undoable happened), an
optional normalised `replay` and an undo `label`. `builtin_command_count()` says how many the editor registers
(108); `command_registry` adds, finds, lists (`names()`, `closest_names(name, count)` for `didYouMean`, empty
for a name over 128 bytes) and describes commands (`describe_all` is `editor.schema`; `schema_hash()` the
`schemaHash`, recomputed on first use after a change).

`command_dispatcher::dispatch(request)` runs the pipeline: unknown name, the scope of the client's token
(`UNAUTHORISED` when `required_access(flags)` is above what the client was granted), schema validation,
preconditions, handler, undo recording, revision, events, and the audit record of a transport client's request. `execute(command_request)` is the in-process form that returns the
result document; `run_batch` runs children atomically (the rollback works while playing and when the batch is
itself an undo or redo step, too); `resolve_completed` delivers finished asynchronous replies once per frame;
`cancel_client` / `cancel_all` answer pending requests with `CANCELLED`; `set_async_timeout_seconds`,
`set_max_pending_per_client` and `set_clock` tune the queue. `dispatch_inverses` / `dispatch_replays` apply an
undo or redo step without recording, bumping the revision or emitting `scene.changed`: `history.undo` / `redo` do
that once per step. `replaying()` is true meanwhile (and during a batch rollback), so a handler can accept a value
an earlier state held that a client may not send, such as a legacy entity name.

Entity names must be addressable by a hierarchy path: `entity.create` (`args.name`), `entity.create_many`
(`args.nameFormat`), `entity.rename`, `prefab.instantiate` (`args.name`) and a write of the `name` component
(`component.set`, `component.set_many`, `entity.create`'s `components.name`) refuse the empty name and one holding
`/` with `INVALID_ARGS`. Undo still restores such a name when a scene file or an import gave it. Identical names
are allowed; an ambiguous path answers with `details.candidates`.

A write of the `layer` component (`component.set`, `component.add`'s `value`, `entity.create`'s `components.layer`)
goes through `scene::set_layer` and refuses an index of 32 or more with `INVALID_ARGS`, leaving the component as it
was: every capture (`play.start`'s snapshot, `scene.save`, `entity.duplicate`, a prefab) refuses such an index. The
`0..31` range `reflection.components` reports for the field is otherwise only an inspector hint; the write path does
not enforce reflected ranges.

A component path whose leading segments spell a full registered name (`transform.rig.weight` with a registered
`transform.rig`) addresses that type, even when its first segment is an alias or another type's short name; the
inspector always sends the registered name.

> [!WARNING]
> A `MUTATES` handler that returns neither an inverse nor `skipRecording` is an `INTERNAL` error in the log:
> the command ran, but it cannot be undone. Module commands cannot return inverses, so their `MUTATES` is
> registered as `MUTATES | NO_UNDO`.

Flag a command `ON_DISK` when it is undoable and its effect lives in the project's files, not in the scene.
`play.stop` restores the scene and nothing on disk, so while playing the dispatcher hands the inverse and replay
of such a command to the play controller instead of dropping them, and `play.stop` makes one undo entry of them.
The flag is a promise about both requests: neither the inverse nor the replay may refer to scene state, because
they run after the restore. `command_registry::add` answers `INVALID_ARGS` for `ON_DISK` without `MUTATES` or
together with `NO_UNDO`, and a module command loses the flag (it has no inverse to keep). A command that writes
a file *and* changes the scene so that its inverse touches both is `EDITING_ONLY` instead, as `prefab.create` is.

Flag a command `READ_ONLY` when a client that holds only the inspect token may run it
([Two tokens](access-and-audit.md#two-tokens-full-control-and-inspect-only)). The flag is a promise: the handler
changes nothing clients share (the scene, a file, the selection, the camera, the play state, the console, the
history), starts no program and loads no code; what belongs to the calling client alone, such as its event
subscriptions, is its own. `required_access(flags)` is the whole rule (`INSPECT` with the flag, `FULL_CONTROL`
without), so a command without the flag is closed to inspect-only clients by default. `command_registry::add`
answers `INVALID_ARGS` for `READ_ONLY` together with `MUTATES`, and a module command that says both keeps
`MUTATES` and loses `READ_ONLY`. The editor's own callers (the UI, `--exec`, `editor.exec_file`) always have
full control.

## Undo, selection, play

`undo_stack` keeps `undo_entry { label, undo[], redo[], coalesceKey, clientId }` up to 1024 entries and 256 MiB
of serialised arguments: `record(...)` and `push(entry)`, `begin_transaction(label, clientId)` /
`end_transaction` group a client's commands, `has_transaction(clientId)` / `has_open_transaction()` (any client)
report them, `pop_for_undo()` / `pop_for_redo()` hand the inverses and replays to the dispatcher,
`set_frozen(true)` while playing, `can_undo()`, `can_redo()`, `cursor()`, `size()`, `clear()`. Consecutive
single-command entries with the same coalesce key and client merge (the inspector's drags). `history.undo`,
`history.redo` and `history.clear` answer `INVALID_REQUEST` while any transaction is open: ending it would push
its entry at the cursor and drop the redo tail.

`selection_set` holds guids (`set`, `add`, `remove`, `clear`, `contains`, `primary()`) and a dense entity bitmap
rebuilt from the scene (`rebuild(scene)`) for O(1) `contains(entity)` while drawing. `guids()` is sorted;
`primary()` (the inspector's entity, `selection.get`'s `primary`) is the guid named last in the `set` or `add`
call that last changed it, so `selection.set {"entities":[a, b]}` makes `b` primary; when the primary leaves
(`remove`, `rebuild`) the lowest remaining guid takes over, and `play.stop` restores it with the selection.
`editor_context::selection_payload()` builds `{guids, primary}` with `primary` null when nothing is selected.
Every `selection.changed` event carries exactly that (`editor_context::emit_selection_changed()` sends it, for
the selection's `on_changed` and for `play.stop`), and `selection.get` / `set` / `clear` answer it plus
`entities` and `count`.

`play_controller` maps the three editor states onto the engine: `EDITING` keeps the engine `PAUSED` and the
script runtime in `EDIT` mode; `start()` snapshots the scene, freezes undo and runs; `pause()` / `resume()`;
`step(fixedSteps, reply)` runs exactly `n` fixed steps and answers when they ran; `stop()` restores the snapshot
(guids preserved), remaps the selection and thaws undo. `state()`, `describe()`, `to_string(play_state)`.
While the stack is frozen the dispatcher reports each recorded edit to the controller: `note_unrecorded_edit()`
for a scene edit (not recorded, discarded by the restore, warning `UNRECORDED_PLAY_EDIT` once per session) and
`keep_file_edit(command, replay, inverse)` for an `ON_DISK` command (warning `PLAY_FILE_EDIT` once per session).
`kept_file_edits()` counts what the session holds, and `stop()` pushes it as one undo entry labelled
`play_controller::FILE_EDITS_LABEL` ("File changes during play"), whichever way the session ends.
The clocks of the play session are `simulation_frame()`, `simulation_time()` and `simulation_fixed_steps()`:
`start()` sets them to 0 by recording the engine's `fixedTime` and `fixedStepIndex`, the time and the fixed steps
advance with fixed steps only (so they stand still while paused), and after `stop()` they keep the ended
session's values until the next `start()`. `describe()` reports them as `simulationFrame`, `simulationTime` and
`fixedStepIndex` beside `state` and the engine `frame`.

## Picking, screenshots, camera, console

`pick(ray, candidates, context, mode)` is CPU picking over `pick_candidate { guid, worldBounds, entity }` from
`gather_pick_candidates(scene)` (active entities only: a disabled entity, or one under a disabled ancestor, is not
pickable): a slab test over world bounds sorted by entry distance, then (`pick_mode::MESH`) the triangles of the
mesh asset's CPU geometry, `BOUNDS` for bounds only, `PHYSICS` through Jolt; `pick_context::maxDistance` bounds
both the bounds entry and the triangle hit. The result is `pick_result { guid, point, normal, distance, hit }` with
`to_json()`. `editor_camera` is the viewport camera:
`set_look_at`, `focus(aabb)`, `orbit`, `pan`, `zoom`, perspective or orthographic, `screen_ray(uv, aspect)`,
`world_units_per_pixel(viewportHeight)` (the visible height at the target over the viewport's pixels, so a pan
follows the cursor at any zoom), `to_json()` / `apply_json()` (what `view.get_camera` / `view.set_camera`
exchange, `fov` in degrees). The viewport keeps orbiting or panning while a drag that started on it continues
outside the panel.
`screenshot_service` parks a `screenshot_request { path, camera, format, width, height, waitFrames }`, captures
through a `screenshot_backend` on the main thread after the wait and encodes the file on a job. The renderer
backend draws the scene through the requested camera into a render target of the requested size and reads it
back; a frame counts as waited only once the engine has rendered one for the request
(`screenshot_backend::frame_rendered`), so the capture is never an empty target. Pictures are taken one at a
time, in request order: a request made while another waits is begun when its turn comes. Tests install
`memory_screenshot_backend`, and headless there is no backend (`available()` false, `NOT_AVAILABLE`).
`view.screenshot`'s `camera` is `"editor"`, `"game"` or an entity by guid, path or name that must carry a camera
component (`COMPONENT_NOT_FOUND` otherwise); the service receives its guid.

A screenshot is a PNG unless it asks for JPEG XL (`screenshot_format`, wire names `"png"` and `"jxl"`):

| `view.screenshot` arguments | File written |
|---|---|
| `path` ends in `.jxl` (any case), no `format` | lossless JPEG XL |
| any other `path` (`.png`, another extension, none), no `format` | PNG, the default: golden images and review tools read it |
| `"format": "jxl"` or `"png"` with a path whose extension names no format (`shot.bin`) | that format |
| `format` contradicting a `.png` / `.jxl` extension, or any other `format` | `INVALID_ARGS` naming `args.format` |

`screenshot_format_of(path)` is the extension rule and `screenshot_service::parse_request` applies the table; the
answer carries `format`, so a client knows what it got. A JPEG XL screenshot holds exactly the pixels of the PNG one
(sRGB, alpha kept) and is encoded at effort 2 by the renderer's `encode_jpeg_xl` (📘 API:
[JPEG XL images](../renderer/jpeg-xl.md)): faster than the PNG encode and usually a third to a half of its size. A
build without the renderer module cannot write it: `screenshot_service::supports(format)` is false and the request
answers `NOT_AVAILABLE`.
`console_buffer` is the log ring the `console.read` command and the console panel read (`install_sink()`,
`read(console_query)`, `clear()`): `read` returns the oldest matches up to `limit`, or with `console_query::newest`
the newest ones (still oldest first), which is what the panel draws so new lines keep appearing past its limit.

## Addressing and events

`entity_ref::resolve(json, scene)` accepts a guid string, a hierarchy path or a unique name; `resolve_many` an
array; `guid_of(scene, entity)` generates and indexes a guid where missing; `path_of` and `to_json` go the other
way. `reflection_path::parse("transform.position.x")` splits a component (an alias, a unique registered-name
suffix or the full name) from field segments and array indices; `resolve(typeInfo)` yields a `field_ref` (offset,
field, `is_whole_component()`), `read_at` / `write_at` move JSON in and out of a component through the reflection
description. `event_broadcaster` fans `protocol_event`s out to clients (`register_client`, `set_subscriptions`,
`set_min_log_level`, `emit`, `make_event`); the `editor_event` enumeration names the fifteen events.

## What a module adds

A plugin registers `command_desc { name, summary, argsSchema, resultSchema, flags, handler, userData }` and
`panel_desc { title, draw, userData, openByDefault }` records through its `module_registrar`;
`editor_module_consumer` validates them (unique names, a parseable schema), attaches commands into the
registry with `owner = <module>` and panels into the workspace, vetoes `module.unload` while playing, and strips
both on detach. The module name `editor` is reserved (the owner of the built-in commands and panels): such a
module fails validation with `REGISTRATION_CONFLICT`, and `module.list` counts commands only for a loaded module.
`module.*` commands only enqueue requests; the module manager applies them between frames.

> [!NOTE]
> The editor loads its static modules before the consumer exists, so `editor_context::install_module_consumer()`
> unloads, adds the consumer and loads again at start-up; a pre-load hook on the modules subsystem is a recorded
> engine request.
<!-- doc-content:body:end -->

## Functions

<!-- doc-content:functions:start -->
<a id="api-summary"></a>

| Signature (short) | Purpose |
|---|---|
| `result<editor_options> parse_editor_options(span<const string_view>)` / `std::string editor_options_help()` / `bool editor_options::stdout_carries_protocol()` | the editor command line |
| `result<void> editor_application::initialise(const editor_options &, layer_factory)` / `int run()` / `result<void> pump_frame()` / `void shutdown()` | the process object |
| `editor_context &editor_application::context()` / `engine::application &engine()` / `protocol_server &server()` | its parts |
| `project_trust &editor_application::trust()` / `static bool can_ask_about_trust(const editor_options &)` | the answers the application opens projects by, and whether a session has a window to ask in |
| `static result<startup_project> resolve_startup_project(const editor_options &, project_trust &)` / `static result<startup_plan> plan_startup(...)` | what `initialise` does about `--project`: open it, leave it out for the window to ask about (`unanswered`), or `PROJECT_NOT_TRUSTED`; the plan adds the engine description, which holds no project that was left out |
| `result<trust_state> project_trust::trust(const path &, bool remember, string_view origin)` / `result<bool> revoke(const path &)` / `trust_state state_of(const path &)` / `bool is_trusted(const path &)` / `vector<trusted_project> remembered()` | answering, taking back and asking |
| `static result<string> project_trust::project_key(const path &)` / `void set_store_file(path)` / `result<path> default_trust_store_file()` | what a project is remembered under, and where |
| `bool editor_context::raise_trust_prompt(const path &)` / `const optional<path> &trust_prompt()` / `void dismiss_trust_prompt()` / `void close_trust_prompt_for(const path &)` | the window's question |
| `optional<json::document> editor_application::startup_scene_warning()` | `{code: "STARTUP_SCENE_FAILED", message}` when the engine started without the startup scene |
| `optional<protocol_response> command_dispatcher::dispatch(const protocol_request &)` / `result<json::document> execute(const command_request &)` | running a command |
| `constexpr access_level required_access(command_flags)` | what a client must have been granted to run a command: `INSPECT` for `READ_ONLY`, `FULL_CONTROL` otherwise |
| `result<void> command_registry::add(command_descriptor)` / `json::document describe_all(bool)` / `vector<string> closest_names(string_view, usize)` | the catalogue |
| `result<compiled_schema> compile_schema(string_view)` / `result<void> validate_args(const compiled_schema &, const json::value &)` | argument validation |
| `optional<undo_entry> undo_stack::pop_for_undo()` / `pop_for_redo()` / `result<void> begin_transaction(string_view, u32)` / `end_transaction(u32)` / `bool has_open_transaction()` | undo |
| `bool command_dispatcher::replaying()` | an undo or redo step (or a batch rollback) is being applied |
| `f32 editor_camera::world_units_per_pixel(f32)` / `f32 editor_context::viewport_aspect()` | viewport scale and shape |
| `bool editor_context::is_open_scene(const assets::asset_id &)` / `names_asset(string_view, const assets::asset_id &)` | whether an asset is the open scene's file, or the file an asset path names |
| `result<void> play_controller::start()` / `stop()` / `pause()` / `resume()` / `step(u32, async_reply)` | play mode |
| `u64 play_controller::simulation_frame()` / `f64 simulation_time()` / `u64 simulation_fixed_steps()` | the play session's clocks |
| `void play_controller::keep_file_edit(string_view, command_request, command_inverse)` / `usize kept_file_edits()` / `void note_unrecorded_edit()` | edits made while playing |
| `pick_result pick(const math::ray &, span<const pick_candidate>, const pick_context &, pick_mode)` | picking |
| `void screenshot_service::request(const screenshot_request &, async_reply)` / `bool available()` | screenshots |
| `optional<screenshot_format> screenshot_format_of(const path &)` / `parse_screenshot_format(string_view)` / `string_view to_string(screenshot_format)` / `static bool screenshot_service::supports(screenshot_format)` | the screenshot file format |
| `static result<ecs::entity> entity_ref::resolve(const json::value &, scene::scene &)` | guid or path to entity |
| `static result<reflection_path> reflection_path::parse(string_view)` / `result<json::document> read_at(...)` / `result<void> write_at(...)` | field paths |
<!-- doc-content:functions:end -->

------------------------------------------

**See also:**

<!-- doc-content:related:start -->
- 📘 API: [The AI control protocol](ai-protocol.md), [The workspace UI](workspace-ui.md)
- 📘 API: [Writing a module](../modules/writing-a-module.md), [Reflection](../core/reflection.md), [Scene files, prefabs and snapshots](../scene/scene-files-and-prefabs.md)
- 🛠 Handbook: [Editor module](../../handbook/editor/editor.md)
<!-- doc-content:related:end -->

<!-- doc-legacy-anchors:start -->
<a id="see-also"></a>
<!-- doc-legacy-anchors:end -->

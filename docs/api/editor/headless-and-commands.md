# Headless mode and the command model

📘 **Public API wiki** · 🛠 Handbook: [Editor module](../../handbook/modules/editor.md)

The editor from the C++ side: the process object (`conjure/editor/editor_application.h`,
`conjure/editor/editor_options.h`), the service table (`conjure/editor/editor_context.h`), the command model
(`conjure/editor/command_dispatcher.h`, `conjure/editor/command_registry.h`, `conjure/editor/editor_command.h`,
`conjure/editor/builtin_commands.h`, `conjure/editor/schema_validator.h`), the services commands drive
(`conjure/editor/undo_stack.h`, `conjure/editor/selection_set.h`, `conjure/editor/play_controller.h`,
`conjure/editor/picker.h`, `conjure/editor/screenshot_service.h`, `conjure/editor/editor_camera.h`,
`conjure/editor/console_buffer.h`), addressing (`conjure/editor/entity_ref.h`, `conjure/editor/reflection_path.h`),
events (`conjure/editor/event_broadcaster.h`) and what a module adds (`conjure/editor/command_desc.h`,
`conjure/editor/panel_desc.h`, `conjure/editor/editor_module_consumer.h`).

## At a glance

| You want to... | Use |
|---|---|
| Run the editor headless from a test or tool | `editor_options { headless = true, projectPath, aiPort = 0 }`, `editor_application::initialise(options, {})`, `pump_frame()` |
| Send a command from C++ | `context.dispatcher().dispatch(request)` (a response, or `nullopt` for an asynchronous one that answers through a sink) |
| Register a command | `context.registry().add(command_descriptor)`; from a module, a `command_desc` extension record |
| Add a panel from a module | a `panel_desc` extension record; `editor_module_consumer` attaches it |
| Resolve `"Board/Cell_3"` or a guid to an entity | `entity_ref::resolve(json, scene)`, `entity_ref::guid_of`, `entity_ref::path_of` |
| Read or write one field of a component by path | `reflection_path::parse("transform.position.x")`, `read_at`, `write_at` |
| Undo and redo programmatically | `context.undo()`: `pop_for_undo()`, `pop_for_redo()`, `begin_transaction`, `end_transaction` |
| Switch play state | `context.play()`: `start()`, `stop()`, `pause()`, `resume()`, `step(n, reply)` |
| Pick what is under a viewport point | `gather_pick_candidates(scene)`, `pick(ray, candidates, context, mode)` |

## The process object

`editor_options` is the parsed command line (`parse_editor_options(args)`): `projectPath`, `execFile`,
`outputFile`, `exportDirectory`, `exportDistribution` (`--distribution <dir>`, only with `--export`: the folder
`conjure_runtime`, `conjure_shared` and `shaders/` are copied from; without it the export holds the pack and the
project files only and its reply warns that the runtime was not copied), `aiToken`, `aiPort` (`0` disables the
server), `timeoutSeconds` (a positive
finite number; `0` is refused), `headless`, `offscreen`, `keepGoing`, `aiStdio`, `noScripting`, `dotnetHost`
(`--dotnet`: a headless session keeps the real .NET script host), `showHelp` and `engineArguments` (everything the
editor did not recognise goes to the engine's parser; an option the engine does not know either fails with
`engine_error::BAD_ARGUMENTS`, exit code 3, before the project is loaded). `editor_options_help()` is the usage
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
`run()` exports (`--export`), executes (`--exec`) or loops frames; `pump_frame()` runs one engine frame and
applies a pending `project.open`; `request_open_project(dir)` restarts the engine on another project at the next
frame boundary (the AI port file moves with it), stopping a play session first, and `play.start` answers `BUSY`
while such a restart is pending; `make_engine_desc(options, projectPath)` shows the merged engine
configuration. A headless `--ai-stdio` session without a TCP server ends `run()` with 0 once standard input
ends.

```cpp
editor::editor_options options = {};
options.headless = true;
options.aiPort = u16{0};
options.projectPath = scratch.path();
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

## The service table

`editor_context` holds references into the running engine (`engine()`, `engine_context()`, `scene()`,
`scene_manager()`, `world()`, `components()`, `filesystem()`, `assets()`, `asset_database()`, `input()`,
`renderer()`, `physics()`, `scripting()`, `modules()`, `frame_index()`; the pointers are null for a disabled
module) and owns the editor's own services: `registry()`, `dispatcher()`, `undo()`, `events()`, `console()`,
`selection()`, `camera()`, `play()`, `screenshots()`, `module_consumer()`. `has_project()`, `has_scene()`,
`scene_path()`, `scene_dirty()`, `headless()`, `offscreen()` describe the state; `rebind(application)` follows a
project restart. `normalise_scene_path(text)` turns a scene argument into the canonical asset path the database
registers (forward slashes, no `.` or `..` segments, `.scene.json` matched without case) and answers `""` for a
path that leaves its mount, which `scene.new`, `scene.open` and `scene.save_as` refuse with `INVALID_ARGS`.
`viewport_aspect()` is the width / height the viewport panel last reported (16:9 until then, and headless):
`view.pick` builds its ray with it when the request names no `aspect`. `script_compile_pending()` is true while a
`script.compile` runs (a second one answers `BUSY`), and `last_script_configuration()` names the configuration of
the last successful compile, whose `scripts/bin/<configuration>/Game.dll` `script.reload` loads by default. Main
thread only.

## The command model

A `command_descriptor` is `name`, `summary`, `argsSchema` and `resultSchema` (a hand-validated JSON Schema
subset: `type`, `properties`, `required`, `additionalProperties`, `enum`, `items`, `minimum`, `maximum`),
`flags` (`MUTATES`, `REQUIRES_PROJECT`, `REQUIRES_SCENE`, `EDITING_ONLY`, `PLAYING_ONLY`, `ASYNC`, `UI_ONLY`,
`NOT_IN_BATCH`, `NO_UNDO`), `owner` and the `command_handler`. The handler receives a `command_invocation`
(context, args, clientId, an `async_reply` for `ASYNC` commands) and returns a `command_outcome`: `result`,
the `inverse` (mandatory for `MUTATES` without `NO_UNDO`; `skipRecording` when nothing undoable happened), an
optional normalised `replay` and an undo `label`. `builtin_command_count()` says how many the editor registers
(105); `command_registry` adds, finds, lists (`names()`, `closest_names(name, count)` for `didYouMean`, empty
for a name over 128 bytes) and describes commands (`describe_all` is `editor.schema`; `schema_hash()` the
`schemaHash`, recomputed on first use after a change).

`command_dispatcher::dispatch(request)` runs the pipeline: unknown name, schema validation, preconditions,
handler, undo recording, revision, events. `execute(command_request)` is the in-process form that returns the
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
through a `screenshot_backend` on the main thread after the wait and encodes the PNG on a job. The renderer
backend draws the scene through the requested camera into a render target of the requested size and reads it
back; a frame counts as waited only once the engine has rendered one for the request
(`screenshot_backend::frame_rendered`), so the capture is never an empty target. Pictures are taken one at a
time, in request order: a request made while another waits is begun when its turn comes. Tests install
`memory_screenshot_backend`, and headless there is no backend (`available()` false, `NOT_AVAILABLE`).
`view.screenshot`'s `camera` is `"editor"`, `"game"` or an entity by guid, path or name that must carry a camera
component (`COMPONENT_NOT_FOUND` otherwise); the service receives its guid.
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

## API summary

| Signature (short) | Purpose |
|---|---|
| `result<editor_options> parse_editor_options(span<const string_view>)` / `std::string editor_options_help()` / `bool editor_options::stdout_carries_protocol()` | the editor command line |
| `result<void> editor_application::initialise(const editor_options &, layer_factory)` / `int run()` / `result<void> pump_frame()` / `void shutdown()` | the process object |
| `editor_context &editor_application::context()` / `engine::application &engine()` / `protocol_server &server()` | its parts |
| `optional<protocol_response> command_dispatcher::dispatch(const protocol_request &)` / `result<json::document> execute(const command_request &)` | running a command |
| `result<void> command_registry::add(command_descriptor)` / `json::document describe_all(bool)` / `vector<string> closest_names(string_view, usize)` | the catalogue |
| `result<compiled_schema> compile_schema(string_view)` / `result<void> validate_args(const compiled_schema &, const json::value &)` | argument validation |
| `optional<undo_entry> undo_stack::pop_for_undo()` / `pop_for_redo()` / `result<void> begin_transaction(string_view, u32)` / `end_transaction(u32)` / `bool has_open_transaction()` | undo |
| `bool command_dispatcher::replaying()` | an undo or redo step (or a batch rollback) is being applied |
| `f32 editor_camera::world_units_per_pixel(f32)` / `f32 editor_context::viewport_aspect()` | viewport scale and shape |
| `result<void> play_controller::start()` / `stop()` / `pause()` / `resume()` / `step(u32, async_reply)` | play mode |
| `u64 play_controller::simulation_frame()` / `f64 simulation_time()` / `u64 simulation_fixed_steps()` | the play session's clocks |
| `pick_result pick(const math::ray &, span<const pick_candidate>, const pick_context &, pick_mode)` | picking |
| `void screenshot_service::request(const screenshot_request &, async_reply)` / `bool available()` | screenshots |
| `static result<ecs::entity> entity_ref::resolve(const json::value &, scene::scene &)` | guid or path to entity |
| `static result<reflection_path> reflection_path::parse(string_view)` / `result<json::document> read_at(...)` / `result<void> write_at(...)` | field paths |

## See also

- 📘 API: [The AI control protocol](ai-protocol.md), [The workspace UI](workspace-ui.md)
- 📘 API: [Writing a module](../modules/writing-a-module.md), [Reflection](../core/reflection.md), [Scene files, prefabs and snapshots](../scene/scene-files-and-prefabs.md)
- 🛠 Handbook: [Editor module](../../handbook/modules/editor.md)

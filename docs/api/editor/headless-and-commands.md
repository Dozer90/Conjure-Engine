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
`outputFile`, `exportDirectory`, `aiToken`, `aiPort` (`0` disables the server), `timeoutSeconds`, `headless`,
`offscreen`, `keepGoing`, `aiStdio`, `noScripting`, `dotnetHost` (`--dotnet`: a headless session keeps the real
.NET script host), `showHelp` and `engineArguments` (everything the editor did not recognise goes to the engine's
parser). `editor_application::initialise(options, uiFactory)` builds the engine (headless: null platform, rhi and
audio, the script runtime on the null host so `script.*` works without .NET), adds
`bindings::scripting_bindings_subsystem` before `initialise()` so every native API slot the C# facades call is
filled (📘 [The native table](../bindings/native-table.md)),
creates the context, pushes the frame layer and, with a factory, the UI layer, and starts the protocol server.
`run()` exports (`--export`), executes (`--exec`) or loops frames; `pump_frame()` runs one engine frame and
applies a pending `project.open`; `request_open_project(dir)` restarts the engine on another project at the next
frame boundary; `make_engine_desc(options, projectPath)` shows the merged engine configuration.

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
project restart. Main thread only.

## The command model

A `command_descriptor` is `name`, `summary`, `argsSchema` and `resultSchema` (a hand-validated JSON Schema
subset: `type`, `properties`, `required`, `additionalProperties`, `enum`, `items`, `minimum`, `maximum`),
`flags` (`MUTATES`, `REQUIRES_PROJECT`, `REQUIRES_SCENE`, `EDITING_ONLY`, `PLAYING_ONLY`, `ASYNC`, `UI_ONLY`,
`NOT_IN_BATCH`, `NO_UNDO`), `owner` and the `command_handler`. The handler receives a `command_invocation`
(context, args, clientId, an `async_reply` for `ASYNC` commands) and returns a `command_outcome`: `result`,
the `inverse` (mandatory for `MUTATES` without `NO_UNDO`; `skipRecording` when nothing undoable happened), an
optional normalised `replay` and an undo `label`. `builtin_command_count()` says how many the editor registers
(105); `command_registry` adds, finds, lists (`names()`, `closest_names(name, count)` for `didYouMean`) and
describes commands (`describe_all` is `editor.schema`; `recompute_hash()` the `schemaHash`).

`command_dispatcher::dispatch(request)` runs the pipeline: unknown name, schema validation, preconditions,
handler, undo recording, revision, events. `execute(command_request)` is the in-process form that returns the
result document; `run_batch` runs children atomically; `resolve_completed` delivers finished asynchronous replies
once per frame; `cancel_client` / `cancel_all` answer pending requests with `CANCELLED`;
`set_async_timeout_seconds`, `set_max_pending_per_client` and `set_clock` tune the queue.

> [!WARNING]
> A `MUTATES` handler that returns neither an inverse nor `skipRecording` is an `INTERNAL` error in the log:
> the command ran, but it cannot be undone. Module commands cannot return inverses, so their `MUTATES` is
> registered as `MUTATES | NO_UNDO`.

## Undo, selection, play

`undo_stack` keeps `undo_entry { label, undo[], redo[], coalesceKey, clientId }` up to 1024 entries and 256 MiB
of serialised arguments: `record(...)` and `push(entry)`, `begin_transaction(label, clientId)` /
`end_transaction` group a client's commands, `pop_for_undo()` / `pop_for_redo()` hand the inverses and replays to
the dispatcher, `set_frozen(true)` while playing, `can_undo()`, `can_redo()`, `cursor()`, `size()`, `clear()`.
Consecutive single-command entries with the same coalesce key and client merge (the inspector's drags).

`selection_set` holds guids (`set`, `add`, `remove`, `clear`, `contains`, `primary()`) and a dense entity bitmap
rebuilt from the scene (`rebuild(scene)`) for O(1) `contains(entity)` while drawing.

`play_controller` maps the three editor states onto the engine: `EDITING` keeps the engine `PAUSED` and the
script runtime in `EDIT` mode; `start()` snapshots the scene, freezes undo and runs; `pause()` / `resume()`;
`step(fixedSteps, reply)` runs exactly `n` fixed steps and answers when they ran; `stop()` restores the snapshot
(guids preserved), remaps the selection and thaws undo. `state()`, `describe()`, `to_string(play_state)`.

## Picking, screenshots, camera, console

`pick(ray, candidates, context, mode)` is CPU picking over `pick_candidate { guid, worldBounds, entity }` from
`gather_pick_candidates(scene)`: a slab test over world bounds sorted by entry distance, then (`pick_mode::MESH`)
the triangles of the mesh asset's CPU geometry, `BOUNDS` for bounds only, `PHYSICS` through Jolt; the result is
`pick_result { guid, point, normal, distance, hit }` with `to_json()`. `editor_camera` is the viewport camera:
`set_look_at`, `focus(aabb)`, `orbit`, `pan`, `zoom`, perspective or orthographic, `screen_ray(uv, aspect)`,
`to_json()` / `apply_json()` (what `view.get_camera` / `view.set_camera` exchange, `fov` in degrees).
`screenshot_service` parks a `screenshot_request { path, camera, format, width, height, waitFrames }`, captures
through a `screenshot_backend` on the main thread after the wait and encodes the PNG on a job; the renderer
backend reads a render target back, tests install `memory_screenshot_backend`, and headless there is no backend
(`available()` false, `NOT_AVAILABLE`). `console_buffer` is the log ring the `console.read` command and the console
panel read (`install_sink()`, `read(console_query)`, `clear()`).

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
both on detach. `module.*` commands only enqueue requests; the module manager applies them between frames.

> [!NOTE]
> The editor loads its static modules before the consumer exists, so `editor_context::install_module_consumer()`
> unloads, adds the consumer and loads again at start-up; a pre-load hook on the modules subsystem is a recorded
> engine request.

## API summary

| Signature (short) | Purpose |
|---|---|
| `result<editor_options> parse_editor_options(span<const string_view>)` / `std::string_view editor_options_help()` | the editor command line |
| `result<void> editor_application::initialise(const editor_options &, layer_factory)` / `int run()` / `result<void> pump_frame()` / `void shutdown()` | the process object |
| `editor_context &editor_application::context()` / `engine::application &engine()` / `protocol_server &server()` | its parts |
| `optional<protocol_response> command_dispatcher::dispatch(const protocol_request &)` / `result<json::document> execute(const command_request &)` | running a command |
| `result<void> command_registry::add(command_descriptor)` / `json::document describe_all(bool)` / `vector<string> closest_names(string_view, usize)` | the catalogue |
| `result<compiled_schema> compile_schema(string_view)` / `result<void> validate_args(const compiled_schema &, const json::value &)` | argument validation |
| `optional<undo_entry> undo_stack::pop_for_undo()` / `pop_for_redo()` / `result<void> begin_transaction(string_view, u32)` / `end_transaction(u32)` | undo |
| `result<void> play_controller::start()` / `stop()` / `pause()` / `resume()` / `step(u32, async_reply)` | play mode |
| `pick_result pick(const math::ray &, span<const pick_candidate>, const pick_context &, pick_mode)` | picking |
| `void screenshot_service::request(const screenshot_request &, async_reply)` / `bool available()` | screenshots |
| `static result<ecs::entity> entity_ref::resolve(const json::value &, scene::scene &)` | guid or path to entity |
| `static result<reflection_path> reflection_path::parse(string_view)` / `result<json::document> read_at(...)` / `result<void> write_at(...)` | field paths |

## See also

- 📘 API: [The AI control protocol](ai-protocol.md), [The workspace UI](workspace-ui.md)
- 📘 API: [Writing a module](../modules/writing-a-module.md), [Reflection](../core/reflection.md), [Scene files, prefabs and snapshots](../scene/scene-files-and-prefabs.md)
- 🛠 Handbook: [Editor module](../../handbook/modules/editor.md)

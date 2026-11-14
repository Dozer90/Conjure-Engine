# conjure_engine

The application and frame loop (`docs/design/06_engine_loop.md`, `ARCHITECTURE.md` 2.3/3/8/9): one object
that owns every module object, starts them in the documented order with reverse unwind on failure, runs the
exact frame sequence of 06 3.1 over the ECS scheduler phases and exposes the `engine_context` service table.
Headless is configuration, not a code path (D8): tests, servers, the editor's step button and the runtime all
go through `application::step_frame()`.

## Layout

| Area | Files | Design |
|---|---|---|
| Public types | `application`, `application_desc`, `command_line`, `engine_config`, `project`, `engine_context`, `engine_subsystem`, `layer`, `layer_stack`, `game_layer`, `imgui_layer` (editor), `debug_overlay_layer` (non-Distribution), `engine_time`, `fixed_step_clock`, `frame_memory`, `frame_stats`, `engine_phase`, `engine_error`, `exit_code`, `engine_version`, `export`, `diagnostics/check_registry` | 06 2, 4 |
| Frame loop | `frame_loop.{h,cpp}` (steps 1-16 of 06 3.1, the failure rules of 06 6), `frame_pacer.{h,cpp}` (06 3.3), `main_thread_queue.{h,cpp}` | 06 3.1, 3.3, 6 |
| Services | `engine_services.h` (every module object as a `unique_ptr`, declared in startup order), `executable_path.{h,cpp}` | 06 5 |
| Subsystem adapters | `subsystems/{platform,rhi,assets,ecs,scene,audio,physics,renderer,scripting,modules}_subsystem.{h,cpp}`, `subsystems/renderer_components.h`, `subsystems/audio_components.h` (the `CONJURE_COMPONENT` names of the renderer and audio components, R7), `subsystems/model_prefab_converter.{h,cpp}` | 06 2.9, 3.4; 09 section 3 |
| Module consumers | `module_consumers/{ecs,assets,scripting}_module_consumer.{h,cpp}`, `module_consumers/module_content_file_system.{h,cpp}` (`modules://<name>/`, R24), `builtin_modules.{h,cpp}` (manifest-only static modules, R18) | 07 D3/D6/4.1/9 |
| Export | `export_pipeline.cpp`, `export_commands.{h,cpp}` (the dotnet command lines of step 3) | 06 3.6, R21, R39 |

## Startup and frame

`application::initialise()` runs the twenty steps of 06 3.4: log sinks, `validate()`, the job system, frame
memory, the vfs mounts (`project://`, `assets://`, `engine://`, `cooked://`; `modules://` joins at step 15),
then the adapters in the order platform, rhi, assets, ecs, scene, audio, physics, renderer, scripting,
modules (the subsystems a module entry adds through the `application` service join right behind it, step
15), the user-added subsystems (step 16), every `post_startup` (the asset scan happens here, R33), every
`register_systems` + `scheduler::finalise()`, the layers' `on_attach` and the startup scene. A failure shuts
the started adapters down in reverse (detaching the layers first when step 20 failed) and the object stays
reusable (state `CREATED`). `shutdown()` is idempotent and follows 06 3.5: `jobs.wait_idle()`,
`device.wait_idle()`, `scene_manager::unload_all()`, `asset_manager::collect()`, layers, adapters in reverse,
core.

The asset database is built over the project file system with the `assets_root` directory as its `assets`
mount when that root lies inside the project; an absolute or parent-relative root is built over the dedicated
native file system `start_core()` mounts as `assets://` (so the vfs and the database agree), and the import
cache (`.conjure/cache`) then lives under that root.

`--scene` / `startupSceneOverride` is relative to the assets root unless it is already scheme-qualified
(`assets://scenes/x.scene.json`), which is used as given.

The polling file watcher and the `assets::hot_reload_controller` exist only when `assets_settings::hotReload`
is set (`engine_config::assets`, the project's `"assets": { "hot_reload": ... }` section; always off in
Distribution and for cooked projects). `headless_defaults()` turns it off, `application_desc::from_options`
copies that switch with the other headless defaults when `--headless` is given (so `conjure_runtime --headless`
and the editor's `--headless` / `--offscreen` modes are covered, not only configurations built from
`headless_defaults()` directly) and `project::apply_to` leaves it off under `headless`, so a headless frame never
schedules the watcher's 500 ms stat pass on a job thread. Schema 1 reserves the top-level `"assets"` section for
these settings: a `project.json` that used the key for its own data is no longer preserved in `extensions`, and
unknown members of the section are dropped on `save()` like those of every other section.

`frame_loop::run_frame()` is 06 3.1 verbatim. The scheduler receives one `ecs::frame_info` per phase built by
`make_frame_info(engine_time)`; `inFixedStep` is true only inside FIXED_UPDATE / PHYSICS_STEP / FIXED_END.
PAUSED runs UPDATE / LATE_UPDATE through `run_phase_filtered(RUNS_WHILE_PAUSED)`; STEP_ONCE and
`step_fixed(n)` leave the mode PAUSED afterwards (R13). A `step_fixed` request runs even while PAUSED, so
`cancel_fixed_steps()` drops one the next frame has not consumed (the editor's `play.stop`). EXTRACT runs only when
a renderer exists.

A startup scene whose source no longer imports (`asset_record_status::IMPORT_FAILED`, the cache keeps the old
blob) is `STARTUP_SCENE_FAILED` for the runtime; the editor loads the cached copy. `physics_subsystem` marks the
shape cache on `scene_manager::on_before_unload` and calls `physics_world::collect_shapes()` at the next
`end_frame`, after the unload destroyed the bodies (14_physics.md). `modules_subsystem` gives an `EDITOR` host
with a project shadow copies in `<project>/.conjure/module_cache` (07 D9); the runtime loads in place. The
folder is shared per project and numbered from 1, so on Windows only one editor per project can load a native
game module (open with the 07 D9 owner: a folder per editor instance). The scripting subsystem clears
`scripting_module_consumer`'s runtime pointer before it destroys the runtime.

`export_pipeline::run` refuses (`BAD_ARGUMENTS`) the project directory itself and any folder inside the assets
root as output, ships only `index.json` and the `.dxil` / `.spv` blobs of `shaders/` (subfolders included, `.d`
depfiles left out), and builds its dotnet command lines in `export_commands.{h,cpp}` (symbols stripped unless
`scripting.exportDebugSymbols`, no bootstrap apphost for a framework-dependent export).

## Allocation-free frames

`08_testing.md` 7.1 gates the three `engine.full_frame_10k_*` benchmarks (and the `headless_engine` integration
scenarios) at zero heap allocations per steady-state frame; the frame loop holds it in Development, inline and
with workers (performance round 2026-11, `09_cross_area_resolutions.md` 4 item 15a):

- `frame_loop::run_frame` drains the two main-thread queues only when they hold a task (the core drain builds a
  local task vector) and `layer_stack::apply_pending` returns before moving its pending vectors when nothing is
  queued (a moved-to `std::vector` allocates its container proxy under MSVC's debug iterators).
- `renderer_subsystem::extract` keeps its camera candidate list as a member (`mCameras`, capacity kept across
  frames) and orders it with `std::sort` plus the world-order tie-break instead of `std::stable_sort`, whose
  temporary buffer is a heap block.
- The asset hot-reload watcher is opt-in (above); its stat pass is the one job-thread allocator a headless frame
  used to carry.
- The scheduler, the per-run `context.view<...>()` idiom, the scene propagation, the physics pull, the renderer
  frame and the script runtime are allocation-free by their own gates (see their READMEs).

What remains, by configuration: a Debug test executable (`_ITERATOR_DEBUG_LEVEL 2`) sees one block per frame,
the container proxy of the local `std::vector` in `assets::detail::completion_queue::drain`
(`asset_manager::update`), which `tests/integration/test_hierarchy_propagation.cpp` records as
`EMPTY_FRAME_ALLOCATION_BUDGET = 1`; spawning and destroying 100 prefab instances per frame costs 20 blocks in
Development and 258 in Debug (`prefab::instantiate_many`'s archive shape, link pass and report vectors,
`core::event::finish_emit`'s graveyard vector behind `scene::bump_structure`, the temporary `command_buffer` of
`world::drain_deferred`), none of them in this module (`test_prefab_spawn_destroy.cpp`,
`STEADY_FRAME_ALLOCATION_BUDGET = 258`). `tests/engine/test_renderer_subsystem.cpp` and `test_layer_stack.cpp`
gate the two engine-side fixes directly.

## Threading model

Everything the engine itself does happens on the main thread: the event pump, the phase orchestration, the
layer callbacks, `render_frame`, the swapchain resize. The scheduler fans PARALLEL systems out over
`core::jobs`; the main thread executes jobs while it waits for a phase (D2). `post_to_main_thread()` is the
one thread-safe entry point: tasks run at frame step 4, a task posted during the drain waits for the next
frame. The module manager refuses loads inside a frame through its `isInsideFrame` hook; the hook carries no
user data, so `modules_subsystem` keeps a mutex-guarded process-wide list of the applications whose modules
subsystem is running and answers "inside a frame" when any of them is (two applications in one process never
silence each other).

`frame_memory::reset()` asserts (and refuses) when the frame loop has marked a scheduler phase in flight
(`set_phase_in_flight`, D6).

`application::inject_render_failure(error, frames)` is the test hook for the failure rules of 06 6: the
renderer runs, its `render_frame` verdict is replaced for the next `frames` frames. Device loss is driven by
`rhi::device::simulate_device_loss()` (`tests/engine/test_render_failure.cpp`).

## Deviations from the design (recorded for review)

- `conjure::modules` is a public dependency (the design listed it private): `engine_context` and
  `application` carry `CONJURE_MODULE_SERVICE` so module entries can `require_service<engine_context>()`.
  The design's other services (`ecs::world`, `assets::asset_database`, `scene::scene_manager`) are not
  provided because those classes would need a `modules` dependency to carry the macro; modules reach them
  through `engine_context`.
- `std::function` replaces `std::move_only_function` (C++23) for `post_to_main_thread`.
- `rendering_settings::shaderDirectory`, `scripting_settings::managedDirectory` and
  `scripting_settings::useNullHost` were added so tests and tools can run the real renderer on the null
  device and the scripting runtime on the null host.
- `layer::on_imgui_end` was added so the ImGui frame can end after every layer drew (R9) without the
  frame loop knowing about ImGui.
- `engine_error` gained `NONE` and `EXPORT_FAILED`; `project_description` carries the raw `physicsSection`
  (R53) and the scene `layers` list.
- The renderer and audio component types are registered into the ECS whenever the libraries are compiled,
  even with rendering or audio disabled, so a headless server still loads scenes that carry them.
- `ecs_module_consumer` keeps trivially copyable columns in place across a reload instead of snapshotting
  them: component ids are stable per name and a layout change is refused at `validate()`.
- Extraction is serial on the main thread (R8 v1: job-parallel preparation lives inside the renderer).
- `frame_memory` warns about overflow once per `OVERFLOW_WARNING_INTERVAL` (60) resets rather than once per
  wall-clock second, so the rule holds under a manual clock.
- `scene::transform_state` has no "physics wrote this" flag: `physics.pull_transforms` writes `local_transform`
  directly and `scene.propagate_transforms` re-dirties those rows through the `changed<local_transform>`
  filter, so the engine needs nothing extra (09 section 4 item 14e).
- `scripting_module_consumer` only records a module's managed assemblies: `script_runtime` has no entry point
  for appending an assembly to the game load context yet.
- `imgui_layer` (and the ImGui window of `debug_overlay_layer`) compile only under `CONJURE_BUILD_EDITOR`, a
  narrowing of 06 2.8/4.1 and ARCHITECTURE 9 (`CONJURE_WITH_RENDERER` and not Distribution): Dear ImGui is
  fetched only when the editor is built (`third_party/CMakeLists.txt`, "editor only"), so a Development
  runtime build with the editor option off has no in-game overlay window. Widening the gate needs the
  ImGui target outside the editor option.
- D6's Debug allocation-epoch check (non-scheduler jobs never allocate from frame memory) is not
  implemented: `core::jobs` does not tag the jobs the scheduler launched, so `frame_memory::allocate()`
  cannot tell them apart. The reset-under-live-jobs half of D6 is asserted (`set_phase_in_flight`).
- `engine_error` gained `SCRIPT_RUNTIME_INIT_FAILED`: a runtime that is present but fails its handshake or
  version check is no longer reported as `SCRIPT_RUNTIME_NOT_FOUND`.
- Owed from 06 10: `tests/integration/test_engine_render_failure.cpp` is covered headless by
  `tests/engine/test_render_failure.cpp` (the null device has no swapchain, so the resize retry and the pacer
  under a real present run only with a window); `test_engine_physics_interpolation.cpp` and
  `tests/benchmarks/bench_engine_frame.cpp` are not delivered yet.

## How to test

```
python tools/build.py --build-dir build/mod_engine --target conjure_engine_tests --test -R engine
```

`tests/engine/` mirrors the public headers one to one and adds the topic files of 06 10 (startup order, phase
order, frame pacer, main-thread queue, the headless application, render transforms) plus one file per adapter
that has behaviour of its own (renderer, audio, scripting, module consumers, built-in modules, export). The
renderer-backed cases read the shader index `conjure_shaders` built (`CONJURE_TEST_SHADER_DIR`); scripting
cases run on the null host.

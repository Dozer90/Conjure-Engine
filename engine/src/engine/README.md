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
| Export | `export_pipeline.cpp` | 06 3.6, R21 |

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

`frame_loop::run_frame()` is 06 3.1 verbatim. The scheduler receives one `ecs::frame_info` per phase built by
`make_frame_info(engine_time)`; `inFixedStep` is true only inside FIXED_UPDATE / PHYSICS_STEP / FIXED_END.
PAUSED runs UPDATE / LATE_UPDATE through `run_phase_filtered(RUNS_WHILE_PAUSED)`; STEP_ONCE and
`step_fixed(n)` leave the mode PAUSED afterwards (R13). EXTRACT runs only when a renderer exists.

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

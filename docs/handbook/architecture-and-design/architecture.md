# Architecture
<!-- doc-template:chapter -->
[Handbook](../index.md) → [Architecture and Design](index.md)

<!-- doc-content:purpose:start -->
This chapter explains the module map, dependency rules, ownership, frame sequence, threading model,
and headless execution. Use it to locate a responsibility and understand the contracts between modules.
<!-- doc-content:purpose:end -->

<!-- doc-content:body:start -->
<a id="1-one-process-one-main-thread-one-job-system"></a>

## One process, one main thread, one job system

Conjure is a C++20, data-oriented 2D and 3D engine with an AI-controllable editor and C# scripting. Six
principles shape every module:

| Principle | What it means in code |
| --- | --- |
| One main thread orchestrates, `core::jobs` does the bulk work | A fixed phase sequence per frame; no render thread in v1; GLFW, ImGui, NVRHI submission and the .NET host live on the main thread |
| Data-oriented internals, object facade | `ecs` stores components in 16 KiB archetype chunks and runs systems as conflict-free parallel stages; `entity_handle` / C# `Entity` wrap it, `chunk_view` / `Span<T>` expose it |
| Everything headless | `null` backends for platform, rhi and audio, a C++ fake of the managed side, an in-memory VFS and a deterministic engine step |
| The editor is a command dispatcher | Every mutation is a `command_request` with an inverse; ImGui panels, the JSON protocol and `--headless --exec` are all clients |
| Plugins are shared libraries with a C front door | ABI identity is proven before any C++ crosses; modules register plain data records that consumers apply atomically |
| Assets are content-addressed blobs | Sources get uuids and `.meta` sidecars, importers cook into a hash-validated cache, export packs it |

<a id="2-module-map"></a>

## Module map

Every engine module is a static library `conjure_<name>` (alias `conjure::<name>`) declared with
`conjure_add_module()` from `cmake/ConjureModule.cmake`; `core` is an umbrella over per-area static libraries
(see [Core](../engine/core.md)). Executables and game modules link the umbrella shared library `conjure_shared`
(alias `conjure::shared`) rather than the static libraries: `conjure_finalize_engine()` in
`cmake/ConjureSharedLibrary.cmake`, called by the root `CMakeLists.txt` right after `add_subdirectory(engine)`,
whole-archives every library registered through `conjure_register_static_library()` (see
[Module system](../extensions/module-system.md) 2 and [Modules module](../engine/modules.md) 7).

| Module | Namespace | Purpose |
| --- | --- | --- |
| [`core`](../engine/core.md) | `conjure::core` | log/assert, `result`/`error`, uuid, hashing, containers, memory, jobs, events, vfs, json, reflection, math, clock, process, dynamic libraries |
| [`ecs`](../engine/ecs.md) | `conjure::ecs` | archetype world, component registry, queries, scheduler, command buffer, archives, serialisation |
| [`platform`](../engine/platform.md) | `conjure::platform` | window, input, events; backends `glfw`, `null` |
| [`rhi`](../engine/rhi.md) | `conjure::rhi` | NVRHI device, swapchain, shader library; backends `d3d12`, `vulkan`, `null` |
| [`assets`](../engine/assets.md) | `conjure::assets` | uuid database, importers, cooked cache, async loading, hot reload, packs |
| [`audio`](../engine/audio.md) | `conjure::audio` | mixer and voices, `conjure.audio.clip` asset type, groups, spatialisation; backends `miniaudio`, `null` |
| [`modules`](../engine/modules.md) | `conjure::modules` | manifests, ABI check, load/unload/reload, registration records, consumers |
| [`scene`](../engine/scene.md) | `conjure::scene` | hierarchy and transforms, name/tag/layer/guid, prefabs, scene files, snapshots, scene manager |
| [`renderer`](../engine/renderer.md) | `conjure::renderer` | render graph over rhi, `render_scene` contract, PBR/IBL/shadows/SSAO/HDR, 2D, ImGui backend, glTF import |
| [`physics`](../engine/physics.md) | `conjure::physics` | Jolt world, body/collider/character components, layers, queries, contact events |
| [`scripting`](../engine/scripting.md) | `conjure::scripting` |.NET host, native/managed tables, script components, hot reload |
| [`engine`](../engine/engine.md) | `conjure::engine` | application, frame loop, subsystems, `project.json`, export pipeline, layers |
| [`bindings`](../engine/bindings.md) | `conjure::bindings` | the non-core native table entries, C# facades, `[NativeComponent]` mirrors, call counters |
| [`editor_core`, `editor_ui`](../editor/editor.md) | `conjure::editor` | commands, undo, protocol, headless runner; ImGui workspace |
| `conjure_editor`, [`conjure_runtime`](../engine/runtime.md) | - | thin `main.cpp` executables |
| `conjure_shared` | - | SHARED umbrella that whole-archives every enabled module |

Executables, game modules, and `LINK_ENGINE_SHARED` test executables link `conjure_shared`.
The editor libraries are included outside Distribution; `tools/check_distribution_symbols.py` checks
that a Distribution runtime carries none of their symbols.

<a id="3-dependency-rules"></a>

## Dependency rules

Public link dependencies, arrows pointing at what is depended on:

```
core
 ^      ^        ^        ^        ^        ^
ecs  platform   rhi    assets <- audio   modules            (rhi -> platform; audio -> assets)
 ^      ^        ^        ^        ^        ^
scene --+        |        |        |        |                (scene -> ecs, assets)
 +--- renderer --+--------+        |        |                renderer -> core, rhi, assets   (NO ecs)
 +--- physics ---+--------+        |        |                physics  -> ecs, scene, assets
 +--- scripting -+--------+        |        |                scripting -> ecs, assets        (NO scene)
engine ----------+--------+--------+--------+                enabled modules, by CONJURE_WITH_* option
bindings    -> scripting, scene, assets, engine (public); physics, audio, renderer, modules, platform (private)
editor_core -> engine, scene, assets, ecs, (renderer, physics, scripting), modules
editor_ui   -> editor_core, renderer (imgui_renderer), ImGui, ImGuizmo
conjure_editor / conjure_runtime / game modules -> conjure_shared only
```

The rules that keep this acyclic:

| Rule | Consequence |
|---|---|
| `renderer` and `audio` never include `ecs` | the engine's subsystem adapters register their component types and own `engine.render_sync`, `engine.render_extract`, `engine.audio_sync` |
| `scripting` never includes `scene` | `script_component` carries its own serializer so scene files round-trip scripts without a scene -> scripting edge |
| `scene` never includes renderer, physics or scripting | `physics` reads scene's `parent` / `local_transform` columns and adds `interpolated_transform` itself |
| no module below `engine` includes `engine` | modules reach engine services only through `modules::module_context` |
| third-party headers stay in `src/` | glm is the single exception, through `conjure/core/math/glm_types.h` |

> [!IMPORTANT]
> Public header changes belong with the owning module's tests and documentation. Dependent modules
> should use the updated API rather than add compatibility workarounds.

<a id="4-ownership"></a>

## Ownership

`engine::application` owns every module object through `engine_services` (one `std::unique_ptr` each, created
in the startup order of and destroyed in reverse) and exposes them as non-owning
pointers in `engine::engine_context`. Nothing else owns a module object. Lifetime invariants:
`component_registry` outlives every `world`; `asset_manager` outlives every handle; `module_manager` is
destroyed before the consumers and services it points at. A disabled `CONJURE_WITH_*` option leaves the
matching `engine_context` pointer null.

<a id="5-the-frame"></a>

## The frame

All phases are `ecs::system_phase` values: `FRAME_BEGIN, FIXED_UPDATE, PHYSICS_STEP, FIXED_END, UPDATE,
LATE_UPDATE, TRANSFORM, SYNC, EXTRACT, FRAME_END`. The engine calls `scheduler.run_phase(phase, frame_info)`;
the scheduler flushes command buffers at every stage boundary, so structural changes become visible at the next
phase, never mid-phase.

```
step_frame()
  time.begin_frame                 clock | forced delta | step_frame(delta) | step_fixed(n)
  frame_memory.reset               safe: every scheduler job of frame N-1 finished at FRAME_END
  window.pump_events -> input      [platform]
  main-thread queue drain          [core::jobs::drain_main_thread_queue]
  subsystems.begin_frame           assets update + hot reload, modules.apply_pending, scripting frame block, ImGui::NewFrame
  FRAME_BEGIN     scene.apply_pending_loads | scripting.frame_begin
  [ x N fixed steps, skipped while PAUSED ]
    FIXED_UPDATE  scripting.collision_dispatch (previous step's contacts) -> scripting.fixed_update
    PHYSICS_STEP  physics.push_transforms -> physics.step (MAIN_THREAD, Jolt jobs on core::jobs)
    FIXED_END     physics.pull_transforms ; physics.contact_events
  UPDATE          scripting.update | gameplay and plugin systems
  LATE_UPDATE     scripting.late_update | camera follow
  TRANSFORM       scene.apply_hierarchy_changes (EXCLUSIVE) -> scene.propagate_transforms (one pass)
  SYNC            engine.render_sync, engine.audio_sync
  EXTRACT         engine.render_extract into the renderer's render_scene ; renderer.render_frame
  FRAME_END       scripting.frame_end, scene.destroy_pending, audio one-shots, stats
```

Pause: the editor keeps `simulation_mode::PAUSED` while editing (fixed loop and non-`RUNS_WHILE_PAUSED`
UPDATE systems skipped; TRANSFORM/SYNC/EXTRACT still run so edits render) and `script_mode::EDIT`. Play is
`RUNNING` + `PLAY`; step is `step_fixed(n)`. The [Engine module](../engine/engine.md#3-the-frame-loop) describes
the full sequence and its subsystem hooks.

<a id="6-threading-model"></a>

## Threading model

| Thread | Owner | Does | Talks to others via |
|---|---|---|---|
| Main | `engine::application` | event pump, phase orchestration, all structural world changes, all managed code (`EXCLUSIVE` scripting systems), ImGui, GPU resource creation and submission | direct calls; executes jobs while waiting |
| Job workers | `core::jobs::job_system` | `PARALLEL` systems over chunks, Jolt jobs, asset IO/decode/import, renderer culling/sorting/fill, IBL bake | job counters only |
| Audio callback | miniaudio inside the `audio` backend | mixing | lock-free command and event rings |
| `dedicated_thread` | `core::jobs` | the editor's blocking `--ai-stdio` stdin reader; the `system::process` pipe readers `process-stdout` and `process-stderr` (Windows) and `process-pipes` (POSIX) | mutex-guarded line queue; mutex-guarded capture buffers |

Nothing else creates threads: `std::thread` / `std::jthread` appear only in `engine/src/core/jobs/` and
`engine/include/conjure/core/jobs/` (`tools/check_style.py` rule
`thread-outside-jobs`). System thread modes are `PARALLEL` (chunk-parallel, access sets declared), `MAIN_THREAD`
(may overlap non-conflicting `PARALLEL` systems) and `EXCLUSIVE` (alone, may mutate the world directly).
Frame memory may be allocated from any scheduler job and is reset only on the main thread at FRAME_BEGIN. The
file watcher and the TCP protocol server own no thread: both poll from the main thread. See
[Data-oriented design](data-oriented-design.md) for how systems and frame memory use the workers today.

<a id="7-headless-philosophy"></a>

## Headless philosophy

Every module must be testable in CI without a GPU, window, audio device or .NET runtime:

- `platform`, `rhi` and `audio` ship `null` backends that behave like the real ones (the null window has an
  in-memory clipboard, `push_event` / `simulate_*` test controls; the null RHI records `null_device_stats`;
  the null audio backend records `null_backend_stats` and runs a virtual clock). See [Platform](../engine/platform.md) 6,
  [RHI](../engine/rhi.md) 3.4 and [Audio](../engine/audio.md) 8.
- `scripting` has `null_script_host`, a C++ fake of the managed side.
- `core::vfs::memory_file_system` replaces the disk; `core::manual_clock` replaces time; `core::random::seed`
  fixes randomness; a `job_system` started with `workerCount = 0` runs every job inline.
- The editor runs the same command dispatcher headless: `conjure_editor --headless --exec commands.json
  --project <dir> --trust-project` (exit codes 0/1/2/3; without an answer for the project it exits with 2).
- `conjure_runtime --project <dir> --headless --frames N` boots an exported folder without a window. A headless
  run executes no C#; `--scripting` runs the project's scripts.

> [!WARNING]
> Tests are forbidden from touching the wall clock, sleeping, creating `std::thread`s or reading environment
> variables (`tools/check_style.py` rule `forbidden-in-tests`). Use `manual_clock`, the job
> system and `environment().seed`; the allowed exceptions are listed with their reason in
> `tools/style_exceptions.json`.

<a id="8-build-and-configuration-matrix"></a>

## Build and configuration matrix

| Axis | Values |
|---|---|
| Configurations | `Debug`, `Development` (default: optimised + asserts + editor + profiling), `Distribution` (`CONJURE_DISTRIBUTION`, asserts out, editor off) |
| Platforms | Windows 11 MSVC 14.44+, Ubuntu 24.04 GCC 13+ / Clang 16+, macOS 14 AppleClang; CMake >= 3.28, Ninja |
| Presets (`CMakePresets.json`) | configure: `windows-msvc-{debug,dev,dist,asan}`, `windows-vs` (multi-config, built and tested through `windows-vs-{debug,dev}`), `linux-gcc-{debug,dev,dist}`, `linux-clang-{debug,sanitize,tsan}`, `macos-{debug,dev,dist}`; test: one per configure preset plus the nightly `windows-msvc-debug-nightly` / `linux-gcc-debug-nightly` (exclude only `benchmark`; the default test presets also exclude `golden`, `downloaded`, `slow`, `process`). `python tools/build.py` defaults to `<platform>-debug` |
| Options | `CONJURE_WITH_RENDERER/PHYSICS/AUDIO/SCRIPTING`, `CONJURE_BUILD_EDITOR/RUNTIME/TESTS/BENCHMARKS/SAMPLES/MANAGED`, `CONJURE_RHI_D3D12` (Windows only, default ON) and `CONJURE_RHI_VULKAN` (default OFF on Windows, ON elsewhere), `CONJURE_PLATFORM_GLFW`, `CONJURE_ENABLE_ASAN/UBSAN` and `CONJURE_ENABLE_TSAN` (`cmake/ConjureSanitizers.cmake`), `CONJURE_TEST_GOLDEN_IMAGES` (`cmake/ConjureTesting.cmake`), `CONJURE_WARNINGS_AS_ERRORS` |
| Warnings | `/W4 /WX` on MSVC, `-Wall -Wextra -Wpedantic -Werror` elsewhere (`cmake/ConjureWarnings.cmake`); third-party targets are `SYSTEM` includes |
| Driver | `python tools/build.py [--preset <name>] [--build-dir build/<name>] [--target <t>] [--test [-R <regex>]]` sets up MSVC on Windows and wraps the presets |

Exactly one of `CONJURE_DEBUG`, `CONJURE_DEVELOPMENT`, `CONJURE_DISTRIBUTION` is 1 (`conjure/core/platform_detect.h`
enforces it); `CONJURE_ASSERTS_ENABLED` and `CONJURE_DEV_TOOLS_ENABLED` follow from it.

## Related

- handbook: [Coding standards](../development/coding-standards.md), [Data-oriented design](data-oriented-design.md),
  the module pages [Core](../engine/core.md), [ECS](../engine/ecs.md), [Platform](../engine/platform.md),
  [RHI](../engine/rhi.md), [Assets](../engine/assets.md), [Audio](../engine/audio.md), [Modules](../engine/modules.md),
  [Module system](../extensions/module-system.md).

<a id="contents"></a>
<!-- doc-content:body:end -->

# Architecture

🛠 **Engineering handbook** · 📘 API: [Public API wiki](../api/index.md)

This chapter is the short form of `docs/ARCHITECTURE.md`: the module map and its waves, the dependency rules
that keep the graph acyclic, the frame sequence, the threading model and the headless philosophy. It tells you
where a responsibility lives and which design record owns the details; it does not repeat those records. When
a statement here and a design record disagree, the record wins, and `09_cross_area_resolutions.md` wins over
every record (see [Decision records](decision-records.md)).

## Contents
- [1. One process, one main thread, one job system](#1-one-process-one-main-thread-one-job-system)
- [2. Module map and waves](#2-module-map-and-waves)
- [3. Dependency rules](#3-dependency-rules)
- [4. Ownership](#4-ownership)
- [5. The frame](#5-the-frame)
- [6. Threading model](#6-threading-model)
- [7. Headless philosophy](#7-headless-philosophy)
- [8. Build and configuration matrix](#8-build-and-configuration-matrix)
- [Related](#related)

## 1. One process, one main thread, one job system

Conjure is a C++20, data-oriented 2D and 3D engine with an AI-controllable editor and C# scripting. Six
principles shape every module:

| Principle | What it means in code | Record |
|---|---|---|
| One main thread orchestrates, `core::jobs` does the bulk work | A fixed phase sequence per frame; no render thread in v1; GLFW, ImGui, NVRHI submission and the .NET host live on the main thread | `06_engine_loop.md` D1 |
| Data-oriented internals, object facade | `ecs` stores components in 16 KiB archetype chunks and runs systems as conflict-free parallel stages; `entity_handle` / C# `Entity` wrap it, `chunk_view` / `Span<T>` expose it | `01_ecs.md`, `03_scripting.md` |
| Everything headless | `null` backends for platform, rhi and audio, a C++ fake of the managed side, an in-memory VFS and a deterministic engine step | `08_testing.md` |
| The editor is a command dispatcher | Every mutation is a `command_request` with an inverse; ImGui panels, the JSON protocol and `--headless --exec` are all clients | `04_editor.md` |
| Plugins are shared libraries with a C front door | ABI identity is proven before any C++ crosses; modules register plain data records that consumers apply atomically | `07_modules.md` |
| Assets are content-addressed blobs | Sources get uuids and `.meta` sidecars, importers cook into a hash-validated cache, export packs it | `05_assets.md` |

> [!NOTE]
> Modules land wave by wave. Waves 1 and 2 are in the tree: `core`, `ecs`, `platform`, `rhi`, `assets`, `audio`
> and `modules`, each with a handbook page under [`modules/`](modules/core.md) and wiki pages under `docs/api/`.
> `engine/CMakeLists.txt` adds each `engine/src/<module>/` that has a `CMakeLists.txt` and skips the rest, so
> the tree always builds; to see what is built today, run the configure step and read its
> `engine: module '<name>' not present yet, skipping` lines. For a wave-3 or later module the design record is
> the reference until its code lands.

## 2. Module map and waves

Every engine module is a static library `conjure_<name>` (alias `conjure::<name>`) declared with
`conjure_add_module()` from `cmake/ConjureModule.cmake`; `core` is an umbrella over per-area static libraries
(see [Core](modules/core.md)). Executables and game modules link the umbrella shared library `conjure_shared`
(alias `conjure::shared`, `07_modules.md` 5) rather than the static libraries: `conjure_finalize_engine()` in
`cmake/ConjureSharedLibrary.cmake`, called by the root `CMakeLists.txt` right after `add_subdirectory(engine)`,
whole-archives every library registered through `conjure_register_static_library()` (see
[Module system](module-system/index.md) 2 and [Modules module](modules/modules.md) 7).

| Wave | Module | Namespace | Design record | Purpose |
|---|---|---|---|---|
| 1 | [`core`](modules/core.md) | `conjure::core` | `10_core.md` | log/assert, `result`/`error`, uuid, hashing, containers, memory, jobs, events, vfs, json, reflection, math, clock, process, dynamic libraries |
| 2 | [`ecs`](modules/ecs.md) | `conjure::ecs` | `01_ecs.md` | archetype world, component registry, queries, scheduler, command buffer, archives, serialisation |
| 2 | [`platform`](modules/platform.md) | `conjure::platform` | `11_platform.md` | window, input, events; backends `glfw`, `null` |
| 2 | [`rhi`](modules/rhi.md) | `conjure::rhi` | `02_renderer.md` 4.1 | NVRHI device, swapchain, shader library; backends `d3d12`, `vulkan`, `null` |
| 2 | [`assets`](modules/assets.md) | `conjure::assets` | `05_assets.md` | uuid database, importers, cooked cache, async loading, hot reload, packs |
| 2 | [`audio`](modules/audio.md) | `conjure::audio` | `12_audio.md` | mixer and voices, `conjure.audio.clip` asset type, groups, spatialisation; backends `miniaudio`, `null` |
| 2 | [`modules`](modules/modules.md) | `conjure::modules` | `07_modules.md` | manifests, ABI check, load/unload/reload, registration records, consumers |
| 3a | `scene` | `conjure::scene` | `13_scene.md` | hierarchy and transforms, name/tag/layer/guid, prefabs, scene files, snapshots, scene manager |
| 3b | `renderer` | `conjure::renderer` | `02_renderer.md` | render graph over rhi, `render_scene` contract, PBR/IBL/shadows/SSAO/HDR, 2D, ImGui backend, glTF import |
| 3b | `physics` | `conjure::physics` | `14_physics.md` | Jolt world, body/collider/character components, layers, queries, contact events |
| 3b | `scripting` | `conjure::scripting` | `03_scripting.md` | .NET host, native/managed tables, script components, hot reload |
| 4 | `engine` | `conjure::engine` | `06_engine_loop.md` | application, frame loop, subsystems, `project.json`, export pipeline, layers |
| 4 | `bindings` | `conjure::bindings` | `15_bindings.md` | the non-core native table entries, C# facades, `[NativeComponent]` mirrors, call counters |
| 4 | `editor_core`, `editor_ui` | `conjure::editor` | `04_editor.md` | commands, undo, protocol, headless runner; ImGui workspace |
| 4 | `conjure_editor`, `conjure_runtime` | - | `04_editor.md` 3.1, `06_engine_loop.md` 4.2 | thin `main.cpp` executables |
| 4 | `conjure_shared` | - | `07_modules.md` 5 | SHARED umbrella that whole-archives every enabled module (R17) |
| 5 | `samples/feature_test`, `samples/stress_test` | - | `08_testing.md` 7.2 | the "everything" scene generated from an AI command file; 10,000 dynamic entities |

Each wave is implementable in parallel across its modules once the previous wave's public headers exist. The
linked rows are implemented (waves 1 and 2); `conjure_shared` is built today by `conjure_finalize_engine()`
even though the executables that link it are wave 4, and the `tests/modules` suite already links it
(`LINK_ENGINE_SHARED`). Rows from wave 3a on are design only. The per-wave definition of done lives in
`docs/ARCHITECTURE.md` 11; the generic one is `08_testing.md` 7.4.

## 3. Dependency rules

Public link dependencies, arrows pointing at what is depended on:

```
core
 ^      ^        ^        ^        ^        ^
ecs  platform   rhi    assets <- audio   modules            wave 2 (rhi -> platform; audio -> assets, R43)
 ^      ^        ^        ^        ^        ^
scene --+        |        |        |        |                wave 3a (scene -> ecs, assets)
 +--- renderer --+--------+        |        |                renderer -> core, rhi, assets   (NO ecs)
 +--- physics ---+--------+        |        |                physics  -> ecs, scene, assets
 +--- scripting -+--------+        |        |                scripting -> ecs, assets        (NO scene)
engine ----------+--------+--------+--------+                wave 4: everything above, by CONJURE_WITH_* option
bindings    -> scripting, scene, assets, engine (public); physics, audio, renderer, modules, platform (private)
editor_core -> engine, scene, assets, ecs, (renderer, physics, scripting), modules
editor_ui   -> editor_core, renderer (imgui_renderer), ImGui, ImGuizmo
conjure_editor / conjure_runtime / game modules -> conjure_shared only
```

The rules that keep this acyclic (R7, R19, R29, R43, R47):

| Rule | Consequence |
|---|---|
| `renderer` and `audio` never include `ecs` | the engine's subsystem adapters register their component types and own `engine.render_sync`, `engine.render_extract`, `engine.audio_sync` |
| `scripting` never includes `scene` | `script_component` carries its own serializer so scene files round-trip scripts without a scene -> scripting edge |
| `scene` never includes renderer, physics or scripting | `physics` reads scene's `parent` / `local_transform` columns and adds `interpolated_transform` itself |
| no module below `engine` includes `engine` | modules reach engine services only through `modules::module_context` |
| third-party headers stay in `src/` | glm is the single exception, through `conjure/core/math/glm_types.h` |

> [!IMPORTANT]
> A public header change in a lower wave is a PR against that module with its own tests; higher waves never
> patch around it (`docs/ARCHITECTURE.md` 11, cross-wave gates).

## 4. Ownership

`engine::application` owns every module object through `engine_services` (one `std::unique_ptr` each, created
in the startup order of `06_engine_loop.md` 3.4 and destroyed in reverse) and exposes them as non-owning
pointers in `engine::engine_context` (R22). Nothing else owns a module object. Lifetime invariants:
`component_registry` outlives every `world`; `asset_manager` outlives every handle; `module_manager` is
destroyed before the consumers and services it points at. A disabled `CONJURE_WITH_*` option leaves the
matching `engine_context` pointer null.

## 5. The frame

All phases are `ecs::system_phase` values (R1): `FRAME_BEGIN, FIXED_UPDATE, PHYSICS_STEP, FIXED_END, UPDATE,
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
  TRANSFORM       scene.apply_hierarchy_changes (EXCLUSIVE) -> scene.propagate_transforms (one pass, R3)
  SYNC            engine.render_sync, engine.audio_sync
  EXTRACT         engine.render_extract into the renderer's render_scene ; renderer.render_frame
  FRAME_END       scripting.frame_end, scene.destroy_pending, audio one-shots, stats
```

Pause (R13): the editor keeps `simulation_mode::PAUSED` while editing (fixed loop and non-`RUNS_WHILE_PAUSED`
UPDATE systems skipped; TRANSFORM/SYNC/EXTRACT still run so edits render) and `script_mode::EDIT`. Play is
`RUNNING` + `PLAY`; step is `step_fixed(n)`. The full sequence with every subsystem hook is `06_engine_loop.md`
3.1.

## 6. Threading model

| Thread | Owner | Does | Talks to others via |
|---|---|---|---|
| Main | `engine::application` | event pump, phase orchestration, all structural world changes, all managed code (`EXCLUSIVE` scripting systems), ImGui, GPU resource creation and submission | direct calls; executes jobs while waiting |
| Job workers | `core::jobs::job_system` | `PARALLEL` systems over chunks, Jolt jobs, asset IO/decode/import, renderer culling/sorting/fill, IBL bake | job counters only |
| Audio callback | miniaudio inside the `audio` backend | mixing | lock-free command and event rings |
| `dedicated_thread` | `core::jobs` | the editor's blocking `--ai-stdio` stdin reader (R38) | mutex-guarded line queue |
| plain `std::thread` | `core::system` | the `process` stdout/stderr pipe readers: the documented exception (`10_core.md` C4), because `system` sits below `jobs` in the area graph (C2) and cannot use `dedicated_thread` | mutex-guarded capture buffers |

Nothing else creates threads: `std::thread` / `std::jthread` appear only in `engine/src/core/jobs/`, in the
`core/system` process backends and in backend implementation files (CONVENTIONS 3). System thread modes are `PARALLEL` (chunk-parallel, access sets declared), `MAIN_THREAD`
(may overlap non-conflicting `PARALLEL` systems) and `EXCLUSIVE` (alone, may mutate the world directly).
Frame memory may be allocated from any scheduler job and is reset only on the main thread at FRAME_BEGIN. The
file watcher and the TCP protocol server own no thread: both poll from the main thread (R38). See
[Data-oriented design](data-oriented-design.md) for how systems and frame memory use the workers today.

## 7. Headless philosophy

Every module must be testable in CI without a GPU, window, audio device or .NET runtime:

- `platform`, `rhi` and `audio` ship `null` backends that behave like the real ones (the null window has an
  in-memory clipboard, `push_event` / `simulate_*` test controls; the null RHI records `null_device_stats`;
  the null audio backend records `null_backend_stats` and runs a virtual clock). See [Platform](modules/platform.md) 6,
  [RHI](modules/rhi.md) 3.4 and [Audio](modules/audio.md) 8.
- `scripting` has `null_script_host`, a C++ fake of the managed side.
- `core::vfs::memory_file_system` replaces the disk; `core::manual_clock` replaces time; `core::random::seed`
  fixes randomness; a `job_system` started with `workerCount = 0` runs every job inline.
- The editor runs the same command dispatcher headless: `conjure_editor --headless --exec commands.json
  --project <dir>` (exit codes 0/1/2/3).
- `conjure_runtime --project <dir> --headless --frames N` boots an exported folder without a window.

> [!WARNING]
> Tests are forbidden from touching the wall clock, sleeping, creating `std::thread`s or reading environment
> variables (CONVENTIONS 6; `tools/check_style.py` rule `forbidden-in-tests`). Use `manual_clock`, the job
> system and `environment().seed`; the allowed exceptions are listed with their reason in
> `tools/style_exceptions.json`.

## 8. Build and configuration matrix

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
- `docs/ARCHITECTURE.md`: the binding long form, including the asset data flow (5), the scripting boundary (6),
  the editor path (7), the export path (8) and the wave plan (11)
- design records: [`../design/00_brief.md`](../design/00_brief.md) (fixed decisions),
  [`../design/06_engine_loop.md`](../design/06_engine_loop.md) (frame, threads),
  [`../design/09_cross_area_resolutions.md`](../design/09_cross_area_resolutions.md) (the `R<n>` ids cited above)
- handbook: [Coding standards](coding-standards.md), [Data-oriented design](data-oriented-design.md),
  the module pages [Core](modules/core.md), [ECS](modules/ecs.md), [Platform](modules/platform.md),
  [RHI](modules/rhi.md), [Assets](modules/assets.md), [Audio](modules/audio.md), [Modules](modules/modules.md),
  [Module system](module-system/index.md), [Decision records](decision-records.md)

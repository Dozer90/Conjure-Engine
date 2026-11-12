# Conjure Engine - Architecture

Status: **binding**. This is the top-level map of the Conjure engine. It does not repeat the area designs
in `docs/design/01_*.md` ... `08_*.md` and the module design notes `10_*.md` ... `15_*.md`; it states how they
fit together, fixes the contracts that no area document owns, and tells an implementer where to look. When this
document and an area document disagree, `docs/design/09_cross_area_resolutions.md` (the consistency review)
decides; its ids (`R<n>`) are cited here.

Inputs, in order of authority: `docs/DESIGN_DOC.md` (customer), `docs/design/00_brief.md` (fixed
decisions), `docs/CONVENTIONS.md` + `docs/CODE_STYLE.cpp` (how code is written), `docs/design/0[1-9]_*.md`,
`docs/design/1[0-5]_*.md`.

---

## 1. Overview

Conjure is a C++20, data-oriented 2D + 3D game engine with an AI-controllable editor and C# scripting.

- **One process, one main thread, one job system.** The main thread orchestrates a fixed sequence of
  phases every frame; `core::jobs` does the bulk work inside each phase; there is no render thread in v1
  (`06_engine_loop.md` D1). GLFW, ImGui, NVRHI submission and the .NET host all live on the main thread.
- **Data-oriented internals, object facade.** `ecs` stores components in 16 KiB archetype chunks and runs
  systems as conflict-free parallel stages; `entity_handle` / `component_ref<T>` and the C# `Entity` wrap
  it for casual users; `chunk_view` / `Span<T>` expose it for power users (`01_ecs.md`, `03_scripting.md`).
- **Everything headless.** `null` backends for platform, rhi and audio, a C++ fake of the managed side
  (`null_script_host`), an in-memory VFS and a deterministic engine step make every module testable in CI
  without a GPU, window, audio device or .NET runtime (`08_testing.md`).
- **The editor is a command dispatcher.** Every mutation is a `command_request` with an inverse; the ImGui
  panels, the newline-delimited JSON protocol (TCP localhost / stdio) and `--headless --exec` are all
  clients of one dispatcher (`04_editor.md`).
- **Plugins are shared libraries with a C front door.** A module proves ABI identity before any C++
  crosses, registers plain data records, and host-side consumers apply them atomically (`07_modules.md`).
- **Assets are content-addressed blobs.** Sources get uuids and `.meta` sidecars, importers produce cooked
  blobs into a hash-validated cache, loaders read blobs in editor and runtime alike, export packs them
  (`05_assets.md`).

---

## 2. Module map

### 2.1 Modules and targets

| Wave | Module (`conjure_<name>`) | Namespace | Design doc | One-line purpose |
|---|---|---|---|---|
| 1 | `core` | `conjure::core` | `10_core.md` (summary in section 4.1 here) | log/assert, `result`/`error`, uuid, hashing, containers, memory, jobs, events, vfs, json, reflection, math, clock, process, dynamic_library |
| 2 | `ecs` | `conjure::ecs` | `01_ecs.md` | archetype world, component registry, queries, scheduler, command buffer, archives, serialisation |
| 2 | `platform` | `conjure::platform` | `11_platform.md` | window, input, events; backends `glfw`, `null` |
| 2 | `rhi` | `conjure::rhi` | `02_renderer.md` 4.1 | NVRHI device/swapchain/shader library; backends `d3d12`, `vulkan`, `null` |
| 2 | `assets` | `conjure::assets` | `05_assets.md` | uuid database, importers, cooked cache, async loading, hot reload, packs, cooker |
| 2 | `audio` | `conjure::audio` | `12_audio.md` | mixer + voices, `conjure.audio.clip` asset type, sources/listener components, groups, spatialisation; backends `miniaudio`, `null` |
| 2 | `modules` | `conjure::modules` | `07_modules.md` | manifests, ABI check, load/unload/reload, registration records, consumers |
| 3a | `scene` | `conjure::scene` | `13_scene.md` | hierarchy + transforms, name/tag/layer/guid, prefabs, scene files, snapshots, scene manager |
| 3b | `renderer` | `conjure::renderer` | `02_renderer.md` | render graph over rhi, render_scene contract, PBR/IBL/shadows/SSAO/HDR, 2D, ImGui backend, glTF import |
| 3b | `physics` | `conjure::physics` | `14_physics.md` | Jolt world, body/collider/character components, layers, queries, contact events, `conjure.physics.collision_mesh` asset type |
| 3b | `scripting` | `conjure::scripting` | `03_scripting.md` | .NET host, native/managed tables, script components, hot reload, managed build |
| 4 | `engine` | `conjure::engine` | `06_engine_loop.md` | application, frame loop, subsystems, project.json, export pipeline, layers |
| 4 | `bindings` | `conjure::bindings` | `15_bindings.md` | the non-core native table entries, C# facades and `[NativeComponent]` mirrors, call counters, diagnostics |
| 4 | `editor_core`, `editor_ui` | `conjure::editor` | `04_editor.md` | commands, undo, protocol, headless runner; ImGui workspace |
| 4 | executables `conjure_editor`, `conjure_runtime` | - | `04_editor.md` 3.1, `06_engine_loop.md` 4.2 | thin `main.cpp`s |
| 4 | `conjure_shared` (umbrella `SHARED`) | - | `07_modules.md` 5 | whole-archives every enabled module; linked by both executables and all plugins (R17) |
| 5 | `samples/feature_test`, `samples/stress_test` | - | `08_testing.md` 7.2 | the "everything" scene, generated from an AI command file |

Test-only: `conjure_test_support` (wave 1), `conjure_test_support_engine` (wave 4), `conjure_<module>_tests`,
`conjure_integration_tests`, `conjure_downloaded_tests`, `conjure_golden_tests`, `conjure_benchmarks`.

### 2.2 Dependency graph (public link dependencies; arrows point at what is depended on)

```
core
 ^      ^        ^        ^        ^        ^
 |      |        |        |        |        |
ecs  platform   rhi    assets <- audio   modules            wave 2 (rhi -> platform; audio -> assets, R43)
 ^      ^        ^        ^        ^        ^
 |      |        |        |        |        |
scene --+        |        |        |        |                wave 3a (scene -> ecs, assets)
 ^               |        |        |        |
 +--- renderer --+--------+        |        |                renderer -> core, rhi, assets   (NO ecs)
 +--- physics ---+--------+        |        |                physics  -> ecs, scene, assets  (Jolt, cgltf private)
 +--- scripting -+--------+        |        |                scripting -> ecs, assets        (NO scene)
 ^               ^        ^        ^        ^
 |               |        |        |        |
engine ----------+--------+--------+--------+                wave 4: everything above, by CONJURE_WITH_* option
 ^
bindings ---> scripting, scene, assets, engine (public); physics*, audio*, renderer*, modules, platform (private)
editor_core -> engine, scene, assets, ecs, (renderer*, physics*, scripting*), modules
editor_ui   -> editor_core, renderer (imgui_renderer), ImGui, ImGuizmo
conjure_editor / conjure_runtime / game modules -> conjure_shared only
```

Rules that keep the graph acyclic (R7, R29, R43): `renderer` and `audio` never include `ecs`; the engine's
subsystem adapters register their component types and own their systems (`audio` still depends on `assets` for its
clip asset type and `asset_ref<audio_clip>` component field, exactly like `renderer`). `scripting` never includes
`scene`; `script_component` carries its own serializer. `scene` never includes renderer, physics or scripting;
`physics` reads scene's `parent`/`local_transform` columns and adds `interpolated_transform` itself (R47). No module
below `engine` includes `engine`; modules reach engine services only through `modules::module_context` services (R19).

### 2.3 Ownership

`engine::application` owns every module object through `engine_services` (`std::unique_ptr` each, created in
the startup order of `06_engine_loop.md` 3.4, destroyed in reverse) and exposes them as non-owning pointers in
`engine::engine_context` (R22). Nothing else owns a module object. `component_registry` outlives every
`world`; `asset_manager` outlives every handle; `module_manager` is destroyed before the consumers and
services it points at.

---

## 3. The frame

All phases are `ecs::system_phase` values (R1); the engine calls `scheduler.run_phase(phase, frame_info)` and the
scheduler flushes command buffers at every stage boundary, so structural changes are visible at the next
phase, never mid-phase. Full sequence: `06_engine_loop.md` 3.1.

```
step_frame()
  time.begin_frame                 clock | forced delta | step_frame(delta) | step_fixed(n) override
  frame_memory.reset               safe: every scheduler job of frame N-1 finished at FRAME_END
  window.pump_events -> layers / input.apply ; input.end_frame      [platform]
  main-thread queue drain                                            [core::jobs::post_to_main_thread]
  subsystems.begin_frame           assets: begin_frame + update (+ hot_reload_controller.update)
                                   modules: apply_pending            scripting: write frame_block
                                   imgui_layer: ImGui::NewFrame
  FRAME_BEGIN     scene.apply_pending_loads | scripting.frame_begin (reload tick, OnCreate flush)
  [ x N fixed steps, skipped while PAUSED ]
    FIXED_UPDATE  scripting.collision_dispatch (previous step's contacts) -> scripting.fixed_update
    PHYSICS_STEP  physics.push_transforms -> physics.step (MAIN_THREAD, Jolt jobs on core::jobs)
    FIXED_END     physics.pull_transforms (interpolated_transform prev/curr) ; physics.contact_events
  UPDATE          scripting.update | gameplay & plugin systems         (filtered by RUNS_WHILE_PAUSED when PAUSED)
  layers.on_update
  LATE_UPDATE     scripting.late_update | camera follow
  TRANSFORM       scene.apply_hierarchy_changes (EXCLUSIVE, repairs raw parent edits, R46) ->
                  scene.propagate_transforms: world_transform + render_world_transform in one pass (R3)
  SYNC            engine.render_sync (camera/light/environment params), engine.audio_sync (listener/sources)
  scene = renderer.begin_extract ; layers.on_imgui (ImGui::Render -> renderer.set_imgui_draw_data)
  EXTRACT         engine.render_extract: cull + mesh_instance/light/sprite/debug-draw fill into `scene`
  renderer.end_extract ; renderer.render_frame (job-parallel prepare, one command list, present)
  FRAME_END       scripting.frame_end (OnDestroy flush), scene.destroy_pending, audio one-shots, stats
  subsystems.end_frame ; layers.on_frame_end ; layers.apply_pending ; pacer ; stats.commit
```

Thread model (`06_engine_loop.md` 5, R38): main thread = orchestration, all structural world changes, all
managed code (scripting systems are `EXCLUSIVE`), ImGui, GPU resource creation, submission. Job workers =
`PARALLEL` systems over chunks, Jolt jobs, asset IO/decode/import, renderer culling/sorting/fill, IBL bake.
Only sanctioned extra thread: `core::jobs::dedicated_thread` for the editor's blocking stdin reader; the
miniaudio callback belongs to the audio backend. Nothing else creates threads.

Pause (R13): the editor keeps the engine in `simulation_mode::PAUSED` while editing (fixed loop and
non-`RUNS_WHILE_PAUSED` UPDATE systems skipped; TRANSFORM/SYNC/EXTRACT still run so edits render) and
`script_mode::EDIT` (no managed instances). Play = `RUNNING` + `PLAY`. Step = `step_fixed(n)`.

---

## 4. Module contracts: core summary and pointers to the design notes

Every module now has a design note (`10_core.md` ... `15_bindings.md`, R65). The notes are binding and more
detailed than this section; where a note and this section differ in detail the note wins, where a note contradicts
`09_cross_area_resolutions.md` that file wins. 4.1 keeps the consolidated core contract because every other module
is written against it; 4.2-4.6 are one-paragraph pointers.

### 4.1 `core` (R30, R16, R25, R31; full design `10_core.md`)

- `result<T>` / `result<void>` (alias `status`) / `error` (`category()`, `code()`, `message()`, `is(code)`). Each
  module declares `enum class <module>_error : u16` and binds it with `CONJURE_DECLARE_ERROR_CATEGORY(enum,
  "name", name_fn)`; errors are built with the generic `core::make_error(<module>_error, std::string_view detail = {})`
  or `make_error(code, fmt, args...)` (no per-module `make_error` overload, `10_core.md` C3, R63); `core::error_code`
  holds the generic ones (`UNSUPPORTED`, `NOT_FOUND`, `INVALID_ARGUMENT`, `INVALID_STATE`, `ALREADY_EXISTS`,
  `OUT_OF_RANGE`, `OUT_OF_MEMORY`, `IO_FAILURE`, `PARSE_FAILURE`, `TIMEOUT`, `PERMISSION_DENIED`, `NOT_IMPLEMENTED`, ...).
- `CONJURE_ASSERT` / `CONJURE_VERIFY` / `CONJURE_UNREACHABLE`; `core::set_assert_handler(handler)` where the
  handler returns `assert_action::CONTINUE` or aborts (tests wrap it as `conjure::testing::expect_assert`).
- Logging: `CONJURE_DECLARE_LOG_CATEGORY(name)` in `<module>/log_category.h`, `CONJURE_DEFINE_LOG_CATEGORY(name,
  "display-name")` in one `.cpp`; `CONJURE_LOG_TRACE/DEBUG/INFO/WARN/ERROR(category, fmt, ...)`; `log::add_sink` /
  `log::remove_sink` taking a `std::shared_ptr<log_sink>` (`console_log_sink`, `file_log_sink`, `callback_log_sink`);
  `log_level` (`ERR`, not `ERROR`, which is a Windows macro).
- `core::jobs::job_system`: `start(job_system_desc)` (`workerCount` 0 = every job runs inline on the caller), `stop()`,
  `schedule(fn, priority = NORMAL) -> job_handle`, `wait(job_handle)` (executes queued jobs on the caller),
  `wait_idle()`, `parallel_for(count, grain, fn(begin, end))` (+ `parallel_for(count, fn(index))`),
  `worker_count()`, `thread_index()`; free functions `core::jobs::is_main_thread()`, `core::jobs::set_main_thread()`,
  `core::jobs::post_to_main_thread(fn)` / `drain_main_thread_queue()` (drained by the engine before FRAME_BEGIN),
  class `core::jobs::dedicated_thread(name, fn(std::stop_token))`.
- `core::memory`: `arena` (stable addresses until `reset`), `pool_allocator(blockSize, alignment, blocksPerSlab)`,
  `frame_allocator` (per-thread blocks, `overflow_count()`), `counters()` (the `allocation_counters` struct of three
  atomics) and `snapshot() -> memory_snapshot`, `tracked_allocate` / `tracked_free`; no `malloc` elsewhere.
- Containers: `slot_map`, `small_vector<T, N>`, `stable_vector<T>`, `flat_hash_map<K, V>`, `flat_map<K, V>`,
  `bit_vector`, `string_id` (constexpr FNV-1a, interned name for debugging), `radix_sort_u64`.
- `core::hash::fnv1a64(std::string_view)` / `(std::span<const std::byte>)`, constexpr, published test vectors
  shared with `Conjure.Managed`.
- `core::reflect`: `type_id` (from the registered name, stable across shared libraries), `type_info { name,
  size, alignment, fields }`, `field_info { name, type, offset, kind, attributes (range, tooltip, hidden),
  default }`, kinds for primitives, math types, `std::string`, `string_id`, `uuid`, nested structs,
  `std::array`, `ENTITY` (detected by `type_id_of<ecs::entity>()`), `ASSET_REF`; `type_info_of<T>()`,
  `type_id_of<T>()`, `type_registry` (enumeration for the inspector), runtime `type_info` construction from a
  field list (dynamic C# components), per-type `to_json` / `from_json`.
- `core::json`: `value` (read API), `document` (owning, deep copy, pointer-style patching), `writer`
  (`object/array/key/value`, `write_raw_json`), `parse`, `to_string(value, canonical)` (sorted keys, shortest
  round-trip floats, minified).
- `core::vfs::file_system` interface (`read_file`, `write_file`, `write_file_atomic`, `stat`, `enumerate(dir,
  enumerate_flags, callback)`, `rename`, `remove`, `remove_all`, `create_directories`, `open_read -> file::read_at`),
  `native_file_system`, `memory_file_system`, and `mount_table` (itself a `file_system` routing `scheme://path`
  URIs: `mount`, `unmount`, `find_mount`, `split`). Mount names: `project://`, `assets://`, `engine://`, `cooked://`,
  `modules://<name>/` (R24). Lexical helpers in `vfs::path_utils`.
- `uuid` (v4 `generate()` thread-safe, `parse`, `to_string`, hash, `<=>`), `clock` / `steady_clock` /
  `manual_clock::advance()` / `sleep_until`, `process` (spawn, capture stdout/stderr, wait, exit code),
  `dynamic_library` (RAII dlopen/LoadLibrary, `symbol()`, `decorate_name`), `image` (RGBA8, PNG load/save),
  `event<Args...>` (multicast, RAII `event_subscription`), `delegate<Signature>`, `event_bus`,
  `random::default_engine()` (seedable), `math` (glm aliases `vec2/3/4`, `uvec2`, `quat` stored x,y,z,w via
  quaternions stored x,y,z,w with a `static_assert` (R58), `mat3/4`, `transform`, `aabb`, `frustum`, `ray`, `urect`), `platform_detect.h`,
  `api.h` (`CONJURE_API`), generated `build_info.h` (`CONJURE_VERSION_*`).

### 4.2 `platform` (`11_platform.md`; R30, R33, R56, R57, R59, R60)

One abstract `window` created by `platform::create_window(window_desc)` with backends `glfw` (GLFW 3.4, main thread
only, `CONJURE_PLATFORM_GLFW`) and `null` (virtual window, no OS calls, in-memory clipboard/cursor mode/gamepad
names, test controls `push_event` / `simulate_*`); `pump_events() -> span<const event>` of trivially copyable
variants; a backend-independent `input` whose `apply` and `inject` are the same state machine and whose `end_frame()`
publishes the immutable `input_state` (512-key bitsets, mouse, 4 gamepads x 6 axes) that the scripting frame block
mirrors. Key codes follow GLFW numbering (`key::DEL`, never a Windows macro name).

### 4.3 `audio` (`12_audio.md`; R7, R43, R50, R52)

`audio_engine(audio_settings)` over `miniaudio` or `null` (construction never fails; falls back to `null`), one
backend-agnostic mixer fed by lock-free command/event rings, 256 voices, group gain tree, 3D attenuation/pan/Doppler,
`conjure.audio.clip` asset type with a CPU-only loader (`.wav .ogg .mp3 .flac`, PCM or streamed). Depends on `core`
and `assets`, never on `ecs`: components `audio_source` (incl. the hidden `playbackRequest`/`isPlaying` fields the
bindings and `engine.audio_sync` exchange) and `audio_listener` are plain reflected structs that the engine's
`audio_subsystem` registers and syncs (`engine.audio_sync` SYNC, `engine.audio_finish_one_shots` FRAME_END).

### 4.4 `scene` (`13_scene.md`; R3, R25, R29, R45, R46, R47, R55)

One `ecs::world` per `scene::scene`; hierarchy as `parent { entity, siblingIndex }` + TRANSIENT `children`,
depth encoded as `depth_tag<1..31>` tag components so `scene.propagate_transforms` (TRANSFORM, PARALLEL per depth
level, the single owner of propagation) computes `world_transform` and `render_world_transform` in one pass;
`scene.apply_hierarchy_changes` (TRANSFORM, EXCLUSIVE) repairs raw `parent` edits and the bindings'
`deferred_set_parent`; `scene.apply_pending_loads` (FRAME_BEGIN) and `scene.destroy_pending` (FRAME_END) complete the
four systems. `interpolated_transform` is defined here but added, written and removed by physics. JSON `.scene.json`
/ `.prefab.json` files over `ecs::write_archive_json`, fully materialised prefab instances (`prefab_instance`,
`prefab_link`), in-memory `scene_snapshot` for play/stop, `scene_manager` owning residency and the
`on_before_unload` event.

### 4.5 `physics` (`14_physics.md`; R6, R44, R47, R48, R49, R53, R59)

Jolt 5.3 fully encapsulated in `physics_world(physics_settings, jobs &)` (`JPH::JobSystem` over `core::jobs`,
allocations through `core::memory`, deterministic for any worker count); plain reflected components `rigid_body`,
five colliders (`convex_collider`/`mesh_collider` referencing the `conjure.physics.collision_mesh` asset type the
module registers), `physics_material`, `character_controller`; bodies created by observers at the next
`physics.push_transforms`, every gameplay mutation a queued command; 32 named layers from `project.json`
`extensions.physics_layers`; entity-keyed, allocation-free queries; `contact_events()` span sorted by body-id pair
and filled by `physics.contact_events` (FIXED_END). Systems: `physics.push_transforms`, `physics.step`
(PHYSICS_STEP, `MAIN_THREAD`), `physics.pull_transforms`, `physics.contact_events` (FIXED_END).

### 4.6 `bindings` (`15_bindings.md`; R5, R26, R51, R54, R61, R64)

`bindings::scripting_bindings_subsystem` (startup step 16) fills the ten bindings-owned groups of the one
`native_api.inl` (`transform_*`, the hierarchy half of `entity_*`, `scene_*`, `prefab_*`, `physics_*`, `audio_*`,
`input_*`, `asset_*`, `diagnostics_*`, `modules_*`; 106 entries, stubs returning `-NOT_SUPPORTED` for compiled-out
or disabled subsystems so the table is always complete) before `script_runtime::initialize`, wraps every slot in a
counting thunk when `!CONJURE_DISTRIBUTION` (`bindings::call_counters()`), routes structural requests made inside
a DOD query scope into `script_runtime::deferred_commands()` (R5), and ships the C# facades, the `[NativeComponent]`
mirrors (field lists owned by 12/13/14, verified by `type_info::layout_hash()`), `AssetRef<T>`, `Conjure.Diagnostics`
and the read-only `Conjure.Modules.Registry`.

---

## 5. Asset data flow (`05_assets.md`, R10, R23, R25, R34)

```
source file + <file>.meta (uuid, importer, settings, sub-asset uuids)         [editor/dev only]
   | asset_database::scan (hash source + side files, stale check)              main thread
   | asset_database::import  -> importer.import() on a job (deterministic)     worker threads
   v
.conjure/cache/<platform>/blobs/<xx>/<uuid>.cab    cooked_blob {header, deps, payload}, hash-validated
   | asset_manager::load(id) -> QUEUED -> LOADING (source.read + loader.load_cpu on a job)
   | dependencies auto-loaded; FINALISING waits until every dependency is terminal
   | loader.finalise on main (CPU payload for renderer types) -> publish (atomic pointer swap, version++)
   v
asset_slot (never freed) <- asset_ref<T>::get() one atomic load, valid until end of next frame
   | asset_manager events LOADED / RELOADED / UNLOADED
   v renderer_subsystem: renderer::upload_*(id, payload) (copies bytes) / release_*     (R10)
   v audio clips: CPU-only loader; the published `audio_clip` payload is self-contained and voices share its
     `pcm_buffer` through shared_ptr - no `audio_engine` takes part in finalise          (R43)
   v collision meshes: the physics loader builds the Jolt shape on the load job; finalise publishes it (R44)
```

Who registers what: `renderer::register_asset_types` (texture, mesh, material, model, environment; glTF,
texture, `.cmat`, `.hdr` importers; IBL baked on the CPU inside the environment importer, R34),
`audio::register_asset_types` (`conjure.audio.clip`, `.wav .ogg .mp3 .flac`), `scene::register_asset_types`
(`conjure.scene.scene` `.scene.json`, `conjure.scene.prefab` `.prefab.json`, R45), `physics::register_asset_types`
(`conjure.physics.collision_mesh`, `.gltf .glb .obj`, R44), `scripting` (`conjure.scripting.script_source`,
`EXCLUDE_FROM_PACK`), plugins via `assets::importer_extension_desc` / `asset_type_extension_desc` records
(R37). Component `asset_ref<T>` fields serialise as uuid strings and are bound by the scene bind pass (R25).
Residency is owned by handles: `scene_manager` holds the scene's `asset_load_batch`, the editor holds what it
previews; `collect()` runs at scene unload. Hot reload: polling watcher (job) -> `hot_reload_controller::update`
(main, frame step 5) -> re-import dirty -> reload dependents in depth order; dependents observe
`asset_ref::version()` (mesh instances compare material versions during extraction).

---

## 6. The scripting boundary (`03_scripting.md`, R4-R6, R26-R29, R36)

```
C# game assembly (collectible ALC)  <->  Conjure.Managed (default ALC)  <->  conjure_scripting + conjure_bindings
         ScriptBehaviour instances         [UnmanagedCallersOnly] entries          conjure_native_api (X-macro table)
         Entity/Transform structs           conjure_managed_api                     conjure_frame_block (shared memory)
```

- One CLR per process (`hostfxr_runtime`), two hosting paths (framework-dependent, self-contained), one
  handshake validating size + version + layout hash on both tables.
- Per phase one native -> managed call (`run_phase`), creates/destroys/collisions batched; `Time`/`Input`
  read the shared `conjure_frame_block` written by the engine before FRAME_BEGIN.
- All managed execution is on the main thread inside the six `EXCLUSIVE` scripting systems (R27); scripts may
  mutate the world directly except inside a DOD query scope, where the bindings defer structural ops (R5).
- `script_component` (4 bytes) points into `script_store`; its custom ops/serializer deep-copy and serialise
  the script set, so scene files, prefabs and snapshots carry scripts without the scene module knowing
  scripting exists (R29).
- `[EcsComponent]` structs become ECS dynamic components via `register_dynamic`; the ABI type id is the ECS
  `component_id` (R4). Mirrors of native components are verified by layout hash.
- Hot reload: load new ALC -> collect fields -> destroy -> unload -> recreate (`OnCreate` again, not
  `OnStart`); plugin assemblies share the game ALC (R36).
- Export: `dotnet publish` of `Conjure.Bootstrap` + `dotnet build` of the game scripts into `<export>/managed`,
  mode from `project.json` `scripting.export_mode` (R39).

---

## 7. Editor and AI control path (`04_editor.md`, R11-R15)

```
agent / UI panel / --exec file
   -> protocol_server (tcp 127.0.0.1:47820+, stdio, loopback)  newline-delimited JSON, id echoed
   -> command_dispatcher: registry lookup -> schema validation -> preconditions -> handler (all-or-nothing)
   -> handler mutates through editor_context: scene_manager -> scene -> ecs::world,
      component_registry + ecs::component_to_json/from_json, entity_archive, asset_database, script_runtime,
      module_manager::request_*, play_controller -> application::set_simulation_mode / step_fixed
   -> command_outcome {result, inverse, replay} -> undo_stack (frozen while playing) ; events broadcast
```

Entities cross the protocol as scene guids or hierarchy paths, never as `ecs::entity`. The catalogue
(`04_editor.md` 4.4) is self-describing (`editor.schema`) and must be able to author every registered
component type (`08_testing.md` D8). `play.start` snapshots the scene (`scene_serializer::capture`),
`play.stop` restores it. Picking is CPU-side over per-entity bounds and `mesh_asset::cpu_geometry()` (R11);
screenshots render into a `render_target` and `read_back` synchronously (R12). Headless:
`conjure_editor --headless --exec commands.json --project <dir>` runs the same dispatcher over a `null`
platform/rhi/audio application and pumps `step_frame()` for async commands; exit codes 0/1/2/3. Module
tooling: `module.*` commands only enqueue requests; `editor_module_consumer` applies module panels and
commands (R14).

---

## 8. Export path (`06_engine_loop.md` 3.6, R21)

`conjure_editor --export <dir>` and the `project.export` command both call `engine::export_pipeline::run`:

1. Build the project's native game module in Distribution (if `<project>/module.json` declares a binary).
2. `assets::asset_cooker::cook` -> `<dir>/cooked/content.cpak` + `manifest.json` (closure from the startup
   scene and `always_include`, deterministic, all-or-nothing).
3. `dotnet publish managed/Conjure.Bootstrap` (framework-dependent or self-contained) and `dotnet build` of
   the game scripts -> `<dir>/managed/` (skipped when scripting is disabled).
4. `modules::module_manager::export_layout(<dir>, DISTRIBUTION)` -> `<dir>/modules/<name>/{module.json, bin/<triplet>/, managed/}`.
5. Copy the Distribution `conjure_runtime`, `conjure_shared` and `shaders/` (index.json + DXIL/SPIR-V blobs).
6. Write `<dir>/project.json` with `"cooked": true`; the runtime then mounts only `engine://` and `cooked://`.

`conjure_runtime --project <dir>` runs the result with no editor code linked (CI greps the Distribution
binary for `conjure::editor` and `ImGui` symbols). `tests/integration/test_export_roundtrip.cpp` and the
nightly `distribution-export` job prove it headless.

---

## 9. Build and configuration matrix

| Axis | Values | Notes |
|---|---|---|
| Configurations | `Debug`, `Development`, `Distribution` | Distribution: `CONJURE_DISTRIBUTION`, asserts out, editor off, `imgui_renderer`/`imgui_layer`/`debug_overlay_layer` not compiled. A single-config Distribution tree forces `CONJURE_BUILD_EDITOR` off; a multi-config tree (`windows-vs`) offers the Distribution configuration only when `CONJURE_BUILD_EDITOR=OFF` (`cmake/ConjureConfigs.cmake`), so no configuration anywhere links editor code or ImGui into `conjure_shared` |
| Platforms / compilers | Windows 11 MSVC 14.44+; Ubuntu 24.04 GCC 13+ / Clang 16+; macOS 14 AppleClang (MoltenVK) | CMake >= 3.28, Ninja, presets `windows-msvc-*`, `linux-gcc-*`, `linux-clang-*`, `macos-*` |
| Options (all default ON) | `CONJURE_WITH_RENDERER`, `_PHYSICS`, `_AUDIO`, `_SCRIPTING`, `CONJURE_BUILD_EDITOR`, `_RUNTIME`, `_TESTS`, `_BENCHMARKS`, `_SAMPLES`, `_MANAGED`, `CONJURE_WARNINGS_AS_ERRORS`, `CONJURE_RHI_D3D12` (Windows only; `CONJURE_RHI_VULKAN` defaults OFF there), `CONJURE_RHI_VULKAN` (Linux, macOS) | A disabled module leaves its `engine_context` pointer null; `module_feature_flags` mirror them for plugins; `CONJURE_BUILD_EDITOR` reaches code as the `conjure::platform_defines` define `CONJURE_BUILD_EDITOR=1` outside Distribution |
| Extra options | `CONJURE_SPIRV_COMPILER=glslang\|dxc` (`cmake/ConjureShaders.cmake`, with the cache paths `CONJURE_DXC_EXECUTABLE` / `CONJURE_GLSLANG_EXECUTABLE`), `CONJURE_TEST_GOLDEN_IMAGES` (OFF), `CONJURE_ENABLE_ASAN/UBSAN` (root) and `CONJURE_ENABLE_TSAN` (`cmake/ConjureSanitizers.cmake`, GCC/Clang, exclusive with ASan) | sanitizer builds set `CONJURE_WITH_SCRIPTING=OFF` |
| Module options (R59) | `CONJURE_PLATFORM_GLFW` (ON; OFF = null-only platform library for servers, `11_platform.md` 4.5; the editor requires ON), `CONJURE_PHYSICS_CROSS_PLATFORM_DETERMINISTIC` (OFF; maps to Jolt `CROSS_PLATFORM_DETERMINISTIC` and adds `-ffp-contract=off` / `/fp:precise` to the physics module, `14_physics.md` P16; root `CMakeLists.txt` since wave 3) | neither is enabled or disabled by CI presets; `is_backend_compiled(GLFW)` reflects the first |
| Linkage | every module `conjure_<module>` static via `conjure_add_module()` (core areas `conjure_core_<area>` via `conjure_add_core_area()`), compiled with `CONJURE_SHARED_EXPORTS` and hidden visibility; umbrella `conjure_shared` (SHARED, whole-archive of every registered static library incl. `engine`, `bindings`, `editor_core` and `editor_ui` outside Distribution, created by `conjure_finalize_engine()` from the root `CMakeLists.txt`, `/MD` only) exports exactly the declarations that carry `CONJURE_API` (`conjure/core/api.h`, rules in `07_modules.md` 5): 5,283 symbols in Debug, checked by `tools/check_exports.py` (CTest `exports.check_annotations`, `exports.count_shared_library`, limit 52,428 of 65,535); `conjure_editor`, `conjure_runtime` and game modules link `conjure_shared` and nothing else from the engine; tests link the static library or `LINK_ENGINE_SHARED` (`conjure::shared` + `conjure::test_support_shared` + `conjure::test_support_engine_shared`, C-runtime allocation functions) | R17; game modules via `conjure_add_game_module()` to `bin/<platform>-<arch>-<config>/`; `tools/check_distribution_symbols.py` proves the Distribution runtime free of editor and ImGui symbols |
| Shaders | HLSL 2018 subset, `conjure_add_shaders()` -> DXIL (`dxc`; the depfile needs a second dxc run because `-MF` compiles nothing) + SPIR-V (`glslang` 15.1.0 fetched, or `dxc -spirv`) + `shaders/index.json`; `engine/shaders` is added by `engine/CMakeLists.txt` when `CONJURE_WITH_RENDERER` | `conjure_shaders` is a dependency of `conjure_rhi_tests` and (wave 3) of `conjure_renderer` and its tests |
| Managed | `conjure_add_managed_project()` (`dotnet build`, stamp file, explicit `<Compile>` items), `Conjure.Managed`, `Conjure.Bootstrap`, `Conjure.Templates/Game`, `Conjure.TestScripts` | `.NET 8` SDK required when `CONJURE_WITH_SCRIPTING`; CI installs it on all runners |
| Third-party (pinned in `third_party/CMakeLists.txt`) | GLFW 3.4, NVRHI (commit), glm 1.0.1, Jolt 5.3.0, miniaudio 0.11.22, nethost (the installed .NET 8+ host pack), cgltf 1.14, stb (commit), yyjson 0.10.0, Dear ImGui docking 1.91.9b (>= 1.92 wanted for the `ImTextureData` path of `imgui_renderer`), ImGuizmo (commit), doctest 2.4.11, glslang 15.1.0 | exposed as `conjure::thirdparty::<name>`; never included from public headers (glm via `core/math.h` excepted); on MSVC the scripting module links the `nethost.lib` import library instead of the static `libnethost.lib` (`/MT`) |
| CI | PR: `windows-msvc-debug`, `windows-msvc-dev` (+export smoke), `linux-gcc-debug`, `linux-clang-sanitize`, `macos-debug`, `style-and-headers`; nightly: TSan, golden (WARP / lavapipe), benchmarks (ci class), distribution export, full matrix | `08_testing.md` 7.3; skips fail CI unless allow-listed |

---

## 10. Where to find what

| Topic | Document and section |
|---|---|
| Fixed decisions, repository layout, waves | `design/00_brief.md` |
| Entity ids, archetypes, chunk layout, queries, change detection | `design/01_ecs.md` 2-3 |
| Scheduler, phases, `system_flags`, `frame_info`, module extension records | `design/01_ecs.md` 4.7, `design/06_engine_loop.md` 2.4 |
| Command buffer flush semantics, archives, JSON serialisation | `design/01_ecs.md` 3.4-3.7, 4.8 |
| `world_hash`, component `types()`, `EDITOR_HIDDEN` | `design/01_ecs.md` 4.2, `design/08_testing.md` 3.4 |
| RHI device/swapchain/shader library, null device | `design/02_renderer.md` 4.1, 7 |
| Render graph, passes, shadows, SSAO, HDR, 2D, ImGui backend | `design/02_renderer.md` 2.7-3.11 |
| `render_scene` contract, `begin_extract/end_extract/render_frame` | `design/02_renderer.md` 2.3, 4.2 |
| Shader build (`conjure_add_shaders`) | `design/02_renderer.md` 3.12 |
| glTF import, renderer asset types, CPU IBL bake | `design/02_renderer.md` 3.6, 4.2 |
| .NET hosting sequence, ABI tables, frame block | `design/03_scripting.md` 2.1, 3.1 |
| Script lifecycle, collisions, fields, inspector contract | `design/03_scripting.md` 3.2-3.6 |
| Hot reload (C#), managed build, DOD `Span<T>` | `design/03_scripting.md` 3.8-3.10 |
| Command model, undo, protocol wire format, error codes | `design/04_editor.md` 2, 3.3-3.4 |
| Command catalogue, events, grid-game walk-through | `design/04_editor.md` 4.4 |
| Play/pause/step, picking, screenshots, headless runner, transports | `design/04_editor.md` 3.7-3.12 |
| Asset identity, `.meta`, cooked blob, pack format | `design/05_assets.md` 2 |
| Async load pipeline, publish/retire, hot reload, cook | `design/05_assets.md` 3 |
| Frame sequence, startup/shutdown order, pacing, interpolation | `design/06_engine_loop.md` 3.1-3.5 |
| `engine_config`, `project.json` schema, command line, exit codes | `design/06_engine_loop.md` 2.3, 2.10-2.12 |
| Export pipeline | `design/06_engine_loop.md` 3.6; here section 8 |
| Layers, subsystems, `engine_context` | `design/06_engine_loop.md` 2.5, 2.8-2.9 |
| Module manifests, ABI signature, load/unload/reload, consumers | `design/07_modules.md` 2-4 |
| `conjure_shared`, `CONJURE_API`, `conjure_add_game_module` | `design/07_modules.md` 5 |
| Test support library, fixtures, skip discipline, benchmarks, CI | `design/08_testing.md` 2-4, 7 |
| Required tests per module, definition of done | `design/08_testing.md` 5.2, 7.4 |
| `feature_test` scene content | `design/08_testing.md` 7.2 |
| Cross-area contradictions and their fixes, remaining risks | `design/09_cross_area_resolutions.md` |
| Core API (`result`/`error`, jobs, memory, containers, reflection + `layout_hash`, json, vfs, math) | `design/10_core.md`; summary here, section 4.1 |
| Window, events, input state machine, GLFW/null backends, key numbering | `design/11_platform.md` 2-3, 7 |
| Mixer, voices, groups, spatialisation, clip asset, `audio_source` fields, engine audio sync | `design/12_audio.md` 2-3, 8 |
| Hierarchy, depth tags, propagation algorithm, scene/prefab file formats, snapshots, `scene_manager` | `design/13_scene.md` 2-3 |
| Jolt encapsulation, physics components and settings, layers, body lifecycle, contacts, queries, collision mesh asset | `design/14_physics.md` 2-3 |
| Native entry list, ABI records, status codes, C# facades, `[NativeComponent]` mirrors, call counters, exclusions file | `design/15_bindings.md` 2, 3.6, 10 |
| Who adds/writes `interpolated_transform`; audio playback through component fields | `design/14_physics.md` 3.3 (R47); `design/12_audio.md` 2.6/8 (R50) |
| Coding rules | `CONVENTIONS.md`, `CODE_STYLE.cpp` (struct fields camelCase per R31) |

---

## 11. Implementation wave plan

Each wave is implementable in parallel across its modules once the previous wave's public headers exist.
"Inputs" are the documents an implementer must read in full before writing code; `ARCHITECTURE.md`,
`CONVENTIONS.md`, `CODE_STYLE.cpp` and `09_cross_area_resolutions.md` are inputs to every wave. The
definition of done for a module is `08_testing.md` 7.4; the items below add the wave-specific gates.

### Wave 1 - foundation

| Module | Inputs | Definition of done |
|---|---|---|
| `core` | `10_core.md` (all); section 4.1; the "Needs from core" paragraphs of 01-08 and 11-15; `08_testing.md` 2.2, 5.2 (core row); R58, R63 | every API in `10_core.md` exists with `tests/core/test_<header>.cpp`; `conjure_header_check(core)` passes; jobs tested with 0 and 8 workers; json canonical output stable; `math::quat` storage asserted x,y,z,w; `type_info::layout_hash` test vectors published; allocation-free `parallel_for` dispatch, `slot_map` lookup, `string_id` compare |
| `conjure_test_support` + CMake infrastructure | `08_testing.md` 2-4, 10 steps 1-5 | `conjure_add_test_module`, `test_main.cpp`, `tracked_global_new.cpp`, `log_capture`, `expect_assert`, `memory_guard`, `allocation_scope`, `temp_directory`; `tests/support/tests` green; `.github/workflows/ci.yml` with the six PR jobs green on an empty engine |
| Build skeleton | `00_brief.md` layout; `07_modules.md` 5 (`conjure_add_module` additions, `CONJURE_API`); `02_renderer.md` 3.12 (`conjure_add_shaders` stub) | root `CMakeLists.txt`, presets, `third_party/CMakeLists.txt` with pinned tags, `ConjureModule.cmake`, `ConjureTesting.cmake`, `ConjureWarnings.cmake`, `ConjureSanitizers.cmake`; configure + build on all three CI platforms |

Wave 1 exit: `ctest` green on all PR jobs; `tools/check_style.py` and header checks pass.

### Wave 2 - independent siblings (all depend on `core` only; `rhi` also on `platform`)

| Module | Inputs | Definition of done |
|---|---|---|
| `ecs` | `01_ecs.md` (all), R1-R5, R25, R32, R40 | every test file of `01_ecs.md` 10 plus `test_world_hash`; `bench_ecs` meets 0.1 ms for 10k transform integrations on 4 workers; `scheduler::finalise`, `run_phase_filtered`, `component_type_desc.h`, `component_to_json/from_json` present |
| `platform` | `11_platform.md` (all); `06_engine_loop.md` 7-8; `08_testing.md` 5.2 (platform row), 8.4; R56, R57, R59, R60 | `null` and `glfw` backends pass the same conformance test (`[window]` for glfw); `inject` edge/level semantics identical; every header has its test file; `conjure_platform_tests` green without a display |
| `rhi` | `02_renderer.md` 1, 4.1, 7, 9 steps 1-4; `08_testing.md` 8.4 | null device passes `test_null_device`, `test_shader_library`, `null_device_stats` recorded; D3D12/Vulkan devices create and present a cleared swapchain locally (`[gpu]`); `conjure_add_shaders` compiles one shader on all CI platforms |
| `assets` | `05_assets.md` (all), R10, R23-R25, R33-R37 | all `tests/assets` cases; `bind_slot`; `importer_extension_desc`/`asset_type_extension_desc`; two cooks byte-identical; 1,000-blob async load < 250 ms main-thread |
| `audio` | `12_audio.md` (all); `05_assets.md` 2-4, 8; `08_testing.md` 5.2 (audio row), 8.4; R43, R50, R52 | `null_backend_stats` drive every test; WAV decode from in-test clips, ogg/mp3/flac behind `precondition::DOWNLOADED_ASSETS` (R62); `bench_audio` thresholds; miniaudio backend plays locally |
| `modules` | `07_modules.md` (all), R14, R17-R20, R36, R37 | static-module tests green everywhere; shared fixtures (`module_alpha` ...) load on all three CI platforms; `conjure_shared` builds and `conjure_modules_tests` links it; `check_exports.py` clean |

Wave 2 exit: `conjure_shared` links all wave-2 modules; `tests/modules/sample_plugin` loads in a test host.

### Wave 3 - scene first, then renderer, physics, scripting in parallel

| Module | Inputs | Definition of done |
|---|---|---|
| `scene` (3a) | `13_scene.md` (all); `01_ecs.md` 8 (offers to scene); `06_engine_loop.md` 3.2; `04_editor.md` 8 (scene row); `05_assets.md` 8 (scene); R3, R25, R29, R45-R47, R55 | `tests/scene` per `13_scene.md` 10; depth-8 chain and 10,000-entity propagation allocation-free after frame 2; save -> load -> save byte-identical; capture + restore 10k < 50 ms; prefab nesting; `on_before_unload`; `deferred_set_parent` and `duplicate_subtree` for the bindings |
| `renderer` (3b) | `02_renderer.md` (all), R7-R12, R34; `05_assets.md` 8 (renderer) | all `tests/renderer` cases on the null device; `test_renderer_null_frame` 10k instances; real-device image of `feature_test` early scene locally; `bench_render_null_frame` < 3 ms; `register_asset_types`, `set_imgui_draw_data`, `begin_extract/end_extract/render_frame` |
| `physics` (3b) | `14_physics.md` (all); `06_engine_loop.md` 3.2, 8; `03_scripting.md` 3.5; `05_assets.md` 8 (physics); `08_testing.md` 5.2 (physics row); R6, R44, R47-R49, R53 | free fall analytic; stack settles; contact events exactly once per pair in sorted order; `contact_events()` span; Jolt allocations visible to `allocation_scope`; `physics.step_10k_sleeping` allocation-free; 0 vs 4 workers bit-identical; `collision_mesh` import byte-identical |
| `scripting` (3b) | `03_scripting.md` (all), R4-R6, R26-R29, R36, R39 | null-host tests on every platform; `[dotnet]` tests pass where the SDK exists and CI fails on skips; reload v1->v2->v3 preserves fields; `10k_scripts_update_phase` < 2 ms; generator check `conjure_scripting_api_generated_up_to_date` |

Wave 3 exit: `tests/integration/test_assets_renderer_headless.cpp`, `test_scripting_scene.cpp`,
`test_assets_scene.cpp` green.

### Wave 4 - engine, bindings, editor, runtime

| Module | Inputs | Definition of done |
|---|---|---|
| `engine` | `06_engine_loop.md` (all); the "Offers to engine" sections of 11-15 (`platform_subsystem`, `audio_subsystem` incl. `engine.audio_sync` over `playbackRequest`, `scene_subsystem`, `physics_subsystem` with `physics_settings::from_json`, `model_prefab_converter`); R1-R2, R7-R10, R13, R18-R24, R27, R33, R38-R39, R21 (export), R46-R47, R50-R53 | `tests/engine` per 10; `test_application_headless` (construction <= 5 ms with null backends); `test_phase_order` exact sequence; startup unwind; `export_pipeline` round trip; `bench_engine_frame` under the `desktop` thresholds |
| `bindings` | `15_bindings.md` (all); `03_scripting.md` 2.1, 3.9, 8; `12_audio.md` 2.6/8, `13_scene.md` 8, `14_physics.md` 8 (offers to bindings); `05_assets.md` 8; `08_testing.md` 5.2 (bindings row), 7.2 (call counters); R5, R26, R48-R51, R54-R58, R61, R62, R64 | every native entry filled (stubs for compiled-out groups); generated C# manifest equals the native table; every mirror of 15 2.6 resolves to a non-zero id; every counter increments in `feature_test` minus `tests/binding_exclusions.json`; `Span<T>` scope test |
| `editor_core`, `editor_ui`, `conjure_editor` | `04_editor.md` (all), R11-R15, R35, R40 | all `tests/editor` cases; `test_command_catalogue` fixture per command; `editor.schema` dump matches `docs/generated/editor_protocol.json`; `test_ai_builds_grid_game` and `test_ai_protocol_e2e` green; `test_editor_process` runs the real executable |
| `conjure_runtime` | `06_engine_loop.md` 4.2; `15_bindings.md` 3.1 (`add_subsystem(scripting_bindings_subsystem)` before `initialise()`) | boots an exported folder headless 60 frames, exit 0; missing `project.json` -> exit 2; Distribution binary has no editor/ImGui symbols |
| `conjure_test_support_engine` | `08_testing.md` 2.3, 4.4-4.6, 11 | `headless_engine`, `input_script`, `world_builder`, `protocol_client`, `golden_image` with their self-tests |

Wave 4 exit: all `tests/integration` scenarios of `08_testing.md` 6.1 green on the PR matrix; `windows-msvc-dev`
exports `feature_test` and runs the result 120 frames.

Status (wave 4 integration, `09_cross_area_resolutions.md` 3): `engine`, `bindings`, `editor_core`, `editor_ui`,
`conjure_editor`, `conjure_runtime`, `conjure_test_support_engine` and `tests/benchmarks` are implemented; every 6.1
scenario exists in `tests/integration/` (`test_module_plugin.cpp` in its own `LINK_ENGINE_SHARED` executable) and
the engine scenarios of `06_engine_loop.md` 10 next to them. Until `samples/feature_test` lands (wave 5) the
export smoke of `windows-msvc-dev` and `test_export_roundtrip.cpp` use `tests/assets/projects/minimal`. The gates
that were measured rather than met (allocation-free frames, the physics stack tolerance, off-screen rendering) are
item 15 of `09_cross_area_resolutions.md` 4.

### Wave 5 - samples and completeness

| Deliverable | Inputs | Definition of done |
|---|---|---|
| `samples/feature_test` | `08_testing.md` 7.2, D8-D9; `04_editor.md` 4.4 | generated by replaying `commands/build_feature_test.json`; every non-hidden component type present; every binding counter > 0 (exclusions justified); `checks().all_passed()` with >= 120 checks; committed scene hash-equal to the replay |
| `samples/stress_test` | `00_brief.md` requirement 2; `08_testing.md` 7.1 | 10,000 dynamic entities (transform, physics body, or script) at 60 Hz on the reference desktop; `engine.full_frame_10k_*` thresholds pass |
| Golden images, nightly jobs | `08_testing.md` 6.2, 7.3 | WARP references committed via LFS; `golden-d3d12-warp` green; lavapipe `continue-on-error` |
| `AGENTS.md`, `.claude/skills/*` | `00_brief.md` layout; `04_editor.md` D15 | `editor-ai-control` skill generated from `editor.schema`; build-test, add-module, add-component, style-review skills reference the documents in section 10 |

### Wave 6 - demo games (`docs/design/17_demo_games.md`)

| Deliverable | Inputs | Definition of done |
|---|---|---|
| `samples/games/grid_sweeper`, `barrage`, `colony` (2D) | `17_demo_games.md` 2.1, 2.3, 2.4, `04_editor.md` 4.4, `03_scripting.md`, `07_modules.md` | each boots headless 600 frames with scripted input, all `Diagnostics.Check` pass, recorded `world_hash` matches, wiki tutorial written; Grid Sweeper is authored entirely through the AI protocol command file; Barrage (C#, 4,096 bullets) and Colony (native plugin module, 5,000 ants) pass their frame-budget and zero-allocation scenarios (`17_demo_games.md` G7, 4.3) after the engine prerequisites of 4.1 |
| `samples/games/rolling_course`, `corridor_strike` (3D) | `17_demo_games.md` 2.2, `14_physics.md`, `02_renderer.md` | same gates plus an exported standalone build that runs 120 frames; placeholders swap to the customer's licensed assets by file replacement |

### Cross-wave gates

- A public header change in a lower wave is a PR against that module with its own tests; higher waves never
  patch around it.
- Every new component type, binding, editor command or feature lands in the same PR with its `feature_test`
  entity, call site, command fixture or `Conjure.Diagnostics.Check` (`08_testing.md` 7.4 item 6).
- `tests/integration/test_determinism.cpp` must stay green at 0, 2 and 8 threads for every system added
  (R41 constraint on spawning).
- The design notes `10_core.md` ... `15_bindings.md` have been reviewed against `09_cross_area_resolutions.md`
  (R43-R65); an implementer of `core`, `platform`, `audio`, `scene`, `physics` or `bindings` reads the note in full
  and treats section 4 here as the summary, not the specification (R65).
- A `[NativeComponent]` mirror (`15_bindings.md` 2.6) changes in the same PR as the native struct it mirrors
  (`12_audio.md` 2.6, `13_scene.md` 2.1, `14_physics.md` 2.3); the layout-hash test is the gate (R48, R50, R55).

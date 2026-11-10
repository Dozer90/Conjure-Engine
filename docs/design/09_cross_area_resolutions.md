# Cross-area resolutions (consistency review of 01-08 and of the design notes 10-15)

Status: **binding**. Author: consistency critic / architecture owner. R1-R42 were found by reading
`01_ecs.md` ... `08_testing.md` against each other and against `00_brief.md`; R43-R65 by reading the module
design notes `11_platform.md` ... `15_bindings.md` against each other, against `10_core.md`, `ARCHITECTURE.md`
and 01-08. Each one has been fixed **in place** in the affected documents, which cite the resolution id
(`R<n>`) at the edited spot (`10_core.md` was not edited: it is being implemented; requirements on it are
recorded here, R58, R63).
When a sentence in an area document still disagrees with this file, this file wins and the area
document has a bug.

Legend for the "Kind" column: **C** contradiction (two documents say incompatible things), **D**
duplicated responsibility (two modules claim the same job), **G** gap (something every document assumes
and nobody designs), **N** naming/spelling drift of one shared API.

---

## 1. Resolution table

| Id | Kind | Area docs | Contradiction / gap | Resolution |
|---|---|---|---|---|
| R1 | C | 01, 03, 06 | ECS declared `enum class phase` with 9 values (`FIXED_BEGIN`, `PRE_RENDER`); the engine declared `ecs::system_phase` with 10 (`PHYSICS_STEP`, `SYNC`, `EXTRACT`). Scripting named a third subset. | One list: `ecs::system_phase` in `conjure/ecs/system_phase.h` with exactly `FRAME_BEGIN, FIXED_UPDATE, PHYSICS_STEP, FIXED_END, UPDATE, LATE_UPDATE, TRANSFORM, SYNC, EXTRACT, FRAME_END, COUNT`. Area F owns the list, area A owns the header. |
| R2 | C | 01, 06, 07 | Engine needed `RUNS_WHILE_PAUSED`, `frame_info`, `run_phase(world, phase, frame_info, jobs)`, `run_phase_filtered`, `finalise()`; the ECS scheduler had `run_phase(phase, float)` and none of those. Modules needed a trivially copyable `system_desc`; the ECS one holds `std::function`. | `system_flags` is `enum class { NONE, MAIN_THREAD, EXCLUSIVE, DISABLED, RUNS_WHILE_PAUSED }` (neither thread bit = PARALLEL); `frame_info` lives in `system_phase.h`; `scheduler(world &, job_system *)` offers `finalise()`, `run_phase(phase, frame_info)`, `run_phase_filtered(phase, frame_info, flags)`; `system_context::frame()`. Module records are the new trivially copyable `ecs::component_type_desc` / `ecs::system_extension_desc` in `component_type_desc.h`; the engine's `ecs_module_consumer` converts them. |
| R3 | D | 01, 06 (+ scene gap) | ECS described propagation as a root-subtree walk; the engine as depth-level parallel; both described dirty tracking differently. | `scene.propagate_transforms` (scene module, TRANSFORM, `RUNS_WHILE_PAUSED`) is the single owner. Algorithm: depth-level parallel (`parallel_for_chunks` per level), dirty = ECS `changed<local_transform>` chunk filter + scene `transform_dirty` tag + "interpolated subtree" bit; computes `world_transform` and `render_world_transform` in one pass. |
| R4 | C | 01, 03 | Scripting registered `[EcsComponent]` types through `component_register_managed` with "layout change resets data to zero"; ECS refuses `redefine_dynamic` while instances exist and uses `uint16_t` ids with 0 = `unresolved_components`; scripting used `uint32_t` with 0 = failure. | `native_api_core.cpp` parses `schema_json` into `dynamic_field_desc`s and calls `register_dynamic`; layout change with instances = `remove_from_all -> redefine_dynamic -> add_raw(zero)` + WARN (that *is* "reset to zero"); ABI `typeId` = `component_id.value` widened, 0 never issued = failure. |
| R5 | C | 01, 03 | Scripting assumed `begin/end_structural_lock` routing structural ops into the command buffer and an `on_remove` registry hook; ECS refuses structural ops while locked and offers `observe(id, REMOVED)`. | The DOD query scope holds `world::lock_iteration()`; while `world::is_locked()` the *bindings* route structural requests into the script runtime's `command_buffer`, flushed at `query_end`. `on_component_removed` is wired through `world::observe(scriptComponentId, REMOVED, ...)`. |
| R6 | G/C | 03, 06, 08 | Scripting and testing referred to an "ECS event queue" for contact events; the ECS has none. Scripting also wanted collision dispatch before FixedUpdate inside the same step, but contacts are produced in FIXED_END. | No ECS event queue. `physics_world::contact_events()` returns the span filled by `physics.contact_events` (FIXED_END, sorted by body-id pair) and cleared at the next PHYSICS_STEP. `scripting.collision_dispatch` is the first EXCLUSIVE system of FIXED_UPDATE and consumes the previous step's contacts (Unity-like ordering, one step of latency). |
| R7 | D/C | 01, 02, 06 | Engine wanted `renderer::register_systems` and `audio::register_systems`; renderer states it does not depend on `ecs` and that the engine owns EXTRACT; renderer components are reflection structs with no ECS registration owner. | Renderer and audio stay `ecs`-free. The engine's `renderer_subsystem` / `audio_subsystem` register their component types (the `CONJURE_COMPONENT` name specialisations live in the subsystem `.cpp`) and own `engine.render_sync`, `engine.render_extract`, `engine.audio_sync`, `engine.audio_finish_one_shots`. Scene, physics and scripting (which depend on `ecs`) register their own systems. |
| R8 | C | 02, 06 | Two renderer APIs: `render(const render_scene &)` + engine-owned `render_scene_pair` + `on_swapchain_resized()` + device-loss recovery (02) versus `renderer(device &, assets &)`, `begin/end_extract`, `render_frame`, `on_resize`, `on_device_lost`, parallel command-list recording, quit on device loss (06). | Renderer owns the `render_scene_pair`; `begin_extract() -> render_scene &`, `end_extract()`, `render_frame()`; `render(scene)` stays for tests; `renderer::create(renderer_desc)`; `on_swapchain_resized()`; `on_device_lost()` then the engine quits with `exit_code::DEVICE_LOST` (recovery is v2); one command list recorded on main in v1, job-parallel preparation only. |
| R9 | D | 02, 04, 06, 07 | ImGui context ownership was claimed by the renderer backend (04), by `imgui_layer` (06), and 04 proposed per-module `SetCurrentContext` while 07 forbade it. | `engine::imgui_layer` owns the single context (NewFrame/Render, input) and calls `renderer::set_imgui_draw_data(ImGui::GetDrawData())`; `renderer::imgui_renderer` only draws; ImGui is compiled once into `conjure_shared` with `IMGUI_API` exported; no per-module `SetCurrentContext`. |
| R10 | C | 02, 05 | Renderer: `upload_*` queue, CPU asset must outlive the upload, assets module "holds the ref". Assets: renderer loaders hold `rhi::device &` and create GPU resources in `finalise`. Two incompatible upload paths and a lifetime hazard with the 64 MB/frame budget. | Renderer loaders are CPU-only; the engine's `renderer_subsystem` subscribes to `asset_manager` events and calls `upload_*` (LOADED/RELOADED) / `release_*` (UNLOADED); `upload_*` copies payload bytes at enqueue, so no lifetime rule exists. `renderer::register_asset_types` is called by the same subsystem. |
| R11 | D | 02, 04 | Renderer designed a GPU `id_pass` + `request_pick/poll_pick`; editor designed CPU picking with `mesh_asset::cpu_geometry()`. | CPU picking (04) is the path (works on the `null` RHI). `id_pass`, `picking.h`, `request_pick`, `poll_pick` and `test_picking` move to the renderer's future list; `mesh_asset::cpu_geometry()` is required in Debug/Development. |
| R12 | C | 02, 04 | Editor called `renderer::request_capture(desc, callback)` (asynchronous, main-thread callback); the renderer offers only `create_render_target` + synchronous `read_back`. | `screenshot_service` renders the camera into a `render_target` view for `waitFrames + 1` frames, then `read_back` (synchronous) and encodes on a job. `null` RHI -> `NOT_AVAILABLE`. |
| R13 | C | 04, 06 | Editor used `application::create(backends)`, `run_frame()`, `set_simulation(mask)`, `request_fixed_steps(n)`, `frame_time`; engine has `application(application_desc)`, `step_frame()`, `set_simulation_mode(RUNNING/PAUSED/STEP_ONCE)`, `engine_time`. | Editor uses the engine API. Engine adds `step_fixed(uint32_t)` (next frame runs exactly n fixed steps then PAUSED) and `step_frame(double)` (tests). Play-state mapping: EDITING = `simulation_mode::PAUSED` + `script_mode::EDIT`; PLAYING = RUNNING + PLAY; PAUSED = PAUSED + PLAY. |
| R14 | C | 04, 07 | Editor: abstract `modules::editor_extension_registry` with `add_command/add_panel` passed to module entries, headers under `engine/include/conjure/editor/`. Modules: trivially copyable `panel_desc`/`command_desc` extension records consumed by `editor_module_consumer`, headers under `editor/include/`. | Data records + consumer (07 mechanism). `editor::panel_desc` and `editor::command_desc` live in `engine/include/conjure/editor/{panel_desc,command_desc}.h` (SDK-installed, no internals). `editor_extension_bridge` is renamed `editor_module_consumer : modules::module_consumer`, registered by the editor executable before `initialise()`. |
| R15 | N | 03, 04, 07, 08 | Protocol names diverged: `command_server`/`headless_session::exec` vs `protocol_server`/`headless_runner`; exit codes 0/1/2 vs 0/1/2/3; `INVALID_PARAMS` vs `INVALID_ARGS`; `query.scene` vs `scene.query`; `viewport.screenshot` vs `view.screenshot`; `modules.*` vs `module.*`; `scripts.build/reload/gc` vs `script.compile`; events `entity_created` vs `scene.changed`; `MESSAGE_TOO_LARGE` keep-alive vs disconnect. | The area D catalogue is canonical. Adopted everywhere: `protocol_server`, `command_dispatcher`, `headless_runner` (exit 0/1/2/3, `--keep-going`), `INVALID_ARGS`, `scene.query`/`entity.get`/`reflection.components`, `view.screenshot` (`NOT_AVAILABLE` on `null`), `module.list/load/unload/reload/discover/build`, `script.compile/reload/gc/attach`. Added to 04: error `MESSAGE_TOO_LARGE` (then disconnect), events `scene.loaded`, `progress`, `module.state`, `script.reloaded`. |
| R16 | N | 01, 07, 08 | Assert hook named `core::test::expect_assert` (01) vs `core::set_assert_handler` + `conjure::testing::expect_assert` (08); `conjure_add_test_module` had two signatures (08 vs 07's `LINK_ENGINE_SHARED`). | `core::set_assert_handler()` is the core primitive; `conjure::testing::expect_assert` is the test wrapper. `conjure_add_test_module(<module> SOURCES ... [DEPS] [ENGINE] [LINK_ENGINE_SHARED] [LABELS] [TIMEOUT])`. Test files mirror public headers one-to-one as a minimum (08 5.1); extra topic files are allowed. |
| R17 | C | 06, 07 | The umbrella shared library and the wave-4 `engine` module were both named `conjure_engine`. Area G also asked the lead architect to confirm that a shared aggregate complies with the brief's "each engine module is a static library". | Umbrella renamed `conjure_shared` (alias `conjure::shared`; macros `CONJURE_ENGINE_EXPORTS` / `CONJURE_ENGINE_SHARED` unchanged). Confirmed compliant: every module remains a `conjure_add_module()` static library; only executables and plugins link the aggregate. `ecs::component_type_cache<T>` is the one documented exception to "no template statics" (cache, not identity). |
| R18 | D | 06, 07 | Built-in libraries would register components/systems twice: through engine subsystem adapters (06) and through `CONJURE_DECLARE_STATIC_MODULE` entries (07). | Subsystem adapters register (startup order matters). Static modules for `conjure.ecs`, `conjure.renderer`, ... are **manifest-only** (null entry) and exist so game modules can declare dependencies and feature flags resolve uniformly. |
| R19 | C | 01, 05, 06, 07 | `conjure_module_entry` had four signatures: `(component_registry &, scheduler &)`, `(asset_type_registry &, importer_registry &, ...)`, `(engine_context &)`, `(module_context *)`. | `module_context *` (07; C ABI needs the modules type). Services provided by the engine: `engine::engine_context`, `engine::application` (for `add_subsystem`, startup step 15 only), `ecs::world`, `assets::asset_database`, `scene::scene_manager`. Consumers: `ecs`, `assets`, `scripting`, `editor`. |
| R20 | C | 06, 07 | `project.json` `modules` was an explicit `[{name, path, required}]` list (06) and a `{searchPaths, enginePlugins}` object (07). | Manifest-driven discovery: `"modules": { "search_paths": ["modules"], "engine_plugins": true, "required": ["..."] }` (snake_case keys per 06); `modules_settings` struct in `engine_config`. |
| R21 | G/D | 03, 04, 05, 06, 07, 08 | Export steps were scattered: dotnet publish (03), `project.export` command and `--export` (04), `asset_cooker` (05), "area E writes cooked=true" (06), `export_layout` (07), "export as a library call" (08). Nobody owned the sequence. | `engine::export_pipeline` (`conjure/engine/export.h`, 06 3.6) owns it: build game module (Distribution) -> `asset_cooker::cook` -> dotnet publish/build -> `module_manager::export_layout` -> copy `conjure_runtime` + `conjure_shared` + `shaders/` -> write `project.json` with `cooked: true`. The editor's `--export` and `project.export` call it. |
| R22 | C | 05, 06 | `engine_context::assets` was `asset_database *`, but the runtime never constructs a database (05 D16) while every consumer needs the `asset_manager`. Scripting pointer was `script_host *`; modules pointer `module_registry *`. | `engine_context` has `asset_manager *assets` (always), `asset_database *assetDatabase` (null when cooked), `component_registry *components`, `script_runtime *scripting`, `module_manager *modules`, `check_registry *checks`, `vfs::file_system *filesystem`. |
| R23 | G/C | 04, 05, 06 | Assets required `begin_frame/update/end_frame` and `hot_reload_controller::update` per frame; the engine frame sequence had no such step. `load_blocking` was refused inside a frame, but editor commands (`prefab.instantiate`, `scene.open`) run inside frames. | `assets_subsystem::begin_frame` runs `asset_manager::begin_frame + update` and `hot_reload_controller::update` (frame step 5); `end_frame` at step 13. `load_blocking` is permitted inside a frame on the main thread with a once-per-frame WARN in Development. |
| R24 | C | 05, 06, 07 | VFS mounts: `assets://`, `engine://` (05) vs `project://`, `assets://`, `cooked://` (06); module content "under `/modules/<name>`" (07). | Mount set: `project://`, `assets://`, `engine://`, `cooked://` (cooked projects mount `engine://` + `cooked://`), and `modules://<name>/` per loaded module. |
| R25 | G | 01, 05 | Assets wanted reflection to bind `asset_ref<T>` fields on read through a context carrying `asset_manager *`; the ECS serializer has no context and `ecs` must depend on `core` only. | `core::reflect` field kind `ASSET_REF` serialises the uuid string; `component_info::assetRefFieldOffsets` lists the fields; the scene module runs a post-load bind pass with the new untyped `asset_manager::bind_slot(id)`. |
| R26 | D | 03 (+ bindings gap) | `entity_*`, `component_*`, `transform_*` entries appeared in the core `.inl` listing with unclear implementers ("core entries" vs bindings). | Scripting implements `log_*`, `script_*`, `entity_*`, `component_*`, `query_*` (need only `ecs`); bindings (wave 4) implements `transform_*`, `physics_*`, `audio_*`, `prefab_*`, `scene_*`, `input_*`, `asset_*`, `diagnostics_*`. Amended by R64: the hierarchy half of `entity_*` and `modules_*` are bindings' too (ten groups, `15_bindings.md` 2.3). |
| R27 | C | 03, 06 | Engine needed `script_host::start/load_game_assembly/request_reload`, `scripting::register_systems(scheduler, script_host &)` and initialised scripting at step 14 before bindings filled the table at step 16; scripting requires the table to be complete before `initialize`. | `scripting_subsystem::startup` (14) constructs `script_runtime`; `bindings_subsystem` (16) fills the table; `scripting_subsystem::post_startup` (17) runs `initialize` then `load_game_assembly`. Systems: `scripting.frame_begin`, `scripting.collision_dispatch`, `scripting.fixed_update`, `scripting.update`, `scripting.late_update`, `scripting.frame_end` via `register_script_systems(ecs::scheduler &, script_runtime &)`. |
| R28 | C/N | 03, 08 | Tests wanted `scripting::host::is_runtime_available()`; "a throwing OnUpdate logs ERROR once" conflicted with 03's "log every fault, disable after three consecutive". | `scripting::is_runtime_available()` added (static hostfxr probe). 03 semantics win; the test uses `allow_errors()` and asserts the count. |
| R29 | C | 03 (+ scene gap) | Scripting required the scene serializer to call `serialize_entity_scripts` / `remap_entity_fields` and scene unload to call `destroy_all`, creating a scene -> scripting dependency between wave-3 siblings; `script_component` (a store handle) would alias when archived/copied. | `script_component` registers custom `ops.copy`/`ops.destruct` (deep copy of the script set; memcpy relocate) and a custom `component_serializer` through `script_runtime::active()`. Scene has no scripting dependency; `scene_manager::on_before_unload` lets the engine's `scripting_subsystem` run `destroy_all(true)`. |
| R30 | N/G | all | `core` has no design document; eight documents spelt its API differently (`jobs::scheduler` vs `job_system`; `post_to_main` vs `post_to_main_thread`; `core::threading::is_main_thread` vs `core::is_main_thread`; `fnv1a_64` vs `fnv1a64` vs `hash64`; `hash_map` vs `flat_hash_map`; `core::reflection` vs `core::reflect`; `error::X` global enumerators vs per-module enums; six log-category macro spellings; `input_state`/`input_injector`/`input::inject`; `rhi::create_device` vs `rhi::device::create`; `dynamic_library` in core and in modules). | Core contract fixed in `ARCHITECTURE.md` 4 and section 3 below: `core::jobs::job_system` (`start(n)`, 0 = inline, `schedule(fn, priority)`, `wait` executes on caller, `wait_idle`, `parallel_for(count, grain, fn(begin, end))`, `worker_count`, `thread_index`), `core::jobs::is_main_thread()`, `core::jobs::post_to_main_thread()`, `core::jobs::dedicated_thread`; `core::hash::fnv1a64`; `core::flat_hash_map` + `core::flat_map`; `core::reflect`; `core::error {category, code, message}` + per-module `enum class <module>_error` and `make_error`; `CONJURE_DECLARE_LOG_CATEGORY(name)` / `CONJURE_DEFINE_LOG_CATEGORY(name)`; `platform::input::{apply, inject, end_frame, snapshot}`; `rhi::device::create(device_desc)`; `core::dynamic_library` (used by modules and scripting). |
| R31 | C | 01, 03, 04, 05, 06, 07, CODE_STYLE | `CODE_STYLE.cpp`'s struct example uses snake_case fields, contradicting its own "camelCase variables" rule; 05 used `mX` on plain structs; several flag enums were plain `enum` without `NONE`, contrary to CONVENTIONS 3. | Plain aggregate structs: camelCase fields, no prefix. Classes: `mCamelCase`. The C ABI structs in `scripting/native_api.h` (mirrored by the C# generator) are the sole snake_case exception. Every flag set is `enum class <name>_flags : uintN_t { NONE = 0, ... }` + `CONJURE_ENUM_FLAGS(<name>_flags);` (fixed in 01, 02, 04, 07; 05 carries a note). |
| R32 | N/G | 01, 08 | Tests needed `component_flags::INTERNAL`, `component_registry::types()` and `ecs::world_hash`; ECS had `EDITOR_HIDDEN`, no `types()`, no hash. | `EDITOR_HIDDEN` is the "internal" flag; `types()` added; `conjure/ecs/world_hash.h` added with the ordering of 08 3.4. |
| R33 | C | 05, 06, 08 | Database `scan()` at startup step 8 precedes importer registration (steps 11, 13, 15); `null` window size 1280x720 (08) vs 1600x900 default (06). | `scan()` + `import_stale()` run in `assets_subsystem::post_startup` (step 17). `headless_defaults()` sets `window = 1280x720`. |
| R34 | C | 02, 05 | Renderer baked IBL on GPU compute *at import* and wrote results back into the cooked blob; asset importers run on worker threads without a device and must be deterministic for the content-hash cache. | IBL (cubemap, SH9, GGX prefilter) is baked on the CPU inside the environment importer, job-parallel per face, deterministic; the GPU BRDF LUT stays at renderer creation. GPU bake shaders are dropped from v1. |
| R35 | C | 04, 05 | `asset.import` was an async command over `asset_database::import_async`; the database's mutations are main-thread only and `import` is a blocking `parallel_for`. | `asset.import` is synchronous in v1 (`add_file` + `import`); an async importer is future work. |
| R36 | C | 03, 07 | Modules: one collectible ALC per module, script fields serialised into `reload_context`, runtime `native_binding_desc`. Scripting: a single game ALC (type identity of `ScriptBehaviour`), plugins extend the native table at build time via `.inl` fragments. | Single game ALC: module assemblies are appended to it; MANAGED_ONLY reload = `script_runtime::reload_game_assembly` (fields preserved by 03 3.8, `reload_context` unused). `native_binding_desc {entryName, function}` fills an *existing* null slot by name; new slots require an `.inl` line in the engine tree. |
| R37 | N | 05, 07 | 07 named the module records `assets::importer_desc` / `assets::asset_type_desc`; 05 already uses `asset_type_desc` for a non-trivially-copyable registry type. | Module records are `assets::importer_extension_desc` and `assets::asset_type_extension_desc` (factory function pointers); the registry types keep their names. |
| R38 | C | 04, 05, 06 | Engine's thread table listed a file-watcher thread with OS notifications and a TCP accept/read thread; assets polls from a job and the editor polls non-blocking sockets on main. | Thread table corrected: no watcher thread (job + main-thread poll), no TCP thread (main-thread poll); only `--ai-stdio` owns a `dedicated_thread`. |
| R39 | C | 03, 06 | `project.json` `scripting` section: `{assembly, mode, debugSymbols}` (03) vs `{assembly, hot_reload}` (06). | `"scripting": { "assembly", "hot_reload", "export_mode": "framework-dependent"\|"self-contained", "debug_symbols" }`; `scripting_settings` gains `exportMode`, `exportDebugSymbols`. |
| R40 | G | 01, 04 | Editor called `ecs::component_registry::to_json/from_json(world, entity, type)`, which ECS never offered (only per-component `component_serializer` hooks). | `ecs::component_to_json(world, entity, id, writer)` / `component_from_json(world, entity, id, value)` added to `serialization.h`. |
| R41 | C (risk) | 01, 08 | `world_hash` hashes entity ids; ECS states id *values* reserved inside PARALLEL grains are not stable across worker counts; the determinism test varies worker counts. | Documented constraint: determinism scenes spawn only from EXCLUSIVE / MAIN_THREAD systems or command buffers flushed in grain order. Remaining risk listed in section 4. |
| R42 | N | 06, 08 | Tests referred to `engine_config::useRenderThread`; the engine has no render thread and no such field. | Removed from 08; no render thread exists in any mode (06 D1). |
| R43 | C | 12, 05, ARCH 2.2/5 | `ARCHITECTURE.md` 2.2 drew `audio -> core` only and section 5 said "audio clips: audio_engine buffers in finalise"; 12 A12/A6 make `audio` depend publicly on `assets` (clip asset type, `asset_ref<audio_clip>` field) and give the clip a CPU-only loader whose `pcm_buffer` is shared with voices via `shared_ptr`. 05 listed `.wav .ogg .mp3` only. | 12 wins: graph shows `audio -> core, assets` (like `renderer`); section 5 and 05 8 state the CPU-only loader; clip extensions `.wav .ogg .mp3 .flac`, `storage: auto \| pcm \| streamed`. `ecs` stays out of audio (R7). |
| R44 | G | 14, 05, ARCH 5 | Physics needs collision geometry in Distribution but may not depend on `renderer` and `mesh_asset::cpu_geometry()` is Debug/Development only (R11); nobody registered a physics asset type. | `physics::register_asset_types` registers `conjure.physics.collision_mesh` (`.gltf .glb .obj`, `collision_mesh_importer`, positions + indices cooked deterministically; CPU-only loader builds the Jolt shape on the load job). Added to 05 8 ("Offers to physics"), `ARCHITECTURE.md` 5 and 06 3.4 step 12; the C# `CollisionMeshAsset` tag joins 15 2.5. |
| R45 | N | 13, 05 | 05 8 named the scene files `.scene` / `.prefab`; 13, 06 2.10 (`scenes/main.scene.json`) and `ARCHITECTURE.md` 4.4 use `.scene.json` / `.prefab.json`. | `.scene.json` / `.prefab.json` everywhere; 05 8 corrected. |
| R46 | C | 13, 06, ARCH 3/4.4 | `ARCHITECTURE.md` 4.4 and 06 3.1/8 listed three scene systems; 13 adds `scene.apply_hierarchy_changes` (TRANSFORM, EXCLUSIVE \| RUNS_WHILE_PAUSED, before propagation) to repair raw `parent` edits, and the TRANSIENT/EDITOR_HIDDEN components `depth_tag<1..31>`, `transform_state`, `inactive`, `local_bounds`, `prefab_link`. | Four systems are canonical (`13_scene.md` 4.7); `ARCHITECTURE.md` 3, 06 3.1 step 8 and 06 8 list them. The extra components are hidden, so `04_editor.md` D8 / `08_testing.md` D8 ("every non-hidden component type") are unaffected. |
| R47 | D | 13, 14, 06 3.2, ARCH 4.4 | Three descriptions of `interpolated_transform`: scene "defines it, written by physics" (13, 06), physics adds it through the deferred buffer in the `ADDED(rigid_body)` observer (14), nobody removed it and characters were not mentioned. | Scene defines and registers the component (TRANSIENT) and never adds it. Physics is the sole adder, writer and remover: the `ADDED` observer of a DYNAMIC/KINEMATIC `rigid_body` or of a `character_controller` enqueues `deferred.add<interpolated_transform>`; the first pull writes `previous = current = pose`; `destroy_body` on a living entity (and a body turning STATIC) enqueues `deferred.remove`. 13 2.1, 14 3.3, 06 3.2 and `ARCHITECTURE.md` 4.4 say the same. Scripts never write it (15 11). |
| R48 | C | 15, 14 | 15 2.6 proposed physics mirrors (`RigidBodyData` 56 bytes with `uint Layer`/`uint Flags`, colliders with `Center`, `CharacterControllerData` 24 bytes) that do not match 14 2.3 (`u8 layer`, `u16 flags`, `maxLinearVelocity`, `isSleeping`, `offset` + `rotation`, `convexRadius`, character ground fields); 14 claimed `rigid_body` is 64 bytes (it is 52). | The owning note defines the fields: 15 2.6 restates 14 2.3 field-for-field (`RigidBodyData` 52, `BoxColliderData` 44, `SphereColliderData` 20, `CapsuleColliderData` 40, `ConvexColliderData` 64, `MeshColliderData` 32, `CharacterControllerData` 72); `rigid_body` is 52 bytes in 14 2.3 and its test; a mirror and its native struct change in the same PR (hash test). |
| R49 | N/G | 15, 14 | Entry names differed (14 8: `physics_layer_find/name`, `physics_character_move/ground`; 15 2.3: `physics_layer_index/layer_name/layers_collide`, `physics_character_is_grounded/character_velocity`) and 15 needed `physics_move_kinematic`, `physics_get/set_gravity` and a `conjure_force_mode`, which `physics_world` lacked. | 15 owns entry names (R26). `physics_world` gains `move_kinematic(entity, transform)` (queued `MoveKinematic`), `set_gravity(vec3)` (queued `SetGravity`) and `gravity()`; `conjure_force_mode` maps FORCE/IMPULSE/VELOCITY_CHANGE/ACCELERATION onto `add_force`/`add_impulse` with the mirror's mass (14 8); `character_is_grounded` = `character_ground() == ON_GROUND`, `character_velocity` reads the component. |
| R50 | C | 15, 12 | Two script-to-voice designs: 12 kept a "play requested" bit in the hidden `source_handle::flags` and offered voice-id-keyed entries (`audio_play(entity, clipUuid, paramsBlob) -> u64`, `audio_stop(u64, fade)` ...); 15 B8 wrote `playbackRequest`/`isPlaying` fields and listed entity-keyed entries; 15's `AudioSourceData` (48 bytes) omitted `playOnStart`, `attenuation`, distances and `handle`. | 15's entity-keyed `audio_*` entries are canonical (15 owns the table). `audio_source` (12 2.6) gains `audio_playback_request playbackRequest` and `bool isPlaying` (hidden, never serialised) before `handle`; `source_handle::flags` keeps only "started once"; `engine.audio_sync` consumes requests, resets them to NONE and writes `isPlaying`; `engine.audio_finish_one_shots` clears `isPlaying`. `AudioSourceData` lists every `audio_source` field in order (88 bytes, `handle` opaque); `AudioListenerData` gains `Active`. `audio_play_one_shot` = `audio_engine::play(clipRef, play_params{retain = false})`; master gain is read from `group(MASTER_GROUP)`. |
| R51 | G | 15, 14, 06 2.5 | 15 and 14 asked for `frame_stats *` and `physics_world *` to be "added" to `engine_context`; 06 2.5 already has `physics` and `audio` pointers but no stats pointer. | `engine_context` gains `const frame_stats *stats` (read by `diagnostics_frame_stats`); `physics`/`audio` pointers unchanged (null when compiled out or disabled). 14 8 corrected. |
| R52 | G | 12, 06 2.3/2.10 | 12 needed `device`, `master_gain`, `doppler_factor`, `speed_of_sound`, `groups[] {name, parent, gain}` under `project.json` `"audio"`; `engine::audio_settings` had `enabled`, `backend`, `sampleRate`. | `engine::audio_settings` gains `device`, `masterGain`, `dopplerFactor`, `speedOfSound`, `std::vector<audio_group_setting> groups`; `project.json` sample updated; `audio_subsystem` maps it one-to-one onto `audio::audio_settings` and `audio_backend_choice::NULL_DEVICE` onto `audio_backend_kind::NULL_BACKEND`. |
| R53 | C | 14, 06 2.3/3.4 | 06 2.3's `engine::physics_settings {enabled, gravity, maxBodies}` cannot author the 20 fields of `physics::physics_settings`; 06 3.4 step 12 did not say who parses `extensions.physics_layers`. | The engine struct stays the small summary (`enabled` is engine-only); `physics_subsystem` calls `physics::physics_settings::from_json(project.physics, project.extensions["physics_layers"])` + `validate()` (-> `PHYSICS_INIT_FAILED`) so every 14 2.1 field is authorable in snake_case without the engine naming it; step 12 also `attach`es the world, registers asset types and forwards `RELOADED`. |
| R54 | G | 15, 03 | 15 B7/3.3 routes locked-scope structural requests into `script_runtime::deferred_commands()` and checks `current_phase()`; 03 4 had `mCurrentPhase` but neither accessor nor a named buffer (R5 only said "the script runtime's command_buffer"). 15 also puts its 106 lines into `native_api.inl` under a marker and adds generator outputs. | 03 4: `ecs::command_buffer mDeferredCommands`, `deferred_commands()`, `current_phase()`; `query_end` and the phase-end force-close flush that buffer. 03 2.1 comment names the `// --- bindings (R26) ---` section, `tools/gen_native_api.py --status/--out-manifest` and `docs/generated/native_api_manifest.json`. |
| R55 | G/N | 15, 13 | 15 needed scene APIs 13 lacked (`deferred_set_parent`, `duplicate_subtree`, `create_many` without a template, `is_load_pending`, `active_path`) and named others wrongly (`scene::instantiate_prefab`, "parent observers rebuild children at flush"); 15's `PrefabInstance {Source, Root}` (32 B), `Parent {Value}` (8 B) and `Layer {uint}` mirrors contradict 13's `prefab_instance {prefab}` (24 B), `parent {value, siblingIndex}` (16 B), `layer {u8}`. | 13 4.3 gains `deferred_set_parent(command_buffer &, child, parent, keepWorld, index)` (records raw parent ops + `keepWorld` local + `transform_dirty`; repaired silently by `scene.apply_hierarchy_changes` via an expected-repairs counter, 13 3.7) and `duplicate_subtree(root, parent, index)`; `create_entities` accepts `NULL_ENTITY` as template; `scene_manager::is_load_pending(asset_id)`; paths cross through `asset_manager::find` / `loaded_scene_info::path`. 15 uses `prefab::instantiate(_many)` and 13's component layouts (`PrefabInstance` 24, `Parent` 16, `Layer` 1 byte). |
| R56 | N | 11, 15 | Windows macros: `key::DELETE` is impossible (11 P14 uses `DEL`). 15's ABI enums drifted from the platform values: `conjure_cursor_mode {NORMAL, HIDDEN, LOCKED}` vs `cursor_mode {NORMAL, HIDDEN, DISABLED, CAPTURED}`; `conjure_button_action {RELEASE, PRESS, TAP}` vs `button_action {RELEASE, PRESS, REPEAT}` while claiming "platform::button_action values". | `key::DEL` (C# `Key.Delete` unchanged); every enumerator avoids Windows macro names (`DELETE`, `ERROR`, `IGNORE`, `OPTIONAL`, `NEAR`/`FAR`, `MIN`/`MAX`). `conjure_cursor_mode` and `conjure_button_action` carry exactly the platform enumerators; "tap" is C#-side sugar (PRESS now, RELEASE next frame). |
| R57 | C/G | 15, 11 | 15 treated `input_clipboard_*` and `input_set_cursor_mode` as `NOT_SUPPORTED` stubs on the null window and excluded the clipboard entries from the `feature_test` counter check; 11 3.5 gives the null backend an in-memory clipboard and round-tripping cursor mode. 15 also needed gamepad names, which 11 did not expose. | 11 wins: those entries are real on both backends; the exclusions file keeps only `asset_path_of` and `diagnostics_call_count`. 11 4.2 gains `window::gamepad_name(u32) -> std::string_view` (`glfwGetGamepadName`; null: `"Null Gamepad <i>"` when connected). |
| R58 | G (requirement on core) | 15, 14, 10 | 15 B5 requires `math::quat` to be stored x,y,z,w so `conjure_quat_xyzw`, `TransformData` and `LocalTransform` are memcpy-able and so the layout matches `JPH::Quat`; `10_core.md` 2.2 only shows the `(w, x, y, z)` constructor and does not fix storage (glm's default is build-dependent). 14 P5 described `math::quat` as "(w, x, y, z)" without distinguishing construction from storage. | Decision (10_core.md 2.2 updated): glm 1.0.1 already stores quaternions x,y,z,w, so core defines nothing (`GLM_FORCE_QUAT_DATA_XYZW` would change the constructor to (x, y, z, w); `GLM_FORCE_QUAT_DATA_WXYZ` is forbidden); `core/math` static_asserts `offsetof(quat, x) == 0 && offsetof(quat, w) == 12` and `tests/core/math/test_math.cpp` repeats it; `type_info::layout_hash` test vectors are published for C#. 14 P5 and 15 B5 distinguish construction (w, x, y, z) from storage (x, y, z, w). |
| R59 | G | 11, 14, ARCH 9 | New CMake options `CONJURE_PLATFORM_GLFW` (11 4.5) and `CONJURE_PHYSICS_CROSS_PLATFORM_DETERMINISTIC` (14 P16) were absent from `ARCHITECTURE.md` 9. | Both listed in `ARCHITECTURE.md` 9 ("Module options"): GLFW ON by default (OFF = null-only platform library), cross-platform determinism OFF by default and never set by CI presets. |
| R60 | N | 11, 06 7 | 06 7 said the null window's `pump_events()` "returns nothing" and `should_close()` is false "until `request_quit`" (an engine call that never touches the window). | 06 7: returns an empty span unless a test queued events (`push_event`, `simulate_*`); `should_close()` false until `request_close()` / `simulate_close()`; clipboard and cursor mode round-trip in memory. |
| R61 | N | 15, 03 | 03 spelt the C# facade `RigidBody` (15: `Rigidbody`); 03 3.9 defined the mirror layout hash as "(offset, size) of fields plus total size" while core implements `type_info::layout_hash()` over `(name, kind, offset, size)`; 03 used `[NativeComponent("conjure::scene::local_transform")]` where the registered name is `conjure.scene.local_transform`. | `Rigidbody`; one hash definition = `type_info::layout_hash()` (10 2.11, 15 B10), C# field names camel-cased, `Pad*` fields excluded on both sides; `[NativeComponent]` takes the registered component name. |
| R62 | N | 08, 12, 14, 15 | 08 5.2 audio row wanted "wav/ogg decode from `tests/assets`" (12 generates WAVs in-test and fetches ogg/mp3/flac into `tests/.downloaded`); 08 5.2 physics row said contacts are "emitted to ecs events" (R6: no ECS event queue); 08 7.2 placed the exclusions file at `tests/downloaded/binding_exclusions.json` though it is authored. | 08 5.2/7.2 corrected: in-test WAV + `precondition::DOWNLOADED_ASSETS` for the compressed formats; `physics_world::contact_events()` span; `tests/binding_exclusions.json`. |
| R63 | N | ARCH 4.1, 10, 12, 13, 14 | `ARCHITECTURE.md` 4.1 required each module to declare its own `make_error(<module>_error, detail)` overload; `10_core.md` C3 (implemented) binds each enum with `CONJURE_DECLARE_ERROR_CATEGORY` and uses the generic `core::make_error`, which 11-15 follow and 14 states explicitly. | 10 wins: no per-module `make_error`; `ARCHITECTURE.md` 4.1 corrected. |
| R64 | N | 15, 03, R26, ARCH 4.6 | R26 and 03 2.1 gave `entity_*` wholly to scripting and listed eight bindings prefixes; 15 adds a `modules_*` group and twenty hierarchy/tag/layer/guid `entity_*` entries that need `scene::scene`. | R26 amended: scripting implements `log_*`, `script_*`, `component_*`, `query_*` and the five core `entity_*` entries (`create`, `destroy`, `is_alive`, `get_name`, `find_by_name`); bindings implements the ten groups `transform, entity (rest), scene, prefab, physics, audio, input, asset, diagnostics, modules` under the `// --- bindings (R26) ---` marker. |
| R65 | G (closure) | ARCH 4, 09 3 | `ARCHITECTURE.md` 4 carried inline contracts for six undocumented modules and 09 3 required design notes before implementation; the notes `10_core.md` ... `15_bindings.md` now exist and are more detailed than section 4. | Section 4 keeps the consolidated core summary (4.1, still the cross-module reference) and turns 4.2-4.6 into pointer paragraphs; the notes are binding, this file wins over a note, a note wins over `ARCHITECTURE.md` detail. Section 3 below is updated accordingly. |

---

## 2. Duplicated responsibilities, final owners

| Responsibility | Claimed by | Owner after review |
|---|---|---|
| Transform propagation | ECS (01), engine (06) | `scene` (`scene.propagate_transforms`), R3 |
| Render extraction (`EXTRACT`), renderer/audio component registration | renderer (02 implied), engine (06) | `engine` subsystem adapters, R7 |
| ImGui context | renderer (04), engine `imgui_layer` (06), modules (07) | `engine::imgui_layer`; renderer draws; one copy in `conjure_shared`, R9 |
| GPU upload of assets | renderer `upload_*` (02), asset loaders' `finalise` (05) | engine `renderer_subsystem` driving `renderer::upload_*` from asset events, R10 |
| Picking | renderer `id_pass` (02), editor CPU picker (04) | `editor` CPU picker, R11 |
| Screenshots | renderer `request_capture` (04 assumption), `read_back` (02) | `editor::screenshot_service` over `renderer::read_back`, R12 |
| Built-in component/system registration | engine subsystems (06), static modules (07) | engine subsystems; static modules are manifest-only, R18 |
| Module entry signature | 01, 05, 06, 07 | `modules::module_context`, R19 |
| Export | 03, 04, 05, 06, 07, 08 | `engine::export_pipeline`, R21 |
| `dynamic_library` RAII wrapper | core (03), modules (07) | `core::dynamic_library`, R30 |
| Scheduling of systems | ECS scheduler (01), engine `register_systems` (06) | ECS `scheduler` executes; each `ecs`-dependent module registers its own systems; engine orders the registration calls and `finalise()`s, R2/R7 |
| Script state inside scene files | scene serializer calling scripting (03) | `script_component`'s own `component_serializer`, R29 |
| Editor reaching scene/ecs | `editor_context` references | `editor_context` holds `scene::scene_manager &` (active `scene::scene` -> `ecs::world`), `ecs::component_registry &` via `engine_context`, and uses `ecs::component_to_json/from_json` + `entity_archive` (R40); never raw chunk layout |
| Adding / writing / removing `interpolated_transform` | scene (13, 06 3.2 "scene defines"), physics (14 observer) | `scene` defines and registers; `physics` adds, writes and removes, R47 |
| Component field lists mirrored in C# | bindings proposal (15 2.6), owning notes (12, 13, 14) | the owning note (`12_audio.md` 2.6, `13_scene.md` 2.1, `14_physics.md` 2.3); 15 restates, same-PR rule, R48/R50/R55 |
| Native entry names and groups | 12 8 / 14 8 "offers to bindings" lists, 15 2.3 | `15_bindings.md` 2.3 (ten groups); 12/13/14 offer the C++ API behind them, R49/R50/R64 |
| Script -> voice playback requests | `source_handle::flags` bit (12), `playbackRequest` field (15 B8) | `audio_source::playbackRequest` / `isPlaying`, consumed by `engine.audio_sync`, R50 |
| Collision geometry asset | renderer `mesh_asset::cpu_geometry()` (Debug only, R11), physics own type (14 P14) | `physics::register_asset_types` -> `conjure.physics.collision_mesh`, R44 |
| Repairing hierarchy after raw / deferred `parent` edits | ECS observers at flush (15 assumption), scene repair system (13 3.7) | `scene.apply_hierarchy_changes` (TRANSFORM, EXCLUSIVE), silent for `deferred_set_parent`, R46/R55 |

---

## 3. Design notes for the formerly undocumented modules (R65)

Six modules were used by every area document but had no judged design. Each now has one, written from
`ARCHITECTURE.md` 4 and reviewed against this file (R43-R65): `10_core.md` (wave 1; being implemented),
`11_platform.md`, `12_audio.md` (wave 2), `13_scene.md`, `14_physics.md` (wave 3), `15_bindings.md` (wave 4).
Authority order: this file, then the note, then `ARCHITECTURE.md` (whose section 4 is now a summary plus
pointers). Still without a note of their own, by design: `runtime` (`runtime/main.cpp` exactly as
`06_engine_loop.md` 4.2 describes; headless boot of an exported folder is `application` with
`--project <dir> --headless --frames N`) and `samples` (`08_testing.md` 7.2 is their specification).

Cross-note decisions that an implementer must not re-decide:

- `interpolated_transform`: scene defines, physics adds/writes/removes (R47).
- Component field lists are owned by 12/13/14; the C# mirrors in 15 2.6 restate them and change in the same PR (R48, R50, R55).
- Native entry names and the ten bindings groups are owned by 15 2.3 (R49, R50, R64); 12/13/14 offer the C++ API the entries call.
- `audio -> assets` is a public dependency; `audio`, `renderer` never include `ecs` (R7, R43).
- `math::quat` storage is x,y,z,w (R58); `type_info::layout_hash()` is the one mirror-hash definition (R61).
- Scene systems are four (R46); scene files are `.scene.json` / `.prefab.json` (R45).

The consolidated contracts below are kept as the short form the notes were derived from; where they differ
from a note in detail, the note is right.

- **core names follow `10_core.md` and the headers** (wave 1 integration): where an area document spelled a core
  name differently, the document was corrected to the implemented API (`log::add_sink`/`log::remove_sink` with
  `std::shared_ptr<log_sink>`, `uuid::is_null`, `error_code::IO_FAILURE`/`PARSE_FAILURE`,
  `CONJURE_DEFINE_LOG_CATEGORY(name, "display")`, `conjure::testing`, `job_system::start(job_system_desc)`,
  `memory::counters()`/`snapshot()`, `vfs::enumerate(dir, enumerate_flags, callback)`). Decisions taken at the same
  time: `str::split` returns `std::vector<std::string_view>` (C2 keeps `strings` off `containers`);
  `flat_hash_map::erase` is backward-shift and invalidates pointers to later elements of the same probe run (rehash
  is not the only invalidation); the time interface keeps its spec name `clock` (write `core::clock` where a
  `using namespace conjure::core;` would make it ambiguous with `::clock()`); `math::align_up`/`is_power_of_two`
  mirror `memory::` rather than depend on it; `slot_key` stays `u32`/`u32` (renderer ids that want a 16-bit
  generation use a `slot_key_type` key, `02_renderer.md` 2.2). CMake keeps failing hard on a listed-but-missing
  source: parallel implementers add the `CMakeLists.txt` entry after the file exists instead.
- **Wave 2 integration (ecs, platform, rhi, assets, audio, modules on core)**: spellings corrected to the implemented
  API, names only. `11_platform.md`: `native_handle_kind::WINDOWS` (not `WIN32`, a macro on every Windows build; `WIN32`
  joins the macro list in `CONVENTIONS.md` 3 and `tools/check_style.py`), `input::skip_next_mouse_delta()` (called by
  the window owner after `set_cursor_mode()`), the `inject(const EventType &)` template overload, `release_all()` also
  clears the modifiers. `02_renderer.md`: `device_desc::window` and `swapchain::create` take `rhi::native_window`
  (`platform::native_window_handle` copied field for field, kind `WINDOWS`; the rhi never includes platform), the
  DXIL depfile is a second `dxc -MF` run, the null device has no `Feature::TimerQueries` (absent from the pinned
  NVRHI). `07_modules.md` 3.8 / 5: `cmake/ConjureSharedLibrary.cmake` (not `ConjureAbiIdentity.cmake`), global property
  `CONJURE_STATIC_LIBRARIES`, macros `CONJURE_SHARED_EXPORTS` / `CONJURE_SHARED_IMPORTS` (R17's `CONJURE_ENGINE_*`
  names lost to `conjure/core/api.h`, which shipped first), the header hash is SHA-256 based, `conjure_add_game_module`
  gained `OUTPUT_ROOT` and `DEFINES`, `test_module_context.cpp` replaces `test_module_context_services.cpp`.
  `conjure_finalize_engine()` is called from the root `CMakeLists.txt`; `LINK_ENGINE_SHARED` links `conjure::shared`
  plus `conjure::test_support_shared` and leaves `tracked_global_new.cpp` out (its `_aligned_free` cannot release
  memory the DLL's C-runtime `operator new` produced); `/wd4251 /wd4275` moved into `conjure::warnings`;
  `CONJURE_BUILD_EDITOR=1` is a `conjure::platform_defines` define outside Distribution. Decisions taken at the same
  time: `core::reflect` caps the alignment it demands of a custom handle (`ENTITY` / `ASSET_REF`) at the owning
  type's alignment (an 8-byte `ecs::entity` is `{u32, u32}` and lives in 4-byte aligned components, so `parent
  {entity value; u32 siblingIndex}` is describable without `alignas(8)`); an `ASSET_REF` field wider than 16 bytes
  serialises its leading uuid and reads back with the rest zeroed (`assets::asset_ref<T>` = uuid + slot pointer, the
  layout contract of R25), and `assets/asset_ref.h` specialises `reflect_kind_override<asset_ref<T>>` under the one
  name `conjure.assets.asset_ref`; `audio::audio_source` (88 bytes), `audio::play_params_from(source, group)` and
  `audio_engine::apply_source` exist as `12_audio.md` 2.6 / 4.4 describe; `precondition::AUDIO_DEVICE` (probe installed
  by the audio tests) lets the `[audio_device]` cases skip with exit 77. Declined or deferred: hidden visibility and
  `CONJURE_SHARED_EXPORTS` on the static libraries wait for `CONJURE_API` on core's public surface (183 declarations,
  `tools/check_exports.py core`); `engine/shaders` is added from `engine/CMakeLists.txt` instead of the rhi module;
  CC0 `tone.{ogg,mp3,flac}` entries for `tools/test_assets.json`, `tests/benchmarks/bench_*.cpp` and the fetched glslang
  link failure under `CONJURE_RHI_VULKAN=ON` on MSVC remain open (section 4, item 13).
- **Wave 3 integration (scene, renderer, physics, scripting on waves 1-2)**: spellings corrected to the implemented
  API, names only. `02_renderer.md`: `alpha_mode::SOLID` (glTF `OPAQUE` is a Windows macro), `ray_from_viewport` takes the
  viewport aspect ratio, the opaque draw key is `[variant:8][material:22][mesh:22][submesh:4][depth:8]` and the blended
  key carries a 24-bit depth; the shadow comparison samplers use `Less` under the reverse-Z linearisation. `03_scripting.md`
  3: on MSVC the scripting module links the `nethost.lib` import library (the static `libnethost.lib` is `/MT`) and copies
  `nethost.dll` beside the executables. `13_scene.md`: `parent` is 12 bytes (`entity` is 4-byte aligned; the C# mirror
  follows `type_info::layout_hash()`), `instantiate_report::entities` lists every created entity, `scene` gained
  `set_guid`, `resolve_guid`, `set_root_owner`, `set_root_order`, `root_owner`, `tag_of`, `layer_of`,
  `pending_destroy_count`, `pending_repair_count`, an archive entry without components loads as a plain root.
  `14_physics.md`: `character_controller` is 68 bytes (R48 said 72), `push_transforms(dt, command_buffer *deferred)`,
  `physics_settings::penetrationSlop` (0.005 m) is an extra field, queries are refused during the push and the pull as
  well as the step. Cross-module changes made for the wave: `ecs::component_type_desc::serializer` (copied by
  `register_described`; `describe_component<T>` leaves it null for reflected types so the registry's reflection walk over its deep copy serves them after a module unload) so `script_component` serialises its
  script set through every archive path without a scene -> scripting dependency (R29); `world::instantiate_batch(archive,
  outRoots, commands, outEntities)` reports every created id, which replaced the scene's ADDED-observer collector;
  the ECS scheduler keeps its per-stage scratch in the phase state so a whole phase is allocation-free after its first
  run (`13_scene.md` 10; `job_system::schedule` still allocates its counter when `parallelSystems` is on);
  `world::get_raw(entity, id) const` documents the locked-world read from grain jobs (`01_ecs.md` 5.2);
  `assets::importer_registry::find_for_path` (longest registered suffix) lets the database pick the `.scene.json` /
  `.prefab.json` importers (R45); the Jolt Development defines and the `CONJURE_PHYSICS_CROSS_PLATFORM_DETERMINISTIC`
  option live in `third_party/CMakeLists.txt` and the root `CMakeLists.txt` (R59); `tests/rhi/test_shader_library.cpp`
  accepts the renderer's permutations in the shader index. `tests/integration/` (`conjure_integration_tests`) holds
  `test_assets_renderer_headless.cpp`, `test_scripting_scene.cpp` and `test_assets_scene.cpp` (the wave 3 exit of
  `ARCHITECTURE.md` 11). Owed to wave 4 (engine): `renderer_subsystem` (asset events -> `upload_*` / `release_*`,
  `register_asset_types`, `set_imgui_draw_data` / `imgui()`, `on_swapchain_resized` / `on_device_lost`, `model_asset ->
  prefab` over `prefab_builder`), `scene_subsystem` (`register_components` at startup step 10, `register_asset_types`
  before `scan()`, `register_systems` at step 18, `scene_manager::load` at step 20, `unload_all` at shutdown step 2,
  `time.interpolatePhysics` -> `transform_system::set_interpolation_enabled`, `layer_table` from `project.json`).
  Declined or deferred: see section 4, item 14.
- **core** (R30, R16, R31): `result<T>`/`error` model, `jobs::job_system` API, `reflect` namespace with
  `type_id`, `type_info`, `field_info` (kinds incl. `ENTITY` by type id and `ASSET_REF`), `set_assert_handler`,
  `memory::counters()`/`snapshot()`, `hash::fnv1a64`, `dynamic_library`, `process`, `clock`/`manual_clock`,
  `image`, `vfs::file_system` + `memory_file_system`, `event<Args...>` + `event_subscription`, `delegate`,
  `event_bus`, containers (`slot_map`, `small_vector`, `stable_vector`, `flat_hash_map`, `flat_map`,
  `bit_vector`, `string_id`, `radix_sort_u64`), `platform_detect.h`, `api.h` (`CONJURE_API`), generated
  `build_info.h`.
- **platform**: `window` (`pump_events`, `should_close`, `framebuffer_size`, `is_minimised`, `is_focused`,
  `native_handle`, `required_vulkan_instance_extensions`, `create_vulkan_surface`), `input` (`apply`,
  `inject`, `end_frame`, `snapshot() -> input_state` with 512-key bitsets, mouse, 4 gamepads x 6 axes),
  `input_event`, `button_action`, backends `glfw` and `null` (1280x720 under `headless_defaults()`), clipboard.
- **scene** (R3, R25, R29): components `parent`, `children` (`small_vector`), `local_transform`,
  `world_transform`, `render_world_transform`, `interpolated_transform`, `transform_dirty` (tag), `name`,
  `tag`, `layer`, `guid`, `enabled`; `scene` (world + hierarchy, `name_of/set_name`, `structure_version()`,
  `guid_index`, `create_entities(count, template)`, reparent with `keepWorld`, world AABB per entity);
  `scene_serializer` (`save/load_scene`, `save_subtree`, `instantiate_subtree`, `capture -> scene_snapshot`,
  `restore`, asset_ref bind pass); prefabs (`conjure.scene.prefab` asset: create/instantiate/apply/unpack,
  nested); `scene_manager(world &, asset_manager &)` (`load` sync, `request_load/unload`, additive,
  `on_before_unload`, `collect()` after unload); systems `scene.apply_pending_loads` (FRAME_BEGIN),
  `scene.apply_hierarchy_changes` (TRANSFORM, EXCLUSIVE, R46), `scene.propagate_transforms` (TRANSFORM),
  `scene.destroy_pending` (FRAME_END); `model_asset -> prefab`.
- **physics** (R6): Jolt behind `physics_world(settings, jobs &)` with a `JPH::JobSystem` adapter and
  `JPH::Allocate*` hooks into `core::memory`; components `rigid_body`, `box/sphere/capsule/convex/mesh_collider`,
  `physics_material`, `character_controller`; layers from `project.json` `extensions.physics_layers`; queries
  `raycast`, `overlap_sphere`, shape casts; systems `physics.push_transforms`, `physics.step` (MAIN_THREAD),
  `physics.pull_transforms`, `physics.contact_events`; `contact_events()` span sorted by body-id pair;
  sleeping query; component sync iterates engine arrays.
- **audio**: `audio_engine(settings)` over miniaudio/`null`, `conjure.audio.clip` asset type + importer
  (CPU-only loader; `audio -> core, assets`, R43), components `audio_source` (incl. `playbackRequest` /
  `isPlaying`, R50), `audio_listener`, mix groups, 3D spatialisation, `null_backend_stats`; no `ecs`
  dependency (the engine owns `engine.audio_sync`, `engine.audio_finish_one_shots`).
- **bindings** (R26): `scripting_bindings_subsystem` filling the non-core native entries, `[NativeComponent]`
  mirrors, call counters (`bindings::call_counters()`), `Conjure.Diagnostics.Check`, `AssetRef<T>` marshalling
  (`asset_ref_get(uuid, typeId)`), read-only `Conjure.Modules.Registry`.
- **runtime**: `runtime/main.cpp` exactly as `06_engine_loop.md` 4.2 describes; headless boot of an exported
  folder is `application` with `--project <dir> --headless --frames N`.

---

## 4. Remaining risks (not resolvable by editing documents)

1. **Determinism versus parallel spawning (R41).** `world_hash` includes entity ids; ids reserved inside
   PARALLEL grains depend on worker count. The documented constraint keeps the determinism test honest, but a
   future gameplay system spawning from a PARALLEL system will silently break thread-count invariance. A
   per-grain reservation protocol is listed as ECS future work.
2. **Collision callback latency (R6).** Scripts receive contacts one fixed step after they occur. Acceptable
   for v1 and matches common engines; documented so nobody "fixes" it by moving dispatch into FIXED_END, where
   managed code would run outside the EXCLUSIVE script systems.
3. **Upload payload copies (R10).** Copying payload bytes at `upload_*` time doubles peak memory for large
   textures during load. Measured, not designed around; streaming is renderer future work.
4. **CPU IBL bake cost (R34).** 128^2 x 6 mips x 128 samples on the CPU is seconds per HDRI at import. It
   is cached and job-parallel; an optional GPU fast path remains future work.
5. **`load_blocking` inside a frame (R23).** Correct but a hitch; the editor should prefer `load_batch` +
   async commands once asset residency UX is designed.
6. **Single game ALC for plugins (R36).** A leaky plugin assembly prevents unloading the whole game
   assembly; this is already the documented .NET limitation for the game assembly itself.
7. **Design notes versus the core implementation in flight (R58, R63, section 3).** `10_core.md` is being
   implemented while 11-15 were written against it. Two requirements this review places on core are not yet
   visible in `10_core.md` itself: no glm define (glm 1.0.1 already stores quaternions x,y,z,w; `GLM_FORCE_QUAT_DATA_XYZW` would only change the constructor argument order and `GLM_FORCE_QUAT_DATA_WXYZ` must never be set) plus a `static_assert` on `offsetof` in `core/math` and its test, and
   published `layout_hash` test vectors for C#. If core ships glm with w-first storage, every `TransformData` /
   `LocalTransform` memcpy in 15 and the Jolt conversions of 14 silently rotate everything; `test_marshal::
   quat_storage_is_xyzw` and `tests/core/test_math.cpp` are the gates and must land with core.
8. **`CODE_STYLE.cpp` struct example (R31).** The style file's own example contradicts its rule; the
   resolution is recorded in `CONVENTIONS.md`'s spirit, but the customer-supplied file was not modified.
   Reviewers must apply R31, not the example.
9. **Mirror/struct coupling across three modules (R48, R50, R55).** `audio_source` (88 bytes incl. an opaque
   `source_handle`), `rigid_body` (52), `character_controller` (72) and `parent` (16) are now mirrored byte-for-byte
   in C#. Any field added to those structs by the engine's `audio_subsystem` or a physics PR breaks
   `component_lookup_type` for every game assembly until the mirror and `Pad*` fields are regenerated; the
   layout-hash test catches it in CI, but the coupling is deliberate and permanent.
10. **Deferred reparenting inside a query scope (R55).** `scene::deferred_set_parent` leaves the tree
    inconsistent (children/depth tags) between the flush at `query_end` and `scene.apply_hierarchy_changes`
    in the same frame's TRANSFORM phase. Systems running in UPDATE/LATE_UPDATE after a script reparent and
    reading `children` or `depth_tag` see the old shape for the rest of that phase. Documented; a flush-time
    repair would need the scene to own the ECS command buffer, which R5 gives to the script runtime.
11. **Physics cross-platform determinism (R59).** `CONJURE_PHYSICS_CROSS_PLATFORM_DETERMINISTIC` is OFF and
    untested in CI; `tests/integration/test_determinism.cpp` stays physics-free (14 P16) until it is proven.
    Golden gameplay recordings that include physics are therefore per-platform.
12. **Audio request latency and ordering (R50).** `playbackRequest` is consumed in SYNC; two scripts writing
    the same source in one frame leave the last writer's request. `isPlaying` is one frame old for scripts.
    Both match the "one native call per phase" model of 03 and are acceptable for v1.
13. **Wave 2 loose ends (integration).** (a) `conjure_shared` exports every symbol of the 17 wave-1/2 static
    libraries (26,739 of the 65,535 a Windows DLL may hold); the switch to `CONJURE_API`-only exports with hidden
    visibility (`07_modules.md` 5) must land before the wave-3/4 libraries push the count past the limit, and it needs
    `CONJURE_API` on core's public classes and functions first. (b) `glslang` 15.1.0 fetched with `ENABLE_OPT=OFF` /
    `BUILD_EXTERNAL=OFF` fails to link `glslang-standalone` on MSVC 2026 (unresolved SPIRV symbols), so no SPIR-V blob
    builds on Windows with `CONJURE_RHI_VULKAN=ON`; the Windows SDK `dxc` has no SPIR-V code generation either. Vulkan
    is OFF by default on Windows and the Linux / macOS presets are unaffected; a glslang tag that links (or a fetched
    DXC release) is wanted before Vulkan is exercised on Windows. (c) `tests/benchmarks` does not exist yet; the
    benchmarks `01`/`05`/`11`/`12` name (`bench_ecs`, `bench_asset_ref_get`, `bench_asset_load`, `bench_pack_lookup`,
    `bench_platform`, `bench_audio`, `bench_module_reload`) are owed by whoever creates it. (d) `tools/test_assets.json`
    still lacks CC0 `audio/tone.{ogg,mp3,flac}` clips, so the `[downloaded]` decoder case skips. (e) The platform
    module's `conjure_input_block` layout pins and `key::DEL` are facts the C# generator (wave 4) must honour.
14. **Wave 3 loose ends (integration).** (a) `conjure_shared` now exports 46,318 symbols in Debug and 47,320 in
    Distribution of the 65,535 limit (21 static libraries); the `CONJURE_API`-only export switch (item 13a) is the
    next thing wave 4 must land before `engine`, `bindings` and the editor libraries join the umbrella. (b) Dear ImGui
    stays at 1.91.9b-docking: the `IMGUI_HAS_TEXTURES` path of `imgui_renderer` is written against >= 1.92 but a bump
    was not attempted inside the integration (the GLFW backend and the font-atlas path are what the tests exercise);
    `ARCHITECTURE.md` 9 records the pinned version. (c) `core::jobs` has no allocation-free fire-and-forget
    `schedule` with a caller-owned counter; the physics Jolt adapter keeps its `parallel_for` helper-range trick and
    the ECS scheduler's `parallelSystems` path still allocates one counter per stage. (d) `tests/benchmarks`
    (`bench_scene`, `bench_physics`, `bench_render_null_frame`, `10k_scripts_update_phase`) is still owed with the
    directory itself (item 13c). (e) `scene::transform_state` has no "physics wrote this" flag; the pull writes
    `local_transform` directly and the pass re-dirties those rows once. (f) `physics.physics_world - 600 empty steps
    allocate nothing after the first` failed once in each of the Debug and Development CTest runs while the other
    configuration was compiling on the same machine and passes alone and in an idle run; the suspect is the job
    system's block pool growing under contention, to be measured before it is called flaky. (g) The `.gitignore`
    already covers `build/`, `**/bin/` and `**/obj/`, so no entry was added for the managed outputs.

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
| R8 | C | 02, 06 | Two renderer APIs: `render(const render_scene &)` + engine-owned `render_scene_pair` + `on_swapchain_resized()` + device-loss recovery (02) versus `renderer(device &, assets &)`, `begin/end_extract`, `render_frame`, `on_resize`, `on_device_lost`, parallel command-list recording, quit on device loss (06). | Renderer owns the `render_scene_pair`; `begin_extract() -> render_scene &`, `end_extract()`, `render_frame()`; `render(scene)` stays for tests; `renderer::create(renderer_desc)`; `resize_swapchain(size)` (code sweep F044: it replaced `on_swapchain_resized()`, which ran only after the swapchain had already refused to resize under the renderer's framebuffers); `on_device_lost()` then the engine quits with `exit_code::DEVICE_LOST` (recovery is v2); one command list recorded on main in v1, job-parallel preparation only. |
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
| R52 | G | 12, 06 2.3/2.10 | 12 needed `device`, `master_gain`, `doppler_factor`, `speed_of_sound`, `groups[] {name, parent, gain}` under `project.json` `"audio"`; `engine::audio_config` had `enabled`, `backend`, `sampleRate`. | `engine::audio_config` gains `deviceName` (the key `device`), `masterGain`, `dopplerFactor`, `speedOfSound`, `std::vector<audio_group_setting> groups`; `project.json` sample updated; `audio_subsystem` maps it one-to-one onto `audio::audio_settings` and `audio_backend_choice::NULL_DEVICE` onto `audio_backend_kind::NULL_BACKEND`. |
| R53 | C | 14, 06 2.3/3.4 | 06 2.3's `engine::physics_config {enabled, gravity, maxBodies}` cannot author the 20 fields of `physics::physics_settings`; 06 3.4 step 12 did not say who parses `extensions.physics_layers`. | The engine struct stays the small summary (`enabled` is engine-only); `physics_subsystem` calls `physics::physics_settings::from_json(project.physics, project.extensions["physics_layers"])` + `validate()` (-> `PHYSICS_INIT_FAILED`) so every 14 2.1 field is authorable in snake_case without the engine naming it; step 12 also `attach`es the world, registers asset types and forwards `RELOADED`. |
| R54 | G | 15, 03 | 15 B7/3.3 routes locked-scope structural requests into `script_runtime::deferred_commands()` and named a `current_phase()` check for the entries; 03 4 had `mCurrentPhase` but neither accessor nor a named buffer (R5 only said "the script runtime's command_buffer"). 15 also puts its 106 lines into `native_api.inl` under a marker and adds generator outputs. | 03 4: `ecs::command_buffer mDeferredCommands`, `deferred_commands()`, `current_phase()`; `query_end` and the phase-end force-close flush that buffer. Scripting's `query_begin` refuses with `NOT_IN_PHASE` outside a phase; the bindings entries check the thread only (`WRONG_THREAD`) and never return `NOT_IN_PHASE`, which stays reserved in `bindings_error.inl` so the status codes keep their values (15 3.2, sweep F166: the `OnCreate`/`OnDestroy` run by the flushes of `frame_begin`, `frame_end` and `set_mode` have no phase and must reach every entry). 03 2.1 comment names the `// --- bindings (R26) ---` section, `tools/gen_native_api.py --status/--out-manifest` and `docs/generated/native_api_manifest.json`. |
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
| R66 | G | 04, 10, 11 | Code sweep F740 (04 D23) has the editor write its AI-protocol token to a file that only the user running the editor may read. That takes operating system calls (a mode at creation on POSIX, an access control list on Windows), and no area offered them: 11 is the window, the event stream and the input snapshot, main thread only, with a null backend that makes no operating system call at all; 10 had the native file calls but none about permissions. | The file system layer owns it: `core::vfs::write_private_file(path, data)` and `core::vfs::is_private_file(path)` in `conjure/core/vfs/native_file_system.h`, implemented once per platform beside the other native file calls (`engine/src/core/vfs/native_platform_windows.cpp`, `native_platform_posix.cpp`, chosen by CMake, no `#ifdef` elsewhere). 11 is unchanged: a platform backend is a window backend. The editor is the first caller (`protocol_server`, 04 3.12, for the two token files of 04 D23 and D25); the audit log decided with F187 is the second (`audit_log` creates `ai_audit.log` through the same call, 04 D26, and falls back to an ordinary file on `UNSUPPORTED`); the project trust store decided with F187 is the third (`project_trust`, 04 D27), and the first caller that also reads by the rule: it believes its store only while `is_private_file` holds, and remembers nothing on `UNSUPPORTED`. A volume that cannot restrict a file to its owner answers `UNSUPPORTED` and the caller decides what that means (the editor writes no token file and says so). The folder that store lives in, the per-user state folder of the machine, is not a file call and not part of this resolution: the editor resolves it in a file pair of its own (`engine/src/editor/core/user_paths_windows.cpp`, `user_paths_posix.cpp`, chosen by CMake) until 11 offers the per-user folders 04 3.15 asks of it, at which point the editor calls that instead. |
| R67 | G | 02, 04, 06, ARCH 8 | Code sweep F200: no document said under which licence Conjure is published, who keeps the attribution of third-party software or how an exported game carries it. The README said "All rights reserved" without a holder, 04 5 promises the licence text of the editor's fonts "in the third-party notices", which did not exist, the tonemap shader of 02 R11 follows two published fits without their licence, and export step 5 of 06 3.6 shipped binaries of MIT, zlib and Apache software without a notice. | The owner's decision, binding detail in 06 D15. Conjure is Apache-2.0 (`LICENSE`), held by "the Conjure contributors" (`NOTICE`). `third_party/CMakeLists.txt` owns the list: one `conjure_third_party_notice()` beside each pin with its licence files and a scope (runtime, editor, build), checked against the pin at configure time; whoever adds a dependency, or tracks a file under someone else's licence (the fonts of 04, licence text under `third_party/licenses/`), adds its notice there. `cmake/ConjureNotices.cmake` and `tools/third_party_notices.py` own the two products: the tracked `THIRD_PARTY_NOTICES.md` for all platforms (CTest `notices.up_to_date`) and the `THIRD_PARTY_NOTICES.txt` every build writes next to `conjure_runtime` with Conjure's own notice and what that build links. The engine's export step 5 owns shipping: it copies that file with the runtime and warns when the distribution directory has none. Shader code that follows a published implementation names source and licence in its comment and has a notice. |

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
  `register_asset_types`, `set_imgui_draw_data` / `imgui()`, `resize_swapchain` / `on_device_lost`, `model_asset ->
  prefab` over `prefab_builder`), `scene_subsystem` (`register_components` at startup step 10, `register_asset_types`
  before `scan()`, `register_systems` at step 18, `scene_manager::load` at step 20, `unload_all` at shutdown step 2,
  `time.interpolatePhysics` -> `transform_system::set_interpolation_enabled`, `layer_table` from `project.json`).
  Declined or deferred: see section 4, item 14.
- **Wave 4 integration (engine, bindings, editor_core / editor_ui, runtime, test support engine on waves 1-3)**:
  spellings corrected to the implemented API, names only. `06_engine_loop.md`: `jobs_settings::workerCount` defaults
  to `AUTO_WORKER_COUNT` (`0xFFFFFFFF` = `hardware_concurrency - 1` clamped to `[1, 16]`) and `0` is the inline job
  mode of `08_testing.md` D4 (`--jobs 0`, `--jobs auto`), which `tests/integration/test_determinism.cpp` runs at 0, 2
  and 8 workers; `application::post_to_main_thread` takes `std::function` (no `std::move_only_function` on the
  supported toolchains); `application::inject_render_failure(error, frames)` and `rhi::device::simulate_device_loss()`
  drive the failure rules of 06 6 headless; `rendering_settings::shaderDirectory`, `scripting_settings::managedDirectory`
  and `scripting_settings::useNullHost` exist for tests and tools; `engine_error` gained `NONE`, `EXPORT_FAILED` and
  `SCRIPT_RUNTIME_INIT_FAILED`; `modules` is a public dependency of `engine` (`engine_context` and `application` carry
  `CONJURE_MODULE_SERVICE`). `04_editor.md`: `command_flags::NO_UNDO` (a `MUTATES` command without an inverse, used for
  module commands), the inverse-only commands `entity.restore`, `asset.restore` and `script.delete`, `view.mode`
  (read) next to `view.set_mode`, `guid` / `guids` arguments on `entity.create` / `entity.create_many`, qualified
  property paths (`<component alias or registered name>.<field>.<subfield>`, aliases from `editor.schema`); a line that
  is not valid UTF-8 is a `PARSE_ERROR` that keeps the client connected (unless no line of the client parsed before
  it: a client whose first line is not a request gets the answer and is disconnected, 04 D24). `15_bindings.md` 2.6:
  `Parent` is 12 bytes and `CharacterControllerData` 68 (4-aligned entity handle). `07_modules.md` 5 and
  `ARCHITECTURE.md` 9: the `CONJURE_API`-only export switch is in place (items 13a and 14a below are closed):
  every static library compiles with `CONJURE_SHARED_EXPORTS` and hidden visibility, no `.def` is generated,
  `conjure_shared` exports 5,283 symbols in Debug with every wave-4 library included, `tools/check_exports.py` guards
  the annotations and the count, and the editor libraries are declared from `engine/CMakeLists.txt` so
  `conjure_editor` links `conjure::shared` like `conjure_runtime`. Cross-module changes made for the wave:
  `conjure::testing::headless_engine_options` gained `registerSystems` (a test's systems at startup step 18) and
  `gameAssembly`; the fixture warms up the process with the full descriptor (project and test systems included) before
  taking its memory baseline; `tools/check_style.py --tests-mirror-headers` pairs
  `tests/support/engine/include/conjure/testing/<name>.h` with `tests/support/tests/engine/test_<name>.cpp`;
  `conjure_benchmarks` carries its own doctest implementation unit (`tests/benchmarks/src/doctest_implementation.cpp`)
  because `conjure::test_support` references doctest from every translation unit; the C# template `Player.cs` copies
  `Transform` to a local before writing a property (`Transform` is a readonly struct returned by value, CS1612
  otherwise). `tests/integration/` now holds every scenario of `08_testing.md` 6.1 plus the engine scenarios of
  `06_engine_loop.md` 10; `test_module_plugin.cpp` lives in the `LINK_ENGINE_SHARED` executable
  `conjure_integration_plugin_tests` with the fixture plugin `tests/integration/fixtures/sample_plugin`, so the static
  executable keeps meaningful `memory_guard` and `allocation_scope` gates. Declined or deferred: see section 4, item 15.
- **Performance round (2026-11; integration on waves 1-4)**: the allocation and time budgets of `08_testing.md`
  7.1 applied to the implemented engine. Each shortfall of item 15(j) was closed by its module owner without
  touching `tests/benchmarks/thresholds.json`, then the tree was integrated as one: clean Debug, Development and
  Distribution builds in `build/perf_integration{,_dev,_dist}`; 4,064 Debug tests and 4,062 of 4,063 Development
  tests green after the review of the integration added its three regression cases (the one red entry is the
  enforced benchmark row below); the three contention-sensitive gates
  (`physics.physics_world - 600 empty steps`, `integration.hierarchy_propagation` 10,000-entity budget,
  `integration.prefab_spawn_destroy` budget) repeated with `ctest --repeat until-fail:20` in both trees without a
  failure; `check_style.py --tests-mirror-headers`, `check_exports.py`, `check_docs.py` and `docs.py build`
  clean. Closed shortfalls (allocations per measured iteration, before -> after, Development, 16 workers, from
  `build/perf_final_dev_run.json`): `ecs.iterate_100k` 1 -> 0 (query matches inline,
  `QUERY_INLINE_MATCHES`), `ecs.spawn_destroy_10k` 33 -> 0 against a budget of 2 (the world's member flush
  plan, recycled command-buffer arenas, parked spare chunks), `renderer.extract_10k` 21 -> 0 and
  `renderer.null_frame_10k` 306 -> 0 (persistent view states, `cull_scratch`, pooled graph entries, binding-desc
  scratch, pre-created timer queries, an allocation-free null device), `scripting.update_10k_dod` 3 -> 0 (a cached
  `ecs::query` per component set, reused scope states), `physics.sync_10k` 2 -> 0 and 0.43 -> 0.07 ms (the pull
  through `parallel_for_chunks` on a persistent query, one `TryGetBody` per row, a dense entity-index row table),
  `engine.full_frame_10k_transform_render` 12 -> 0, `engine.full_frame_10k_physics` 42 -> 0 and
  `engine.full_frame_10k_scripts` 1 -> 0 (stage jobs on a stack-held `core::jobs::job_fence`, the asset
  hot-reload watcher opt-in through `assets_settings::hotReload` and off headless, the extraction's member camera
  list, main-thread queues drained only when non-empty). Integration changes on top, each the smallest one:
  `core/jobs` keeps an idle worker and a thread in `wait()` spinning for a bounded 50 us (`pause`, a `yield()`
  every 64th) before it sleeps, guards the queue with a spinlock instead of a `std::mutex` (a ticket lock when
  measured; the review of the integration replaced it with a test-and-test-and-set lock that parks after a
  bounded spin, because a FIFO ticket stalls every later waiter behind a preempted one on a pool sized to the
  machine, and had `finish_job` wake a job parked in `wait_idle()` when the last sibling of its own batch
  finishes, a pre-existing lost wake-up the review found), separates the
  hot queue atomics onto their own cache lines and counts its statistics per thread (a queued job cost 1.7 us
  of serialised hand-off with 16 workers; `core.job_parallel_for_1m` 0.100 -> 0.042 ms,
  `scene.transform_propagation_10k` 0.225 -> 0.172, `_sparse` 0.150 -> 0.125, `ecs.iterate_100k` 0.113 -> 0.055,
  `engine.full_frame_10k_transform_render` 0.571 -> 0.492, all with the ticket lock; the final parking lock measures
  0.052, 0.173, 0.136, 0.049 and 0.470, `build/perf_final_dev_run.json`); `core/vfs` `native_file_system::write_file_atomic`
  passes its `<target>.tmp-<uuid>` temporary through `extend_native_path` on its own (the temporary is 41
  characters longer than the target, so a target just under the classic Windows limit had a temporary beyond it:
  the integration tests' meta writes failed in a build directory four characters longer than the owners',
  `tests/core/vfs` regression `perf-round`); `tests/core/jobs` "stop called from a worker" polls the handle
  after the body's `done` flag instead of checking it at once (the counter drops after the body returns; the
  check raced the worker once); `tests/editor` `headless_editor::send` bounds its wait for an asynchronous reply by
  wall time alone (twice the dispatcher's timeout: the application clock is wall time, the forced 1/60 s delta
  shapes the frame delta only) and no longer by a 100,000-frame cap, which a Development build's microsecond
  headless frame exhausted before `dotnet` could answer `script.compile` (two editor cases failed in
  Development after the round). Threshold left unmet, unchanged for the design owner to decide:
  `scene.transform_propagation_10k_sparse` p50 0.136 ms against 0.100 (p99 0.411, allocations 0; 0.125 with the
  ticket lock). The
  arithmetic, owner's and integrator's: S4's chunk-granular stamp makes "10 % of the roots" 6,770 of 10,000 rows
  (68 %), 25 ns each, so the work fits any worker count from two up (inline 0.122 ms, 2 workers 0.094, 4 workers
  0.070, 8 workers 0.092, 16 workers 0.125 with the ticket lock and 0.136 with the parking lock); the 16-worker
  figure is the dispatch of about 41 jobs per iteration through the single shared queue at about 0.6 to 0.8 us
  per pop (down from 1.7 us), plus two joins, and neither
  one range per thread (load imbalance: dense 0.199, sparse 0.097 at 4 workers) nor dropping the pass's per-chunk
  counters changes it. Closing it needs per-worker queues with stealing or a `parallel_for` that queues
  min(ranges, workers) claim blocks, both of which the core tests pin against (`test_parallel_for.cpp`:
  `poolCapacity >= range_count - 1`, the per-range queue-full fallback) and `10_core.md` describes; or the
  scenario at `--threads 4`; or 7.1's "10 % dirty" read as 10 % of the rows. The patterns the round settled on
  (persistent frame state, caller-owned job counters, cached queries, opt-in background work) are
  `docs/handbook/data-oriented-design.md` section 8; `tests/benchmarks/README.md` has the full table.
- **Demo lineup revision (2026-11-12; wave 6 design, `17_demo_games.md`)**: at the owner's request the 2D demos
  showcase the data-oriented design: Barrage (bullet hell, all gameplay in C# over `ComponentQuery` spans) and Colony
  (ant colony, native plugin module plus a C# director, the flagship) replace the two earlier small 2D demos; Grid
  Sweeper, Rolling Course and Corridor Strike are unchanged. 17 gains G7 (entity targets and enforced budgets), a G3
  that admits a project-owned native module built through the public module API while still forbidding engine changes
  for a demo, and a G5 whose recorded hash is a per-configuration data file. The two designs were checked against the
  headers; the engine gaps they found, each a follow-up owed before or during wave 6. **Prerequisites** (engine
  changes with their own tests, 17 4.1): (P1, done) `ecs::world_hash` hashed all 24 bytes of an `ASSET_REF` field,
  slot pointer included (`world_hash.cpp` `hash_fields`), so no world with a bound sprite hashed equal across
  processes; it now hashes the leading 16 uuid bytes only; (P2)
  `entry_component_add` rejects an id that `Entity.Create` reserved inside a query scope, so C# cannot spawn with
  components through the deferred buffer; accept `add_raw` for ids reserved by the runtime's deferred buffer, before
  Barrage; (P3) no benchmark row contains a sprite; add `renderer.sprites_10k` (extraction, `sprite_batcher::build`,
  upload), which confirms both demos' frame gates; (P4) the renderer components' `CONJURE_COMPONENT` name bindings are
  in the private `engine/src/engine/subsystems/renderer_components.h`; move them to a public header, before Colony.
  The review of P1 added two: (P5) every guid minted at run time (`scene::create_entity`, `ensure_guid`, instantiate
  with `REGENERATE_ALL`, additive-load conflicts, `prefab_builder::build`) is a per-process random uuid that
  `world_hash` covers, so the scene owes a guid source that is seedable in headless mode before a hashed run creates
  scene entities at run time; (P6) `audio_source` hashes its voice handle and `isPlaying`, so a field attribute for
  runtime link state that `world_hash` skips is owed before a hashed run contains an audio source.
  **Follow-ups with a way around in the demo** (17 2.3.11, 2.4.12): `world_hash` depends on component registration
  order and hashes TRANSIENT components (`transform_state::lastUpdateRun`); no world resource or singleton storage and
  no user data on `system_extension_desc::run` (Colony's grids are module-static memory behind a hashed stand-in
  component); `parallel_for_chunks` passes no grain index; `world::FLUSH_INLINE_PAYLOADS = 4` (a fifth payload on one
  entity in one flush allocates); stages are packed by component mask rather than archetype and
  `scheduler_desc::minEntitiesPerJob` cannot be set per system or project; sprite extraction takes a mutex and a hash
  lookup per sprite (`gpu_resource_cache::find_texture`); no shipped C# `Sprite` or `Camera` mirror; no C# window size
  or screen-to-world conversion (Grid Sweeper's picking needs it too); `ComponentQuery` has no `without` / `changed`
  filter and C# has no multi-component or batch spawn; run-time scene entities get random guids that are hashed (no
  seedable generator, no guid setter in the bindings); `docs/api/bindings/csharp-api.md` describes `Entity.Create` as
  making a scene entity with a transform and a guid while the entry creates a bare entity; `Entity.Create` allocates
  for a non-empty name; the frame statistics carry no managed allocation figure; a shared-engine executable cannot
  count allocations and `conjure::bench_harness` exports no `main`; no floating-point contraction policy in the CMake
  files. **Unverified, settled by the first stage of each demo**: `[NativeComponent]` mirrors of plugin-registered and
  of game-declared engine components resolve; the real plugin and the real CLR share one shared-engine test process;
  scene unload, snapshot, picker and serializer over raw entities that carry only a sprite and a matrix. **Plumbing
  owed by wave 6 outside `engine/`**: `samples/CMakeLists.txt` adds `games/<demo>` ahead of its early return;
  `docs/api/tutorials/`; `samples/tools/use_packs.py` and the `samples/games_packed/` ignore entry (17 G4); the
  nightly `benchmarks-ci-class` job builds and runs `barrage_benchmarks` and `colony_benchmarks`. Four Colony choices
  await the owner's confirmation (17 2.4.13), first among them a per-row intent column in place of the preferred
  per-worker deposit buffers.
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
13. **Wave 2 loose ends (integration).** (a) *Resolved in wave 4*: `conjure_shared` exported every symbol of the 17
    wave-1/2 static libraries (26,739 of the 65,535 a Windows DLL may hold); the switch to `CONJURE_API`-only exports
    with hidden visibility (`07_modules.md` 5) landed with `CONJURE_API` on every public declaration of waves 1-4
    (4,213 Debug / 2,938 Distribution exports before the wave-4 libraries, 5,283 Debug with them). (b) `glslang` 15.1.0 fetched with `ENABLE_OPT=OFF` /
    `BUILD_EXTERNAL=OFF` fails to link `glslang-standalone` on MSVC 2026 (unresolved SPIRV symbols), so no SPIR-V blob
    builds on Windows with `CONJURE_RHI_VULKAN=ON`; the Windows SDK `dxc` has no SPIR-V code generation either. Vulkan
    is OFF by default on Windows and the Linux / macOS presets are unaffected; a glslang tag that links (or a fetched
    DXC release) is wanted before Vulkan is exercised on Windows. (c) `tests/benchmarks` exists since wave 4
    (`bench_core`, `bench_ecs`, `bench_scene`, `bench_physics`, `bench_render_null_frame`,
    `bench_scripts_update_phase`, `bench_full_frame_10k`; `tests/benchmarks/README.md`). Still owed, with their
    design budgets: `bench_asset_ref_get` (10,000 `get()` < 20 us, 05:211/917), `bench_asset_load` (1,000 blobs
    < 250 ms main thread, 05:918), `bench_pack_lookup` (05:918), `bench_platform` (10,000 events + `end_frame`
    < 50 us, snapshot < 0.2 us, 11:677), `bench_audio` (update + `end_frame` with 256 voices < 0.1 ms, 12 A16/:792),
    `bench_module_reload` (07:930), `bench_culling_10k` / `bench_draw_list_10k` (< 2 ms) /
    `bench_render_graph_compile` (02:928-929), `bench_bindings` (`1k_raycasts` < 1 ms, `counter_thunk_overhead`
    < 2 ns/call, `10k_transform_set_via_facade` < 1.5 ms, `10k_transform_set_via_span` < 0.1 ms; 15:779, also
    03:921-922), `bench_editor` (`create_many` 10,000 entities with 4 components < 20 ms, 04:331;
    `persist_capture_apply_100`, `create_many_10k`, `snapshot_capture_restore_10k`, `hierarchy_rebuild_10k`,
    `pick_broad_phase_10k`, `dispatch_10k_component_set`, 04:1114-1115) and the scripting
    `reload_10k_instances` (< 250 ms, 03:922). (d) `tools/test_assets.json` still lacks CC0
    `audio/tone.{ogg,mp3,flac}`; the `[downloaded]` decoder case skips only where nothing was downloaded and fails
    its `REQUIRE(exists)` wherever `tools/download_test_assets.py` has run (every `build-test` leg of `ci.yml` and
    every leg of the nightly matrix; code sweep 2026-11-12). (e) The platform module's `conjure_input_block`
    layout pins and `key::DEL` are facts the C# generator (wave 4) must honour.
14. **Wave 3 loose ends (integration).** (a) *Resolved in wave 4*: `conjure_shared` exported 46,318 symbols in Debug
    and 47,320 in Distribution of the 65,535 limit (21 static libraries); the `CONJURE_API`-only export switch (item
    13a) landed before `engine`, `bindings` and the editor libraries joined the umbrella (5,283 Debug exports with all
    of them). (b) Dear ImGui
    stays at 1.91.9b-docking: the `IMGUI_HAS_TEXTURES` path of `imgui_renderer` is written against >= 1.92 but a bump
    was not attempted inside the integration (the GLFW backend and the font-atlas path are what the tests exercise);
    `ARCHITECTURE.md` 9 records the pinned version. (c) `core::jobs` has no allocation-free fire-and-forget
    `schedule` with a caller-owned counter; the physics Jolt adapter keeps its `parallel_for` helper-range trick and
    the ECS scheduler's `parallelSystems` path still allocates one counter per stage. (d) *Resolved in wave 4*:
    `tests/benchmarks` holds `bench_scene`, `bench_physics`, `bench_render_null_frame` and `scripting.update_10k`
    in `bench_scripts_update_phase` (item 13c lists what is still owed). (e) `scene::transform_state` has no "physics
    wrote this" flag; the pull writes `local_transform` directly and the pass re-dirties those rows once. (f)
    `physics.physics_world - 600 empty steps allocate nothing after the first` failed once in each of the Debug and
    Development CTest runs while the other configuration was compiling on the same machine and passes alone and in an
    idle run; the suspect is the job system's block pool growing under contention, to be measured before it is called
    flaky. (g) The `.gitignore` already covers `build/`, `**/bin/` and `**/obj/`, so no entry was added for the managed
    outputs.
15. **Wave 4 loose ends (integration).** (a) *Resolved in the performance round (2026-11)*: an engine frame is
    allocation-free in Development. The wave-4 integration measured an empty headless frame at 13 blocks with
    inline jobs and 11 with two workers (review 4: 10), the same over a 10,000-entity hierarchy, and 526-542 per
    frame for spawning and destroying 100 crate prefab instances. The sites, named with a stack-capturing hook on
    `tracked_allocate`, were the scheduler's per-stage `make_shared<job_counter>` (now a stack-held
    `core::jobs::job_fence`, item 14c), the per-run `context.view<...>()` query (now `small_vector<archetype *,
    QUERY_INLINE_MATCHES>`), the asset hot-reload watcher created unconditionally outside Distribution whose
    500 ms stat pass allocated on a job thread (now opt-in through `engine_config::assets.hotReload` and the
    project's `"assets": {"hot_reload"}`, off in `headless_defaults()`, copied by the `--headless` merge of
    `application_desc::from_options` (review 4: the merge had every other headless default but this one, so the
    runtime's and the editor's command-line headless modes kept the watcher) and kept off by `project::apply_to`
    under headless), the extraction's per-frame camera candidate vector and `std::stable_sort` buffer (a member
    list and `std::sort` with a world-order tie-break), and, under MSVC's debug iterators only, the container
    proxies of the main-thread queue drain's and `layer_stack::apply_pending`'s local vectors (both skipped
    when nothing is queued). Measured after the round (Development, `headless_engine`): the empty frame 0
    blocks inline and with two workers, a frame with the renderer, physics and audio on 0, and the three
    `engine.full_frame_10k_*` benchmarks 0 (item j); the integration's enforced run
    (`build/perf_integration_dev_run.json`) and its 20-fold repeats of the two integration budgets and the
    physics "600 empty steps" gate in both trees confirm it (section 3, "Performance round"). A Debug test executable still sees one block per frame, the
    debug-iterator proxy of the local `std::vector` in `assets::detail::completion_queue::drain`
    (`asset_manager::update`), so `test_hierarchy_propagation` pins `EMPTY_FRAME_ALLOCATION_BUDGET = 1` (13
    before); the prefab churn costs 20 blocks per frame in Development and 258 in Debug, none in the frame
    loop (101 the local graveyard vector of `core::event::finish_emit` behind `scene::bump_structure`, 100 the
    temporary `command_buffer` that `std::swap` builds in `ecs::world::drain_deferred` once per
    `scene::destroy`, 57 the archive shape, link pass and report vectors of `prefab::instantiate_many`), so
    `test_prefab_spawn_destroy` pins `STEADY_FRAME_ALLOCATION_BUDGET = 258` (542 before). Both budgets may only
    go down. They and the renderer_subsystem gate (a frame with a camera, a mesh and a light costs no more than
    the empty frame, both measured in the same executable over five baseline frames) were measured on MSVC
    alone; libstdc++ and libc++ have no debug-iterator proxies, so the Linux and macOS CI legs are their first
    measurement (item h) and the relative gates, not the MSVC figures, are the primary assertions there. The
    zero of `08_testing.md` 6.1 for the Debug executables is owed by assets (a member drain
    vector), core (`finish_emit` without a local vector when nothing died), ecs (swap the deferred buffer
    pointers) and scene (`instantiate_many` scratch kept across calls). (b) Jolt settles ten stacked unit boxes
    1.3 cm low at the top and lets them slide up to 4 cm sideways before sleeping (penetrationSlop 0.005 m per
    contact); `test_physics_stack` gates 2 cm and 5 cm instead of the design's 1 cm. (c) `samples/feature_test` does
    not exist yet (wave 5); `test_engine_project_boot` boots `tests/assets/projects/minimal` and `test_export_roundtrip`
    exports it; the CI `windows-msvc-dev` export smoke runs the same fixture until the sample lands. (d) Off-screen
    rendering is not available: `engine_config::validate` refuses a real backend when headless, `renderer_subsystem`
    counts but drops camera views without a swapchain and `golden_image::render_offscreen` fails with `UNSUPPORTED`;
    the editor's `--offscreen` and `view.screenshot` fall back to the null device. A `preferSoftwareAdapter` switch,
    a headless view target in `renderer_subsystem` and the validation change are one engine/renderer task. (e)
    Requests recorded, not applied: `core::system::executable_directory()` replacing the engine's private
    `executable_path.{h,cpp}`; `service_id` / `CONJURE_MODULE_SERVICE` moved into core so `ecs::world`,
    `assets::asset_database` and `scene::scene_manager` can be module services; `script_runtime::add_assembly(path)` for
    the assemblies `scripting_module_consumer` only records; a public `importer_registry` accessor on
    `asset_database` and `hot_reload_controller` exposure for `script.reload`; a pre-load consumer hook on
    `modules_subsystem` so `editor_module_consumer` need not unload and reload; a public `scene::root_index_of` and an
    id-independent `ecs::world_hash` variant; a command-buffer prefab adoption path with expected-repair accounting
    for locked-scope `prefab_*` / `create_many` entries; `script_component` with a reflection description, and the
    `ScriptComponentHandle` mirror itself, which was never written; `renderer::debug_draw` exposed through
    `engine_context`; a one-time WARN in `renderer::render()` for swapchain views without a swapchain; a
    registry-independent `ecs::entity_archive::~entity_archive`; `conjure::testing::protocol_client::connect_in_process`
    / `connect_tcp` over `editor::loopback_transport` and the TCP socket (the design-11 ping cases);
    `bench_engine_frame.cpp` (the frame-overhead scenarios; `bench_full_frame_10k.cpp` covers the 10,000-entity frames).
    (f) The first successful `core::vfs::native_file_system` read in a process allocates about 1,070 blocks that live
    until exit (stream and locale machinery); `headless_engine` warms it up before its baseline. (g) `IMGUI_API`
    export and `conjure::thirdparty::imgui_interface` stay in `cmake/ConjureSharedLibrary.cmake`
    (`conjure_export_imgui_from_shared`), which `07_modules.md` 5 names next to the ImGui target; behaviour is
    identical and the move was not made. (h) The GCC/Clang builds with `CXX_VISIBILITY_PRESET hidden` are not
    verifiable from the Windows integration machine; the first Linux and macOS CI runs prove them. (i) Core
    changes made by the integration, each the smallest one: `native_file_system` prefixes `\\?\` to paths beyond
    the classic Windows limit (`extend_native_path`); the Distribution `CONJURE_VERIFY` discards its condition's
    value explicitly (MSVC C4834 when the condition is a `[[nodiscard]]` call, found by the Distribution build of
    `tests/modules/fixtures/module_surface`). (j) The first Development benchmark run (`conjure_benchmarks`,
    desktop class, 16 threads, 200 iterations, 2025-11 integration machine) passed 8 of 17 scenarios against
    `thresholds.json`: timings inside their budgets except `physics.sync_10k` (p50 0.43 ms, budget 0.30; the serial
    fallback, as fix round 4 found) and `scene.transform_propagation_10k_sparse` (p50 0.25 ms, budget 0.10), the
    other breaches allocation counts the design sets to zero (`ecs.iterate_100k` 1, `ecs.spawn_destroy_10k` 33
    against 2, `engine.full_frame_10k_physics` 42, `engine.full_frame_10k_scripts` 1,
    `engine.full_frame_10k_transform_render` 12, `renderer.null_frame_10k` 306, `scripting.update_10k_dod` 3). Fix
    round 4 (2026-11) corrected the scenario set (`core.job_parallel_for_1m`, `renderer.extract_10k` and
    `scripting.native_call_100k` were missing; `scene.snapshot_capture_restore_10k`, `renderer.null_frame_10k` and
    `physics.step_2k_settling` stood in their place) and measured `physics.sync_10k` through a scheduler with
    `physics::register_systems`. *The performance round (2026-11) closed every allocation breach*, each by its
    module owner without touching `thresholds.json`: ecs (query matches inline, the world's member flush plan and
    placement scratch, recycled command-buffer arenas, parked spare chunks; `ecs.iterate_100k` 1 -> 0,
    `ecs.spawn_destroy_10k` 33 -> 0 at 0.24 ms), core/ecs (stage jobs on a stack-held `core::jobs::job_fence`,
    a warmed-up phase allocates nothing in both scheduler modes), renderer and rhi (persistent view states,
    `cull_scratch`, pooled render-graph entries, binding-desc scratch, pre-created timer queries, an
    allocation-free null device with marker names recorded only after `reset_null_stats()`;
    `renderer.extract_10k` 21 -> 0, `renderer.null_frame_10k` 306 -> 0), scripting (a cached `ecs::query` per
    component set and reusable scope states; `scripting.update_10k_dod` 3 -> 0), physics (the pull through
    `parallel_for_chunks` on a persistent query, one `TryGetBody` per row, a dense entity-index row table;
    `physics.sync_10k` 2 -> 0 at 0.11 ms, its serial fallback 0.44 -> 0.27 ms) and engine (item a;
    `engine.full_frame_10k_transform_render` 12 -> 0 at 0.57 ms, `engine.full_frame_10k_physics` 42 -> 0 at
    4.47 ms, `engine.full_frame_10k_scripts` 1 -> 0 at 0.12 ms). One enforced Development run of all 20
    scenarios (`build/perf_engine_dev_run.json`, idle machine) passed 19; the integration's run after its
    job-system change (`build/perf_integration_dev_run.json`: a 50 us spin window before a thread sleeps and a
    spinlock on the queue, the ticket form of section 3, `core.job_parallel_for_1m` 0.100 -> 0.042 ms) and the final run with the
    parking lock that replaced it (`build/perf_final_dev_run.json`, 0.052 ms) pass
    the same 19 and the
    nightly `benchmarks-ci-class` job stays red on the one remaining row, the time budget of
    `scene.transform_propagation_10k_sparse` (p50 0.136 ms against 0.100 with the parking lock, 0.125 with the
    ticket lock; 0.150 before the job-system change,
    0.17 before the scene owner's pass rewrite; dense 0.172 of 0.400). The arithmetic, recorded with the
    threshold unchanged: the scenario moves 10 % of the roots, but the 100 roots share one 67-row chunk, so the
    chunk-granular `local_transform` stamp (13_scene.md S4) recomputes 6,770 of the 10,000 rows (68 %); at
    about 25 ns per row the work fits every worker count (inline 0.122 ms, 2 workers 0.094, 4 workers 0.070,
    8 workers 0.092, 16 workers 0.125 with the ticket lock, 0.136 with the parking lock), and the 16-worker
    figure is the dispatch of about 41 jobs per iteration through the single shared queue at about 0.6 to 0.8 us
    of serialised hand-off per pop (1.7 us before the
    integration's change, which is what the owner measured as 80-100 us of wake-up and join) plus two joins.
    Closing it needs per-worker queues or a claim-based `parallel_for` in core (both against what
    `tests/core/jobs/test_parallel_for.cpp` and `10_core.md` pin), the scenario at `--threads 4`, or a reading
    of 7.1's "10 % dirty" as 10 % of the rows rather than of the roots (section 3, "Performance round", has the
    experiments that were rejected). Allocation gates are sensitive
    to CPU contention (a frame that is allocation-free idle reported 4 blocks while another build saturated the
    machine, item 14f), so the enforced run belongs on an idle machine. `tests/benchmarks/README.md` records the
    full table and every remaining deviation.

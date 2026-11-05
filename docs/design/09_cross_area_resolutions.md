# Cross-area resolutions (consistency review of 01-08)

Status: **binding**. Author: consistency critic / architecture owner. Every item below was found by
reading `01_ecs.md` ... `08_testing.md` against each other and against `00_brief.md`; each one has been
fixed **in place** in the affected documents, which cite the resolution id (`R<n>`) at the edited spot.
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
| R26 | D | 03 (+ bindings gap) | `entity_*`, `component_*`, `transform_*` entries appeared in the core `.inl` listing with unclear implementers ("core entries" vs bindings). | Scripting implements `log_*`, `script_*`, `entity_*`, `component_*`, `query_*` (need only `ecs`); bindings (wave 4) implements `transform_*`, `physics_*`, `audio_*`, `prefab_*`, `scene_*`, `input_*`, `asset_*`, `diagnostics_*`. |
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

---

## 3. Gaps: modules without a design document

Six modules are used by every area document but have no judged design: `core` (wave 1), `platform`,
`audio` (wave 2), `scene`, `physics` (wave 3), `bindings`, `runtime` (wave 4), `samples` (wave 5).
Their contracts are the union of what the eight documents assume, now reconciled by the resolutions above
and written down in `docs/ARCHITECTURE.md` section 4 ("Contracts of undocumented modules"). Before any
wave-1/2/3 implementation of those modules starts, a short design note (same template as 01-08) must be
written from that section; until then `ARCHITECTURE.md` 4 is the binding specification.

The most consequential consolidated contracts:

- **core** (R30, R16, R31): `result<T>`/`error` model, `jobs::job_system` API, `reflect` namespace with
  `type_id`, `type_info`, `field_info` (kinds incl. `ENTITY` by type id and `ASSET_REF`), `set_assert_handler`,
  `memory::allocation_counters()`, `hash::fnv1a64`, `dynamic_library`, `process`, `clock`/`manual_clock`,
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
  `scene.propagate_transforms` (TRANSFORM), `scene.destroy_pending` (FRAME_END); `model_asset -> prefab`.
- **physics** (R6): Jolt behind `physics_world(settings, jobs &)` with a `JPH::JobSystem` adapter and
  `JPH::Allocate*` hooks into `core::memory`; components `rigid_body`, `box/sphere/capsule/convex/mesh_collider`,
  `physics_material`, `character_controller`; layers from `project.json` `extensions.physics_layers`; queries
  `raycast`, `overlap_sphere`, shape casts; systems `physics.push_transforms`, `physics.step` (MAIN_THREAD),
  `physics.pull_transforms`, `physics.contact_events`; `contact_events()` span sorted by body-id pair;
  sleeping query; component sync iterates engine arrays.
- **audio**: `audio_engine(settings)` over miniaudio/`null`, `conjure.audio.clip` asset type + importer,
  components `audio_source`, `audio_listener`, mix groups, 3D spatialisation, `null_backend_stats`; no `ecs`
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
7. **Undocumented modules (section 3).** `core`, `platform`, `scene`, `physics`, `audio`, `bindings`
   have consolidated contracts but no full design; their implementers inherit more freedom than the brief
   intended. Each needs a design note before implementation begins.
8. **`CODE_STYLE.cpp` struct example (R31).** The style file's own example contradicts its rule; the
   resolution is recorded in `CONVENTIONS.md`'s spirit, but the customer-supplied file was not modified.
   Reviewers must apply R31, not the example.

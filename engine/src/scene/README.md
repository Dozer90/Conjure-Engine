# scene

The authored scene over one `ecs::world` (`conjure::scene`, target `conjure_scene`, wave 3a): a
parent/child hierarchy with local, world and render transforms, names, tags, layers and stable guids,
`.scene.json` / `.prefab.json` files over the ECS archive pipeline, fully materialised prefab instances,
in-memory snapshots for play/stop and a `scene_manager` that loads and unloads scene assets. Binding
design: `docs/design/13_scene.md`; the cross-area decisions it cites are R3, R25, R29, R45-R47 and R55
of `docs/design/09_cross_area_resolutions.md`. Public dependencies `conjure::core`, `conjure::ecs` and
`conjure::assets`; no renderer, physics or scripting include anywhere in the module.

## Public headers (`engine/include/conjure/scene/`)

| Header | Contents |
|---|---|
| `log_category.h` | `LOG_SCENE` (`"scene"`) |
| `scene_error.h` | `scene_error` (17 codes), `scene_error_name`, category `"scene"` |
| `components.h` | `parent`, `children`, `local_transform`, `world_transform`, `render_world_transform`, `interpolated_transform`, `transform_state` + `transform_state_flags`, `transform_dirty`, `depth_tag<1..31>`, `name`, `tag`, `layer`, `guid`, `disabled`, `inactive`, `local_bounds`, `prefab_instance`, `prefab_link`, `scene_component_ids`, `MAX_HIERARCHY_DEPTH`, `APPEND_CHILD`, `SCENE_FORMAT_VERSION` |
| `hierarchy.h` | `hierarchy`: `parent_of`, `children_of`, `sibling_index`, `depth_of`, `is_ancestor_of`, `root_of`, `subtree_height`, `for_each_in_subtree`, `collect_subtree`, `subtree_size` (iterative, never recursive) |
| `guid_index.h` | `guid_index`: `find`, `contains`, `insert` (first wins), `erase`, `rebind`, `reserve`, `clear` |
| `scene.h` | `scene_id`, `SCENE_ID_AUTHORED`, `root_entry`, `guid_policy`, `instantiate_options`, `instantiate_report`, `scene` |
| `transform_system.h` | `propagation_stats`, `transform_system` (`run`, `describe`, `set_interpolation_enabled`, `last_stats`), `compute_world_transform` |
| `scene_serializer.h` | `scene_document` / `scene_asset`, `prefab_document`, format and extension constants, `scene_serializer` (save, parse, load, `save_subtree`, `instantiate_subtree`, `capture_subtree`, `bind_asset_refs`, `bind_entity`) |
| `scene_snapshot.h` | `scene_snapshot`, `capture`, `restore` |
| `prefab.h` | `prefab_asset`, `entity_diff_kind`, `entity_diff`, `prefab_diff`, namespace `prefab` (`build_document`, `mark_instance`, `instantiate`, `instantiate_many`, `instance_root_of`, `diff`, `revert`, `apply`, `refresh_instances`, `unpack`), `prefab_builder` |
| `scene_manager.h` | `load_mode`, `load_request_id`, `loaded_scene_info`, `layer_table`, `scene_manager` |
| `register.h` | `register_components`, `register_asset_types`, `register_systems`, the system and importer name constants |

Private (`engine/src/scene/`): `archive_tools.{h,cpp}` (typed reads, deep cloning and the hierarchy
shape of an `entity_archive`, asset reference collection), `asset_ref_raw.h` (the `{uuid, slot}` view the
bind pass writes through, pinned by `static_assert`s and a runtime self-test), `scene_json.{h,cpp}` (file
headers), `scene_observers.cpp`, `scene_link_pass.cpp` (`instantiate_archive`, hierarchy repair),
`transform_propagation.cpp` (the chunk pass), `scene_loader.{h,cpp}` (asset loaders),
`prefab_importer.{h,cpp}` (importers), one `.cpp` per public header.

## How it works

- **One world, one scene.** `scene` holds the ordered roots (`root_entry {entity, owner}`), the guid index,
  the opaque settings document, the name, the destroy and repair queues and a scratch `command_buffer`.
  Membership of a loaded scene asset is "the root it hangs under": a root reparented under another
  scene's root changes owner implicitly; detaching it again gives it the owner of the tree it came from.
- **Hierarchy storage.** `parent {value, siblingIndex}` is the single serialised truth; `children`
  (TRANSIENT `small_vector<entity, 4>`) is the cache, added to a parent on its first child and kept sorted
  with dense sibling indices. Depth is one tag component per level (`depth_tag<D>`, roots carry none), so
  `scene.propagate_transforms` runs `parallel_for_chunks` over one level at a time: the parent of every
  entity in a level finished in the previous level's join.
- **Every structural change is one flush.** `create_entity` builds the final mask (guid, name,
  local_transform and its companions, transform_dirty, parent, depth tag, inactive) and creates once;
  `set_parent`, `set_enabled`, `add_transform` and the link pass record depth retags, dirty and inactive
  changes into the scratch buffer and flush once, so every touched entity moves at most once. Failures
  (`SELF_PARENT`, `HIERARCHY_CYCLE`, `HIERARCHY_TOO_DEEP`, `INVALID_ENTITY`, `DUPLICATE_GUID`) are checked
  before anything changes.
- **Observers tolerate the raw ECS.** `observe_destroyed` enqueues `destroy_entity(child)` per child so a
  10k subtree dies in one call without recursion; a raw `destroy_entity` of a root forgets the root. Raw
  `add/remove<parent>` unlinks immediately where it can and queues a repair that
  `scene.apply_hierarchy_changes` (TRANSFORM, EXCLUSIVE, before propagation) applies in the same frame
  with one WARN per frame; a raw detach keeps the owner of the tree it left (the REMOVED observer reads
  it while the link is still there), so the entity still dies with its loaded scene. Repairs are applied
  one flush each: `retag_subtree` and `refresh_inactive_subtree` decide from the live mask, so two repairs
  on one subtree in a frame must each see the previous one landed (one archetype move per repaired entity,
  bounded by the repair count). `deferred_set_parent` (the bindings' locked-scope route, R55) records the
  same commands plus a repair entry that names the parent the flush installs: the entry waits in the queue
  while the caller's buffer is unflushed (a pass that runs before the flush leaves the tree alone) and is
  applied by the first pass after it, silently through the expected-repair count. Several calls for one
  child before the flush replace the entry's intent and must share the buffer (the count follows the net
  mask delta of one flush); a buffer that is dropped instead of flushed leaves its entry waiting and one
  later raw edit unwarned. The guid observers always index (first mapping wins; a raw duplicate warns);
  the `local_transform` observer adds missing companions.
- **Propagation.** A chunk is recomputed when its `local_transform` column changed since the previous
  run (`since_version`), when it carries `transform_dirty`, when a row's parent was recomputed this run
  (`transform_state::lastUpdateRun`), or when an interpolated pose is involved; a chunk that needs
  nothing is left unstamped (`chunksSkipped`). Both matrices are written in one visit: `render` is the
  lerp of `interpolated_transform` for the entity itself, `parentRender * local` below one, and the world
  matrix elsewhere; the `INTERPOLATED_SUBTREE` bit carries the mode down the tree. A transformless
  ancestor contributes the identity (the pass and `compute_world_transform` both skip it and continue
  upwards). Dirty tags are removed through the grain command buffers, so the second frame after a
  structural change recomputes the moved rows once more and the pass is allocation-free from the third.
- **Instantiation (3.9).** `instantiate_archive` validates the archive, its shape (pre-order, depths,
  cycles) and the guid policy before anything exists, then runs `world::instantiate_batch` once with its
  `outEntities` vector, which reports every created id copy-major in entry order (entries without
  components included, so an empty entity in a file becomes a plain root). Guids are regenerated and rebound where the policy says so; the link pass gathers the children of every
  new internal node in a side table, sorts them by the persisted sibling index, renumbers through the live
  `parent` links and records the `children` component together with depth tags, companions, dirty and
  inactive tags and the root overrides into one flush, so each entity of the archive moves once; archive
  roots attach under `options.parent`, a live external parent or the root list. The bind pass then
  resolves every `asset_ref` through `asset_manager::bind_slot`.
- **Files.** Header keys `format`, `version`, `name`, `settings` (canonical), `roots` (guid strings),
  `archive` (`ecs::write_archive_json` of every entity in hierarchy order, component keys in id order).
  Unknown components survive through `unresolved_components`; save -> load -> save is byte-identical.
  `capture_subtree` drops the root's `parent` (unless external references are kept) so entry 0 of a
  prefab is a true root; `parse_prefab` enforces a single root at entry 0.
- **Prefabs (S9).** Instances are fully materialised: `prefab_instance` on the root, `prefab_link` on every
  entity. `diff` matches entries by link guid, compares components as canonical JSON with the instance's
  local entity references remapped to the asset's indices (so an added entity never shifts the others),
  and treats `parent` by parent guid plus sibling index. `revert` re-instantiates at the same place with
  the old root guid; `apply` writes the link guids as entry guids and drops the root marker; `unpack`
  stops at nested instance roots unless recursive. `prefab_builder` produces a document from nodes and
  components by registered name (the engine's model converter uses it).
- **Snapshots.** A `scene_snapshot` is the typed archive of every entity in hierarchy order plus the root
  order, owners, settings, name and structure version; `restore` clears the scene and instantiates with
  `KEEP` guids (an empty scene stays empty on failure).
- **Scene manager.** `load` goes through `load_blocking` (dependencies first) and keeps the handle in the
  record's `asset_load_batch`; `load_file` parses from a `vfs::file_system` and loads the referenced assets
  as a batch; `request_load` queues a handle that `apply_pending_loads` (FRAME_BEGIN) applies once
  LOADED, in request order, dropping FAILED ones with ERROR and `on_load_failed`. `unload` emits
  `on_before_unload` with the roots still alive (scripting runs `destroy_all` there, R29), destroys them in
  reverse, drops the handles and runs `collect()`. REPLACE first runs everything `instantiate_archive`
  would reject (`detail::check_loadable`: validation, empty entries, shape, duplicate guids) so a bad file
  leaves the current content in place, then unloads everything (`scene::clear()` last), takes the file's
  settings and name and keeps guids; ADDITIVE regenerates colliding guids with one WARN. `info()` and
  `loaded_scenes()` gather a record's roots from the scene on every call (a `restore()` or a reparent
  changes them), so `loaded_scene_info::roots` is never stale.

## Deviations from `13_scene.md` worth knowing

- `instantiate_report::entities` lists every created entity (copy-major, archive order); the serializer,
  the snapshot and the prefab functions need it because `world::instantiate_batch` returns only roots.
- `scene` gained `set_guid`, `resolve_guid` (the `GUID_NOT_FOUND` form of `find_guid`), `set_root_owner`,
  `set_root_order`, `root_owner`, `tag_of`, `layer_of`, `pending_destroy_count` and
  `pending_repair_count`; `hierarchy` gained `subtree_size`; `scene_serializer` gained `write_document`
  and `collect_hierarchy_order`; `prefab_asset` carries its own `id` so a fresh instance can be bound to
  the asset it came from; `scene_manager::info` returns the record by value (`std::optional`).
- `scene::destroy` does not scan the queue for duplicates (O(1) per call): an entity queued twice is
  listed twice in `pending_destroy_count()` and dies once at `destroy_pending`.
- `scene::validate` requires a parentless entity to be a listed root only when it carries a scene
  component; raw ECS entities with gameplay components alone are ignored, so integration tests can mix
  them with a scene.
- `prefab_builder` also reserves `name` and `local_transform` (they come from `add_node`), so a JSON copy
  is `BUILDER_INVALID` instead of a duplicate component at `build()`.
- `parent` is 12 bytes (`entity` is 4-byte aligned, `01_ecs.md` reflection rule), not the 16 of R55; the
  C# mirror follows `type_info::layout_hash()`.
- A transformless ancestor contributes the identity but does not cut the chain: its own ancestors still
  apply, in the pass and in the fresh walk alike.
- The guid policy of a multi-copy instantiation is always `REGENERATE_ALL`.
- `set_enabled` is not a structural change: it does not bump `structure_version()`.

## Cross-module requests

- Resolved at wave 3 integration: `world::instantiate_batch(archive, outRoots, commands, outEntities)`
  reports every created id (the scene's `instantiation_collector` is gone), the ECS scheduler keeps its
  stage scratch in the phase state so a whole TRANSFORM phase is gated with `CONJURE_CHECK_NO_ALLOCATIONS`,
  `ecs::component_type_desc` carries a `serializer` (scripting's `script_component` rides through scene
  files on it), `importer_registry::find_for_path` picks the `.scene.json` / `.prefab.json` importers for
  the database, and `world::get_raw(entity, id) const` documents that grain jobs may read other
  entities' columns while the world is locked.
- `tests/benchmarks/bench_scene.cpp` (`propagate_10k_flat`, `propagate_10k_depth8`,
  `set_parent_subtree_1k`, `instantiate_prefab_100`, `snapshot_capture_restore_10k`,
  `destroy_cascade_10k`; `13_scene.md` 10) guards the 0.3 ms propagation and 50 ms snapshot budgets; the
  directory does not exist yet and is outside this module.

## Threading

`scene`, `scene_manager`, `guid_index`, the prefab functions and the serializer are owner-thread only
(the world's rules apply and return `WRONG_THREAD` / `STRUCTURE_LOCKED`); events fire on that thread.
`scene.propagate_transforms` is the only parallel code: grain jobs write `world_transform`,
`render_world_transform` and `transform_state` of their own rows, read their parents' columns through
const random access (finished in the previous level) and accumulate relaxed counters once per chunk.
The asset loaders and importers run on workers and touch only their inputs and `component_registry`
lookups. Observers run synchronously inside structural calls on the owner thread and only enqueue into
the deferred buffer or push into scene-side queues.

## Testing

`tests/scene/` builds `conjure_scene_tests`: one `test_<header>.cpp` per public header plus
`test_name_tag_layer`, `test_transform_propagation` and `test_scene_serialisation`.
`scene_test_fixture.h` provides the test components (`test_asset_holder`, `test_entity_holder`,
`test_speed`), `scene_fixture` (registry, world, asset manager over a memory source, `scene_manager`,
scheduler with the four scene systems, `step()`, validation in the destructor) and `for_each_job_mode`.
The scheduler runs its stages inline (`parallelSystems = false`) so `parallel_for_chunks` is the
parallel path under test; the allocation gate of the 10k propagation drives `transform_system::run`
through a `system_context` directly, because the scheduler's own per-stage job vectors allocate.

```
python tools/build.py --build-dir build/mod_scene --target conjure_scene_tests --test -R scene
build/mod_scene/bin/conjure_scene_tests
```

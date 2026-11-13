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
  `scene.propagate_transforms` dispatches its level jobs with `job_system::parallel_for` one level at a
  time: the parent of every entity in a level finished in the previous level's join.
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
- **Propagation.** A chunk is recomputed as a whole when its `local_transform` column changed since the
  previous run (`since_version`), when it carries `transform_dirty` or when it holds interpolated poses;
  otherwise only its rows marked `transform_state::pending` are recomputed, the mark a parent's visit
  left on each child when it recomputed its world matrix (`transform_state::lastUpdateRun`, the S4
  stamp the child still reads), sits in an interpolated subtree or changed flags. Every level runs in
  three steps on the system thread: *collect* walks the level's chunks once (`query::for_each_chunk`),
  keeps the ones with work and counts their rows (a clean chunk costs one sequential scan of its 8-byte
  state column and is left unstamped, `chunksSkipped`; no parent is looked up for a clean row);
  *dispatch* splits the kept chunks into jobs of about `ROWS_PER_JOB` (128) dirty rows over
  `job_system::parallel_for` (inline without a job system or below `INLINE_ROW_LIMIT` = 512 rows, where
  the dispatch costs more than the rows; the limit is a fixed row count, not one per worker, see the
  deviations), each job resolving a row's parent once (consecutive siblings share the lookup) and
  marking the row's children through its own `children` column (a transformless child, S5, is not in
  any level: the mark goes to its children, an explicit stack bounded by `MAX_HIERARCHY_DEPTH`); then
  the `transform_dirty` tags of the recomputed chunks are removed through the system's command buffer.
  `describe()` declares reads `{local_transform, parent, children, interpolated_transform,
  transform_dirty}` and writes `{world_transform, render_world_transform, transform_state}`; the
  `children` read is what keeps a TRANSFORM-phase writer of the lists out of the pass's stage (the
  scheduler packs stages from the declared masks alone). Both matrices are written in one visit: `render` is the lerp of
  `interpolated_transform` for the entity itself, `parentRender * local` below one, and the world matrix
  elsewhere; the `INTERPOLATED_SUBTREE` bit carries the mode down the tree. A transformless ancestor
  contributes the identity (the pass and `compute_world_transform` both skip it and continue upwards).
  The second frame after a structural change recomputes the moved rows once more and the pass is
  allocation-free from the third (the level scratch, `mLevelChunks` and `mJobBounds`, keeps its
  capacity). Work is proportional to the dirty rows, where a dirty row is one of a stamped chunk or a
  descendant of one: moving one root recomputes every root of its chunk and their subtrees (S4 is
  chunk-granular), so `scene.transform_propagation_10k_sparse` (10 of 100 roots in one 67-row chunk)
  recomputes 6,770 of its 10,000 rows, 68 % of the dense scenario.
- **Instantiation (3.9).** `instantiate_archive` validates the archive, its shape (pre-order, depths,
  cycles), every `layer` index (`detail::check_layers`: 32 or more is `BAD_FORMAT` naming the entry, S15;
  `layer` is a grouping label, never a render mask) and the guid policy before anything exists, then runs
  `world::instantiate_batch` once with its `outEntities` vector, which reports every created id copy-major
  in entry order (entries without components included, so an empty entity in a file becomes a plain
  root). Guids are regenerated and rebound where the policy says so (the two regenerate policies also give
  a null guid a fresh one; `KEEP`, which snapshot restore and REPLACE loads use, leaves it null); the link
  pass gathers the children of every new internal node in a side table, sorts them by the persisted
  sibling index, renumbers through the live `parent` links and records the `children` component together
  with depth tags, companions, dirty and inactive tags and the root overrides into one flush, so each
  entity of the archive moves once; archive roots attach under `options.parent`, a live external parent or
  the root list. The bind pass then resolves every `asset_ref` through `asset_manager::bind_slot`.
- **Captures (S15).** `capture`, `capture_subtree` (and so `duplicate_subtree`, `save_subtree` and the
  prefab `build_document`, `diff` and `apply`) and `save_scene` run `detail::check_layers` on what they
  captured, with the captured entities, so a layer of 32 or more is `BAD_FORMAT` naming the entity before
  anything is destroyed or written. Otherwise play/stop (whose `restore` clears first), the editor's
  destroy undo and a saved file would carry a value that instantiation refuses. `refresh_instances` skips
  an instance whose `diff` fails, so it never reverts one.
- **Files.** Header keys `format`, `version`, `name`, `settings` (canonical), `roots` (guid strings),
  `archive` (`ecs::write_archive_json` of every entity in hierarchy order, component keys in id order).
  Unknown components survive through `unresolved_components`; save -> load -> save is byte-identical.
  `capture_subtree` drops the root's `parent` (unless external references are kept) so entry 0 of a
  prefab is a true root; `parse_prefab` enforces a single root at entry 0 (`detail::check_prefab_roots`,
  shared with `build_document`, `instantiate_many` and `revert`: `BAD_FORMAT` for an empty archive,
  `PREFAB_MULTIPLE_ROOTS` otherwise, naming the misplaced root or the root count; the message naming the
  archive is formatted only on failure) and unique entry guids (`DUPLICATE_GUID`).
- **Prefabs (S9).** Instances are fully materialised: `prefab_instance` on the root, `prefab_link` on every
  entity. `diff` matches entries by link guid, compares components as canonical JSON with the instance's
  local entity references remapped to the asset's indices (so an added entity never shifts the others),
  and treats `parent` by parent guid plus sibling index. `revert` first checks that the current asset still
  fits at the instance's place (archive, single root, depth limit, layer range) and only then
  re-instantiates there with the old root guid, so those failures leave the instance untouched; `apply`
  writes the link guids as entry guids (a link guid already used by an earlier entity, as after a
  duplicate, gets a fresh one) and drops the root marker; `unpack` stops at nested instance roots unless
  recursive. `prefab_builder` produces a document from nodes and components by registered name (the
  engine's model converter uses it).
- **Snapshots.** A `scene_snapshot` is the typed archive of every entity in hierarchy order plus the root
  order, owners, settings, name and the structure version at capture (informational: `restore` cannot
  rewind the counter, its clear and instantiate bump it like any structural change); `restore` clears the
  scene and instantiates with `KEEP` guids (an empty scene stays empty on failure), and `capture` refuses a
  layer of 32 or more so a captured snapshot never fails that way.
- **Scene manager.** `load` goes through `load_blocking` (dependencies first) and keeps the handle in the
  record's `asset_load_batch`; `load_file` parses from a `vfs::file_system` and loads the referenced assets
  as a batch; `request_load` queues a handle that `apply_pending_loads` (FRAME_BEGIN) applies once
  LOADED, in request order, dropping FAILED ones with ERROR and `on_load_failed`. `unload` emits
  `on_before_unload` with the roots still alive (scripting runs `destroy_all` there, R29), destroys them in
  reverse, drops the handles and runs `collect()`. Handlers may load and unload scenes, so neither
  `unload` nor `apply_document` holds a record pointer across an event: both look the record up again by
  id, and a load whose scene a handler unloaded returns `SCENE_NOT_LOADED` without touching the newer
  scene that took over its record slot. `unload_all` unloads every record in reverse load order and, when
  any entity is left (authored content, raw entities), fires `on_before_unload` once more with
  `SCENE_ID_AUTHORED` and the remaining roots before `scene::clear()`; a scene a handler loaded meanwhile
  is cleared and retired with the rest. REPLACE first runs everything `instantiate_archive` would reject
  (`detail::check_loadable`: validation, empty entries, shape, layer range, duplicate guids) so a bad
  file leaves the current content in place, then unloads everything (`scene::clear()` last), takes the
  file's settings and name and keeps guids; ADDITIVE regenerates colliding guids with one WARN. `info()`
  and `loaded_scenes()` gather a record's roots from the scene on every call (a `restore()` or a reparent
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
- `transform_dirty` is only added to entities that carry a `local_transform` (3.2 step 5 says "always"):
  the propagation pass is the tag's only remover and never visits a transformless entity, which would keep
  the tag forever. `set_parent`, `deferred_set_parent`, the repair pass and the link pass all skip them.
- `create_entities` with a null template is one structural operation (one `structure_version()` bump, one
  `on_structure_changed`) and atomic like the template path: when a creation fails, the entities the call
  already made are destroyed again without a bump and the error is returned, so a failed call announces
  nothing.
- `transform_state` carries a `pending` byte (in the padding of 3.4's `{lastUpdateRun, flags}`, still 8
  bytes) that the parent's level writes and the child's level clears: the design's pull-only stamp made
  every level look up every row's parent (O(N) random reads per frame whatever the dirty set); the mark
  keeps the stamp as the truth the child reads and adds the address where to read it. Written only when a
  transform changed, like the stamp, so it is not the per-frame dirty bit section 1 rejects.
- The pass's inline limit (`INLINE_ROW_LIMIT` = 512 dirty rows) is not scaled by the worker count. The
  review-4 suggestion (256 rows per worker, 4,096 with 16) was measured in Development on the reference
  machine with interleaved A/B runs of `scene.transform_propagation_10k_sparse` / `_10k`: with 16 workers
  the fixed limit gives 0.152-0.163 / 0.223-0.225 ms (median), the scaled one 0.201-0.206 / 0.322-0.340 ms;
  with 2 workers both limits coincide at 512 and measure the same (0.102-0.106 / 0.148-0.151 ms). A level
  that runs inline between two dispatched levels used to let the workers fall asleep (they spun a few
  yields only), so the next dispatch paid the wake-up again, while back-to-back dispatches paid it once per
  run; the 1,072-row sparse level is therefore cheaper dispatched than inline. The performance round gave
  the job system a 50 us spin window and a spinlock on its queue (`core/jobs` README), which moved the
  two scenarios to 0.136 / 0.173 ms with 16 workers (0.125 / 0.172 with the ticket lock the round first
  used, `tests/benchmarks/README.md`); the sparse row stays over its 0.100 ms budget
  (`tests/benchmarks/README.md`, deviations).

## Cross-module requests

- Resolved at wave 3 integration: `world::instantiate_batch(archive, outRoots, commands, outEntities)`
  reports every created id (the scene's `instantiation_collector` is gone), the ECS scheduler keeps its
  stage scratch in the phase state so a whole TRANSFORM phase is gated with `CONJURE_CHECK_NO_ALLOCATIONS`,
  `ecs::component_type_desc` carries a `serializer` (scripting's `script_component` rides through scene
  files on it), `importer_registry::find_for_path` picks the `.scene.json` / `.prefab.json` importers for
  the database, and `world::get_raw(entity, id) const` documents that grain jobs may read other
  entities' columns while the world is locked.
- `tests/benchmarks/bench_scene.cpp` (`scene.transform_propagation_10k`,
  `scene.transform_propagation_10k_sparse`, `scene.snapshot_capture_restore_10k`, `scene.load_10k`; limits
  in `tests/benchmarks/thresholds.json`) guards the propagation and snapshot budgets of `13_scene.md` 10;
  the design's `propagate_10k_depth8`, `set_parent_subtree_1k`, `instantiate_prefab_100` and
  `destroy_cascade_10k` are not written.

## Threading

`scene`, `scene_manager`, `guid_index`, the prefab functions and the serializer are owner-thread only
(the world's rules apply and return `WRONG_THREAD` / `STRUCTURE_LOCKED`); events fire on that thread.
`scene.propagate_transforms` is the only parallel code: level jobs write `world_transform`,
`render_world_transform` and `transform_state` of their own rows, read their parents' columns through
const random access (finished in the previous level), read their own rows' `children` lists and write
the `transform_state::pending` byte of those children (rows of the next level, which no job touches
until the level join; one parent per child, so no two jobs write the same byte; the mark goes through
the non-const `world::get_raw(entity, id, false)`, which stamps nothing) and add their totals to relaxed
counters once per job range (a local tally per range, so the shared cache line is not written per chunk;
`chunksSkipped` is counted by the serial collect step). The pass takes `world::lock_iteration()` for the
dispatch and records dirty-tag removals on the system thread only. Chunk parallelism follows
`system_context::jobs()`: with a job system present the level jobs fan out even when the scheduler was
built with `parallelChunks = false` (the flag has no accessor, requested from ecs as
`system_context::parallel_chunks()`; every caller that disables it today also runs without a job
system). The asset loaders and importers run on workers and touch only their inputs and
`component_registry` lookups. Observers run synchronously inside structural calls on the owner thread
and only enqueue into the deferred buffer or push into scene-side queues.

## Testing

`tests/scene/` builds `conjure_scene_tests`: one `test_<header>.cpp` per public header plus
`test_name_tag_layer`, `test_transform_propagation` and `test_scene_serialisation`.
`scene_test_fixture.h` provides the test components (`test_asset_holder`, `test_entity_holder`,
`test_speed`), `scene_fixture` (registry, world, asset manager over a memory source, `scene_manager`,
scheduler with the four scene systems, `step()`, validation in the destructor) and `for_each_job_mode`.
The scheduler runs its stages inline (`parallelSystems = false`); the pass fans its level jobs out with
`job_system::parallel_for` when the fixture has a job system. The 10k allocation gate runs twice:
`propagation - 10k entities allocation-free after frame 2 [parallel]` drives `transform_system::run`
through a `system_context` directly, and
`propagation - whole TRANSFORM phase through the scheduler allocation-free after frame 2 [parallel]`
runs the whole phase through the scheduler, which keeps its per-stage bookkeeping in the phase state.

```
python tools/build.py --build-dir build/mod_scene --target conjure_scene_tests --test -R scene
build/mod_scene/bin/conjure_scene_tests
```

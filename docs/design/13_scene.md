# 13 - Scene module design (`conjure::scene`, wave 3a)

Status: **binding** (owner: lead architect). Expands `ARCHITECTURE.md` 4.4 into an implementable design.
Module `conjure_scene`, namespace `conjure::scene`, public headers in `engine/include/conjure/scene/`,
sources in `engine/src/scene/`, tests in `tests/scene/`. Public link dependencies: `conjure_ecs`,
`conjure_assets` (and transitively `conjure_core`). No renderer, physics, scripting or engine include
anywhere in the module (R7, R29). Where this document and `ARCHITECTURE.md` 4.4 differ in detail, this
document wins; where it differs from `09_cross_area_resolutions.md`, that file wins and this one has a bug.

Inputs read in full: `01_ecs.md`, `10_core.md`, `05_assets.md` 2-4/8, `06_engine_loop.md` 3.1/3.2/3.4/8,
`04_editor.md` 3.7/4.4/8, `03_scripting.md` 8, `08_testing.md` 5.2/6.1/7.2, R3, R25, R29.

---

## 1. Overview and decisions

The scene module turns one `ecs::world` into an authored scene: a parent/child hierarchy with local and
world transforms, names, tags, layers and stable guids, scene files and prefab assets in JSON, in-memory
snapshots for play/stop, and a manager that loads and unloads scene assets (replace or additive) through
the asset pipeline. It owns exactly one per-frame hot path, `scene.propagate_transforms` (R3).

| Id | Decision | Rationale |
|---|---|---|
| S1 | **One `ecs::world` per `scene::scene`; additive scene assets share the world.** A loaded scene asset is a record `{scene_id, roots, asset handles}`; membership of an entity is "the root it hangs under". | One world keeps queries, bindings and systems trivial (`engine_context::world`); Unity semantics for additive loads without per-entity bookkeeping. |
| S2 | **Hierarchy depth is encoded as a tag component per depth (`depth_tag<D>`, D = 1..31, TRANSIENT, EDITOR_HIDDEN); roots are `without<parent>`.** `MAX_HIERARCHY_DEPTH = 32`. | Makes "one level at a time" (R3, `01_ecs.md` 8) literally true: every level is a set of chunks, the `changed<local_transform>` chunk filter applies per level, and children of one wide root still fan out. Reparenting across depths costs one archetype move per descendant (measured, acceptable: ~1 us each). |
| S3 | **`children` is `core::small_vector<ecs::entity, 4>`, TRANSIENT, rebuilt from `parent` on load.** Sibling order is persisted as `parent::siblingIndex`; root order is persisted in the scene header `roots`. | The vector is a cache for traversal and the editor; `parent` is the single serialised truth, so files cannot encode an inconsistent tree. |
| S4 | **Propagation recomputes an entity when its chunk's `local_transform` changed, its chunk carries `transform_dirty`, or its parent recomputed this run.** The last fact is a per-entity `transform_state::lastUpdateRun` stamp written by the parent's level pass and read after the level join; the parent's visit also sets the child's `transform_state::pending` mark, so the child's level finds the row without a parent lookup per row (3.4). `render_world_transform` is written in the same pass (R3, `06_engine_loop.md` 3.2). | Chunk-granular change detection has no false negatives; the stamp carries dirtiness down the tree with O(1) random access and no allocation. |
| S5 | **Transform companions are a unit.** Every entity with `local_transform` also has `world_transform`, `render_world_transform` and `transform_state`; `scene` adds them together, and an observer repairs raw adds. An ancestor without `local_transform` contributes identity (its children are positioned in world space). | One rule, no "optional world transform" branches in the hot pass. |
| S6 | **Cascaded destroy is observer-driven through the ECS deferred buffer; the scene never recurses.** `observe_destroyed` enqueues `destroy_entity(child)` per child; the ECS drains until empty. `scene::destroy()` queues for `scene.destroy_pending` (FRAME_END); `scene::destroy_now()` is the editor/test path. | Binding per `ARCHITECTURE.md` 4.4 and `01_ecs.md` D9; depth is bounded by the drain loop, not the C++ stack. |
| S7 | **All hierarchy mutation goes through `scene` (`set_parent`, `create_entity`, instantiate, destroy).** Raw `world.add/remove<parent>` is tolerated: observers queue a repair that `scene.apply_hierarchy_changes` (TRANSFORM, EXCLUSIVE, before propagation) applies in the same frame with one WARN per frame. | Scripts can reach the raw ECS (`03_scripting.md`); a stale tree may never be the result. |
| S8 | **Scene and prefab files are JSON: a small header plus `ecs::write_archive_json` output.** Entities are written in hierarchy order (roots by root order, children by sibling order) and each entity's component keys in name order (`01_ecs.md` D14; code sweep 2026-11-12, F606); unknown components round-trip verbatim (`unresolved_components`) in the same order. Save -> load -> save is byte-identical. | One archive pipeline (`01_ecs.md` D10); diffable files that do not depend on the registration order of the build that saved them; plugin data survives. |
| S9 | **Prefab instances are fully materialised.** The instance root carries `prefab_instance { asset_ref<prefab_asset> }`, every entity of the instance carries `prefab_link { uuid assetGuid }`. Nested prefabs are baked into the outer prefab's archive with their markers. Overrides are computed by `prefab::diff`, not stored. | Scenes load without resolving prefabs; no override-record format in v1; `apply`/`revert`/`unpack` are simple archive operations. |
| S10 | **`scene_snapshot` is an in-memory `entity_archive` plus roots order, settings and structure version.** No file form. | Play/stop needs 10k entities captured and restored under 50 ms; `ops.copy` on typed bytes is a memcpy per column. |
| S11 | **Asset residency belongs to `scene_manager`.** A scene asset's slot holds its dependency handles (`05_assets.md` D9); the manager holds the scene handle in an `asset_load_batch`, gates deferred loads on `is_complete()`, binds slots after instantiation (R25) and calls `collect()` after unload. | Nothing in a component owns an asset (`asset_ref` is non-owning, `05_assets.md` D2). |
| S12 | **`model_asset -> prefab` is a `prefab_builder` in scene plus a converter in `engine`.** Scene cannot name renderer types (dependency graph, `ARCHITECTURE.md` 2.2); it offers a type-agnostic builder (`add_node`, `add_component_json`) and the engine's `renderer_subsystem` walks `model_asset` nodes into it. | Keeps the graph acyclic while the mapping is specified here. |
| S13 | **Scene settings are an opaque `core::json::document`.** Scene stores, preserves and canonicalises it; engine subsystems read the keys they own (`ambient`, `skybox`, `gravity`, `physics_layers`); `on_settings_changed` notifies them. | Scene does not know renderer or physics settings and must not. |
| S14 | **Transform algebra uses `math::transform` composition, not matrix decomposition.** `keepWorld` reparenting computes `newLocal = world(newParent).inverse() * world(child)` with fresh ancestor walks. | Exact for TRS without shear; avoids drift from decomposing matrices every reparent. Shear from non-uniform parent scale is unsupported (as in Unity) and documented. |
| S15 | **`layer` is a grouping label, not a render mask.** It serves `find_by_layer`, scripts and the editor. What a camera draws is `mesh_renderer_component::layerMask & camera_component::layerMask` (the same bit numbering, so a game can keep the two in step); sprites are not masked. An index of 32 or more is `BAD_FORMAT` at both ends: every capture (`capture`, `capture_subtree` and so `duplicate_subtree`, `save_subtree` and the prefab `build_document`, `diff` and `apply`, and `save_scene`) refuses it naming the entity, and `instantiate_archive` refuses it naming the entry, next to its depth pre-check (3.9). | Making the label authoritative would be a renderer change, and OR-ing its bit into the mesh mask would not hide anything because the default mesh mask keeps bit 0. `set_layer` already refused 32 and above, but a file, a prefab or a raw write (the editor's `component.set` goes through `component_from_json`, which ignores the reflected maximum) could carry such a value. Capture refuses it too because a later instantiation would fail after content is gone: `restore` clears the scene first (play/stop), and the editor's destroy undo replays a captured subtree (code sweep 2026-11-12, F151 and F363). |
| S16 | **`set_local_transform` also takes a `math::transform`** (forwarding to `local_transform::from_transform`), so `local_transform_of`'s result feeds the setter directly. `destroy` / `destroy_now`, `add_node(parentIndex, name)` and the getter's return type keep their names and shapes. | The asymmetric pair made every caller convert by hand (the editor did so three times). A bare `{}` argument is now ambiguous and names its type; renaming or retyping the other functions would break bindings, editor, runtime and docs for no behaviour gain (code sweep 2026-11-12, F635). |

Rejected: a hierarchy container outside the ECS (two sources of truth); per-frame per-entity dirty bits (the
S4 stamp is written only when a transform changes); override records in prefab files (not needed for the
PacMan-class goal); binary scene files (JSON suffices for 10k entities); a scene-owned level table of entity
arrays (would force `parallel_for` over ids instead of chunks and lose the chunk change filter).

---

## 2. Data model

Naming (R31): plain component structs use unprefixed camelCase fields; classes use `mCamelCase` members.
All components are named `conjure.scene.<name>` through `CONJURE_COMPONENT` and reflected with
`CONJURE_REFLECT_STRUCT` unless marked TRANSIENT without reflection.

### 2.1 Components (`components.h`)

| Component | Fields | Flags | Serialised | Notes |
|---|---|---|---|---|
| `parent` | `ecs::entity value = NULL_ENTITY; u32 siblingIndex = 0;` | - | yes | `value` has reflection kind `ENTITY`, so archives remap it (local index in files). |
| `children` | `core::small_vector<ecs::entity, 4> entities;` | TRANSIENT | no | Non-trivial; `component_ops` generated from special members. Rebuilt by the link pass. |
| `local_transform` | `math::vec3 position = {0,0,0}; math::quat rotation = identity; math::vec3 scale = {1,1,1};` | - | yes | `static_assert(sizeof == 40 && trivially copyable)`; byte-identical to `math::transform`, mirrored by C# `LocalTransform` (layout hash checked). `to_transform()` / `from_transform()` inline helpers. |
| `world_transform` | `math::mat4 matrix = identity;` | TRANSIENT | no | Simulation truth. Reflection with `readOnly` attribute for the inspector. |
| `render_world_transform` | `math::mat4 matrix = identity;` | TRANSIENT | no | What extraction reads. |
| `interpolated_transform` | `math::transform previous; math::transform current;` | TRANSIENT | no | World-space poses. Scene only defines and registers it; physics adds it (deferred buffer, `ADDED` observer of a dynamic/kinematic `rigid_body` or a `character_controller`), writes it in `physics.pull_transforms` and removes it when the body dies on a living entity (`14_physics.md` 3.3, `06_engine_loop.md` 3.2, R47). |
| `transform_state` | `u32 lastUpdateRun = 0; transform_state_flags flags = NONE; bool pending = false;` | TRANSIENT, EDITOR_HIDDEN | no | `enum class transform_state_flags : u8 { NONE = 0, INTERPOLATED_SUBTREE = 1 << 0 }` + `CONJURE_ENUM_FLAGS`. Still 8 bytes (`static_assert`); `pending` is set by the parent's level when the parent's world matrix changed, it sits in an interpolated subtree or its flags changed, and cleared by the child's level (3.4). |
| `transform_dirty` | (tag) | TRANSIENT, EDITOR_HIDDEN | no | Set by structural hierarchy changes; cleared by the propagation pass through its command buffer. |
| `depth_tag<D>` (D = 1..31) | (tag) | TRANSIENT, EDITOR_HIDDEN | no | Names `conjure.scene.depth_01` .. `depth_31`. Roots carry none. |
| `name` | `core::string_id value;` | - | yes (string) | Interned in every configuration (`10_core.md` 2.4). |
| `tag` | `core::string_id value;` | - | yes (string) | |
| `layer` | `u8 index = 0;` | - | yes | 0..31; a grouping index for `find_by_layer`, scripts and the editor, not a render mask (S15): a camera draws a mesh when `mesh_renderer_component::layerMask & camera_component::layerMask` is non-zero (the same bit numbering), and sprites are not masked. An index of 32 or more fails a capture (naming the entity) and instantiation (naming the archive entry) with `BAD_FORMAT` (3.9, 3.14). Names live in `layer_table` (2.4). |
| `guid` | `core::uuid value;` | - | yes (string) | Editor identity; indexed by `guid_index`. |
| `disabled` | (tag) | - | yes (`{}`) | Self state. |
| `inactive` | (tag) | TRANSIENT, EDITOR_HIDDEN | no | On every descendant of a `disabled` entity; consumers filter `without<disabled>().without<inactive>()`. |
| `local_bounds` | `math::aabb box;` | TRANSIENT | no | Written by engine subsystems (mesh/collider bounds); `scene::world_bounds` transforms it. |
| `prefab_instance` | `assets::asset_ref<prefab_asset> prefab;` | - | yes (uuid) | On the instance root. Bound by the bind pass; declared as a dependency by the importer. |
| `prefab_link` | `core::uuid assetGuid;` | - | yes | On every entity instantiated from a prefab; maps to the asset entry's `guid`. |

```cpp
namespace conjure::scene {

    inline constexpr u32 MAX_HIERARCHY_DEPTH = 32;          // depth 0 = root; depth_tag<1..31>
    inline constexpr u32 APPEND_CHILD = 0xFFFFFFFFu;        // sibling index meaning "last"

    struct parent { ecs::entity value = ecs::NULL_ENTITY; u32 siblingIndex = 0; };
    CONJURE_COMPONENT(conjure::scene::parent, "conjure.scene.parent");
    CONJURE_REFLECT_STRUCT(conjure::scene::parent, "conjure.scene.parent",
        CONJURE_FIELD(value), CONJURE_FIELD(siblingIndex, .readOnly = true))

    struct children { core::small_vector<ecs::entity, 4> entities = {}; };     // registered TRANSIENT
    CONJURE_COMPONENT(conjure::scene::children, "conjure.scene.children");

    struct local_transform {
        math::vec3 position = math::vec3(0.0f);
        math::quat rotation = math::quat(1.0f, 0.0f, 0.0f, 0.0f);
        math::vec3 scale = math::vec3(1.0f);
        [[nodiscard]] math::transform to_transform() const noexcept;
        [[nodiscard]] static local_transform from_transform(const math::transform &value) noexcept;
    };
    static_assert(sizeof(local_transform) == sizeof(math::transform));
    template <u8 Depth> struct depth_tag {};   // component_name<depth_tag<D>>::VALUE = "conjure.scene.depth_DD"
}
```

`register_components(component_registry &)` registers every type above in a fixed order (so ids agree in
every world of a process) and records the resolved ids in a `scene_component_ids` struct
(`components.h`): `parent, children, localTransform, worldTransform, renderWorldTransform,
interpolatedTransform, transformState, transformDirty, depth[MAX_HIERARCHY_DEPTH] (depth[0] INVALID), name,
tag, layer, guid, disabled, inactive, localBounds, prefabInstance, prefabLink`, obtained with
`scene_component_ids::resolve(const component_registry &)`.

### 2.2 Scene-side state (`scene.h`, private members)

`scene` holds: `ecs::world &mWorld`, `scene_component_ids mIds`, `std::vector<root_entry> mRoots`
(`root_entry { ecs::entity entity; scene_id owner; }`, ordered), `guid_index mGuids`, `core::json::document
mSettings`, `std::string mName`, `u64 mStructureVersion = 1`, `std::vector<ecs::entity> mDestroyQueue`,
`mRepairQueue` (S7), `ecs::command_buffer mScratch` (retag flushes), `core::small_vector<ecs::entity, 64>
mWalkStack` (iterative traversals), `u8 mMaxDepthInUse`, observer ids, `bool mInOperation` (observers skip
bookkeeping while the scene itself mutates), events `on_structure_changed`, `on_settings_changed`.

`scene_id { u32 index = 0; u32 generation = 0; }` identifies a loaded scene asset (generation 0 = none;
`SCENE_ID_AUTHORED` = `{0, 1}` is the implicit owner of entities created by hand).

### 2.3 Scene file format (`.scene.json`, asset type `conjure.scene.scene`; extensions per R45)

```json
{
  "format": "conjure.scene",
  "version": 1,
  "name": "main",
  "settings": {"ambient": [0.1, 0.1, 0.12], "gravity": [0, -9.81, 0], "skybox": "4f1b...-...-...c2"},
  "roots": ["6d2a9f0e-3b7c-4d1e-9a2f-0c5b7e8d1a21", "b1f0c3d2-aa10-4e22-8c71-2fd3e4a5b6c7"],
  "archive": {
    "entities": [
      {"components": {
        "com.studio.spinner": {"speed": 2.5},
        "conjure.renderer.mesh_renderer": {"mesh": "9a77...-...-...e01", "materials": ["c3d4...-...", "", "", ""]},
        "conjure.scene.guid": {"value": "6d2a9f0e-3b7c-4d1e-9a2f-0c5b7e8d1a21"},
        "conjure.scene.layer": {"index": 3},
        "conjure.scene.local_transform": {"position": [0, 1, 0], "rotation": [0, 0, 0, 1], "scale": [1, 1, 1]},
        "conjure.scene.name": {"value": "Player"},
        "conjure.scene.tag": {"value": "Player"}
      }},
      {"components": {
        "conjure.scene.guid": {"value": "0e8d7c6b-5a49-4382-9170-6f5e4d3c2b1a"},
        "conjure.scene.local_transform": {"position": [0, 2, 0], "rotation": [0, 0, 0, 1], "scale": [1, 1, 1]},
        "conjure.scene.name": {"value": "Lamp"},
        "conjure.scene.parent": {"value": 0, "siblingIndex": 0}
      }},
      {"components": {
        "conjure.scene.disabled": {},
        "conjure.scene.guid": {"value": "b1f0c3d2-aa10-4e22-8c71-2fd3e4a5b6c7"},
        "conjure.scene.local_transform": {"position": [0, 0, 0], "rotation": [0, 0, 0, 1], "scale": [50, 1, 50]},
        "conjure.scene.name": {"value": "Ground"}
      }}
    ]
  }
}
```

Rules: header keys in exactly this order (`format`, `version`, `name`, `settings`, `roots`, `archive`);
`settings` is `document::to_canonical_string()` spliced with `write_raw_json`; `archive` is the verbatim
output of `ecs::write_archive_json` with entities in hierarchy order (roots in root order, each subtree
depth-first in sibling order) and each entity's component keys in name order (the byte order of the UTF-8 text,
as in the canonical cooked form; `01_ecs.md` D14), so the text does not depend on component ids and therefore
not on which modules the saving build registered, or in which order; `parent.value` is an archive-local
index; `asset_ref` fields are uuid strings (empty = none); `com.studio.spinner` is an unknown component kept
through `unresolved_components` and written in the same name order. A file saved before this rule (keys in component
id order) loads unchanged and is reordered once, by its next save. `version` is `SCENE_FORMAT_VERSION = 1` (greater
-> `UNSUPPORTED_VERSION`; wrong `format` -> `BAD_FORMAT`). A guid in `roots` absent from the archive is skipped with
WARN; a root entry missing from `roots` is appended in archive order with WARN.

### 2.4 Prefab file format (`.prefab.json`, asset type `conjure.scene.prefab`)

Same shape with `"format": "conjure.prefab"`, no `settings`, no `roots`; entry 0 is the root and must be the
only entry without `parent` (`PREFAB_MULTIPLE_ROOTS`). Entities keep their `guid` (the prefab-local guid that
instances record in `prefab_link`). A nested instance is an entity with `prefab_instance` plus its baked subtree.
Payloads: `prefab_asset { std::shared_ptr<const ecs::entity_archive> archive; std::string name; core::uuid
rootGuid; std::vector<assets::asset_id> dependencies; }` (sorted, from the blob header) and `scene_asset =
scene_document { std::string name; core::json::document settings; std::vector<core::uuid> roots;
std::shared_ptr<const ecs::entity_archive> archive; }`.

`layer_table` (`scene_manager.h`): `std::array<core::string_id, 32> names`; `set_name(u8, string_id)`,
`find(string_id) -> std::optional<u8>`, filled by the engine from `project.json`.

### 2.5 Snapshot (`scene_snapshot.h`)

`struct scene_snapshot` (move-only, owns its bytes): `ecs::entity_archive archive` (every entity in hierarchy
order, TRANSIENT columns excluded), `std::vector<u32> rootEntries` (archive indices of roots, in root order),
`std::vector<scene_id> rootOwners` (parallel), `core::json::document settings` (deep copy), `std::string
name`, `u64 structureVersion`, `entity_count()`.

---

## 3. Algorithms and flow

### 3.1 Creating entities

`scene::create_entity(name, parent, index)`: build the mask `{guid, name, local_transform, world_transform,
render_world_transform, transform_state, transform_dirty}` plus `parent` and `depth_tag<depth(parent) + 1>`
when parented; one `world.create_entity(mask)` (final archetype, zero moves); write `guid::value =
uuid::generate()`, `name`, `parent{value, index}`; insert into the parent's `children` at `index` (or into
`mRoots`) and renumber the following siblings' `siblingIndex`; `++mStructureVersion`. Observers see
`mInOperation == true` and skip bookkeeping; the guid observer still indexes. `create_entities(count,
templateEntity, parent, out)` captures the template subtree once (`capture_subtree`) and runs 3.9 with `count`
copies. `add_transform(entity, local)` adds the companions and `transform_dirty` in one `mScratch` flush (one
move); the companion observer (`observe(localTransform, ADDED)`) does the same through the deferred buffer when
`!mInOperation`.

### 3.2 `set_parent(child, newParent, keepWorld, index)`

1. Validate: `child` alive else `INVALID_ENTITY`; `newParent` null or alive; `child == newParent` ->
   `SELF_PARENT`; walk up from `newParent` through `parent::value` (at most `MAX_HIERARCHY_DEPTH` steps),
   `HIERARCHY_CYCLE` if `child` is met; `d = depth(newParent) + 1` (0 when null), `d + subtree_height(child)
   >= MAX_HIERARCHY_DEPTH` -> `HIERARCHY_TOO_DEEP`. Nothing has changed on error.
2. `keepWorld`: `childWorld = compute_world_transform(child)`, `parentWorld = compute_world_transform(newParent)`
   (fresh ancestor walks composing `to_transform()` with `math::transform::operator*`, identity for transformless
   ancestors); write `local_transform::from_transform(parentWorld.inverse() * childWorld)` through
   `world.get<local_transform>(child)` (stamps the chunk). Transformless entities ignore `keepWorld`.
3. Unlink: erase `child` from the old parent's `children` (or `mRoots`), renumber the following siblings.
4. Link: null parent -> `mScratch.remove<parent>(child)`, insert into `mRoots` at `index` (owner kept, or
   `SCENE_ID_AUTHORED`); otherwise `mScratch.add_raw/set<parent>(child, {newParent, index})`, insert into
   `newParent.children` at `min(index, size)`, renumber.
5. Depth retag (3.3) when the depth changed; `mScratch.add<transform_dirty>(child)` always; recompute
   `inactive` for the subtree when the new parent's active state differs (same walk). `world.flush(mScratch)`
   once: every touched entity moves at most once (`01_ecs.md` 3.4).
6. `++mStructureVersion`; `on_structure_changed.emit()`.

`compute_world_transform` is O(depth) and exact whether or not the pass has run this frame; the editor's
`entity.set_transform {space:"world"}` uses it too.

### 3.3 Sibling order and depth retag

`set_sibling_index(entity, index)`: erase + insert in the parent's `children` (or `mRoots`), renumber the
affected range, `++mStructureVersion`. Children are always stored sorted by `siblingIndex`, indices dense.

Retag (no recursion): push `child` on `mWalkStack`; pop, depth = parent depth + 1, `mScratch.remove_raw(e,
mIds.depth[old])` + `add_raw(e, mIds.depth[new])`, push its `children`; the flush in 3.2 step 5 applies all of
it. A row moving into a new chunk stamps all its columns (`01_ecs.md` 3.1), so retagged entities are recomputed
by the next propagation without a dirty tag of their own.

### 3.4 Transform propagation (`scene.propagate_transforms`, TRANSFORM, PARALLEL, `RUNS_WHILE_PAUSED`)

Owned by `transform_system`. Queries, built once: `mRootQuery = {with: local_transform, without: parent}`,
`mLevelQuery[d] = {with: local_transform | depth_tag<d>}` for d = 1..31. `describe()` declares `writes =
{world_transform, render_world_transform, transform_state}` and `reads = {local_transform, parent, children,
interpolated_transform, transform_dirty}`; the `children` read keeps a TRANSFORM-phase writer of the lists out of
the pass's stage (the scheduler packs stages from the declared masks alone).

Each level (the roots, then depth 1 .. `max_depth_in_use()`) runs in three steps on the system thread. *Collect*
walks the level's chunks once with `level.for_each_chunk` and keeps the rows to recompute: a whole chunk when its
`local_transform` changed since the previous run, when it carries `transform_dirty` or when it holds interpolated
poses, otherwise only its rows marked `transform_state::pending`. *Dispatch* runs the kept rows inline below
`INLINE_ROW_LIMIT` = 512 rows (or without a job system), otherwise through `jobs->parallel_for(jobCount, 1, body)`
in jobs of about `ROWS_PER_JOB` = 128 rows; each job resolves a row's parent once (consecutive siblings share the
lookup) and marks the row's children pending. The join of the dispatch is the barrier between levels. Last,
`transform_dirty` is removed on the system thread through the command buffer.

```text
run(ctx):
    runId = ++mRunCounter (never 0); since = ctx.since_version()
    alpha = mInterpolationEnabled ? ctx.frame().interpolationAlpha : 1.0f
    run_level(mRootQuery)                                                  // level 0
    for d in 1 .. scene.max_depth_in_use(): run_level(mLevelQuery[d])

run_level(level):
    level.for_each_chunk(chunk):                                           // collect, sequential
        removeDirty  = chunk.has(ids.transformDirty)
        recomputeAll = removeDirty || chunk.column_version(ids.localTransform) > since
        whole        = recomputeAll || chunk.has(ids.interpolatedTransform)
        rows = whole ? chunk.count() : count of rows with states[row].pending
        if rows == 0: stats.chunksSkipped++; continue                      // chunk untouched, no stamp
        mLevelChunks.push({chunk, rows, removeDirty, recomputeAll, whole})
    split mLevelChunks into jobs of consecutive chunks holding about ROWS_PER_JOB rows (mJobBounds)
    lock = world.lock_iteration()
    if jobs && jobCount > 1 && totalRows >= INLINE_ROW_LIMIT: jobs->parallel_for(jobCount, 1, process_chunk...)
    else: process_chunk for every kept chunk, inline
    lock.release()
    for kept chunk with removeDirty: for row: ctx.commands().remove_raw(entities[row], ids.transformDirty)

process_chunk(item):                                                       // a job thread
    for row where item.whole || states[row].pending:
        states[row].pending = false
        p = lookup_parent(parents[row].value)       // nearest ancestor with a world_transform, cached per sibling run
        parentWorld = p ? p.world : identity; parentRender = p ? p.render : identity
        recomputeWorld = item.recomputeAll || (p && p.state.lastUpdateRun == runId)
        if recomputeWorld:
            worlds[row] = parentWorld * locals[row].to_transform().to_matrix(); states[row].lastUpdateRun = runId
        self = chunk holds poses; subtree = self || (p && p.state.flags & INTERPOLATED_SUBTREE)
        hadSubtree = states[row].flags & INTERPOLATED_SUBTREE
        if self:         renders[row] = math::transform::lerp(poses[row].previous, poses[row].current, alpha).to_matrix()
        elif subtree:    renders[row] = parentRender * locals[row].to_transform().to_matrix()
        elif recomputeWorld or hadSubtree: renders[row] = worlds[row]
        states[row].flags = subtree ? INTERPOLATED_SUBTREE : NONE
        if recomputeWorld || subtree || hadSubtree != subtree: mark_children_pending(children[row])
    stats accumulate (a local tally per job range, added to relaxed atomics once per range)
```

Correctness: every ancestor of a level-d entity is at a shallower level and finished before the join; parent
reads are `const` random access (`01_ecs.md` 5.2 allows reading other entities' columns while locked and writing
disjoint rows). A job writes rows of its own chunks and the `pending` byte of their children, which sit in the
next level (no job touches them until the join; one parent per child, so no two jobs write the same byte); the
mark goes through the non-const `world::get_raw(entity, id, false)`, which stamps nothing, and the child's own
level stamps the column when it processes the row. A transformless child (S5) is in no level, so the mark goes to
its children instead (an explicit stack bounded by `MAX_HIERARCHY_DEPTH`). A clean chunk costs one sequential
scan of its 8-byte state column and no parent is looked up for a clean row; below `INLINE_ROW_LIMIT` dirty rows
the dispatch costs more than the rows (the limit is a fixed row count, not one per worker, see
`engine/src/scene/README.md`). An interpolated entity's render pose is its own world-space lerp (the
"parent-render space" R of `06_engine_loop.md` 3.2 cancels against `parentRender`); descendants follow
`parentRender` every frame through `INTERPOLATED_SUBTREE`; roots without a pose get `render == world`. Both
matrices are written in one visit (R3). Allocation-free in steady state: queries refresh only on new archetypes,
the level scratch (`mLevelChunks`, `mJobBounds`) keeps its capacity and dirty tags are gone after the first run,
so the pass allocates nothing from the third frame after a structural change. Budget: 10k entities, 8 workers,
~0.3 ms (`06_engine_loop.md` 8).

### 3.5 Dirty tracking summary

| Cause | Mechanism | Who recomputes |
|---|---|---|
| `local_transform` written (script, physics pull, editor) | chunk column version (`changed<local_transform>`) | every row of that chunk, then descendants via the stamp |
| Reparent at same depth | `transform_dirty` tag on the reparented entity | its chunk, then descendants |
| Reparent across depths / new entity / instantiate / load | archetype move stamps all columns + `transform_dirty` | chunk, then descendants |
| Parent recomputed | `transform_state::lastUpdateRun == runId`, found through the `transform_state::pending` mark the parent's visit sets | the child |
| `interpolated_transform` present on self or ancestor | `INTERPOLATED_SUBTREE` bit | render matrix only, every frame |

### 3.6 Destroy

`scene::destroy(entity)` appends to `mDestroyQueue` (duplicates ignored). `scene.destroy_pending` (FRAME_END,
EXCLUSIVE, `after("scripting.frame_end")`) pops each entity and calls `destroy_now`. `destroy_now(e)`:
unlink from parent or roots (direct, `mInOperation = true`), `world.destroy_entity(e)`, `++mStructureVersion`.
Observers (registered in the `scene` constructor, owner thread, enqueue-only):

- `observe_destroyed(e)`: for each entry of `e`'s `children` (readable: destroy fires observers before
  destructing columns) `deferred.destroy_entity(child)`. The ECS drains the deferred buffer until empty inside
  the original call, so a 10k-entity subtree dies in one `destroy_now` with no recursion.
- `observe(parent, REMOVED)`, `!mInOperation`: if `parent::value` is alive, `deferred.set<children>(p,
  copy_without(p.children, e))` (allocation only above four children); push `e` to `mRepairQueue` so roots and
  depth are fixed by 3.7 when the entity survives (raw `remove<parent>`).
- `observe(parent, ADDED)`, `!mInOperation`: push `e` to `mRepairQueue`, WARN once per frame.
- `observe(guid, ADDED/REMOVED)`: 3.8. `observe(localTransform, ADDED)`, `!mInOperation`: companions (3.1).

### 3.7 Hierarchy repair (`scene.apply_hierarchy_changes`, TRANSFORM, EXCLUSIVE, before propagation)

For each live entity in `mRepairQueue`: a `parent` with a dead value -> `remove<parent>`, make it a root
(WARN); link into its parent's `children` at `siblingIndex` (clamped, renumbered) or into `mRoots`; retag the
subtree (3.3); add `transform_dirty`; recompute `inactive`. One `mScratch` flush. An empty queue costs one
exclusive stage boundary per frame.

`scene::deferred_set_parent(command_buffer &, child, newParent, keepWorld, index)` (4.3; the bindings' R5 route
for `transform_set_parent*` inside a DOD query scope, `15_bindings.md` 3.3, R55) is the sanctioned way to reparent
through a command buffer: it records `set_raw<parent>{newParent, index}` (or `add_raw`, or `remove_raw` for a null
parent), `set<local_transform>` computed now when `keepWorld` (reads are legal while locked) and `add<transform_dirty>`,
and increments `mExpectedRepairs`. The `parent` observers decrement that counter before warning, so repairs that
originate here are silent; everything else in this section applies unchanged, and the tree is correct by the end
of the same frame's TRANSFORM phase.

### 3.8 Guid index

`guid_index` is a `core::flat_hash_map<core::uuid, ecs::entity>` reserved to 1024 and grown by the link
pass before instantiation (`reserve(size + count)`), so observers never allocate during a batch. `ADDED`
observer: `insert(value, e)`, first mapping wins on collision (WARN with both entities); `REMOVED` observer:
`erase` if the mapping points at `e`. Duplicates are prevented, not repaired: every instantiation path
validates archive guids against the index first (3.9).

### 3.9 Instantiating an archive (shared by load, prefab, duplicate, snapshot restore)

`scene_serializer::instantiate_archive(scene &, const shared_ptr<const entity_archive> &, u32 count,
instantiate_options, std::span<ecs::entity> outRoots) -> result<void>`:

1. `archive->validate()`; count entries without `parent`; prefab paths require exactly one (`PREFAB_MULTIPLE_ROOTS`).
   The shape and depth of every entry (`HIERARCHY_CYCLE`, `HIERARCHY_TOO_DEEP`) and every `layer` index (32 or
   more is `BAD_FORMAT` naming the entry, S15) are checked here, before anything exists.
2. Guids from each entry's `guid` bytes. `KEEP`: any collision with the index or inside the archive ->
   `DUPLICATE_GUID`, nothing created. `REGENERATE_ON_CONFLICT`: colliding entries get fresh guids (one WARN per
   call). `REGENERATE_ALL` (prefabs, duplicates, `count > 1`): every entity. Fresh guids are written after step
   3 through `guid_index::rebind` (the observer indexed the archive guid first).
3. `mInOperation = true`; `world.instantiate_batch(archive, outRoots, nullptr, &entities)` (`count` copies, every
   created id reported copy-major in entry order, entries without components included; `parent::value`
   remapped by the ECS; `script_component` deep-copied by its own ops, R29).
4. Link pass in archive order per copy: an entity with `parent` is appended to its parent's `children` (each
   touched vector stable-sorted by `siblingIndex`, renumbered); an archive root attaches to `options.parent` at
   `options.index` or is appended to `mRoots` with `options.owner`. Depth = parent depth + 1
   (`HIERARCHY_TOO_DEEP` destroys the created entities and fails). Into `mScratch` per entity: `depth_tag`,
   missing companions, `transform_dirty`, `inactive` under a `disabled` ancestor. One flush = one move each.
5. Root overrides (`rootName`, `rootTransform`); `mInOperation = false`.
6. Bind pass (3.10) when `options.bindAssets`; ids returned in `instantiate_report`.
7. `++mStructureVersion`, `on_structure_changed.emit()`.

Cost for 10k entities: `instantiate_batch` ~5 ms, link + one flush ~10 ms, bind ~1 ms.

### 3.10 Post-load asset bind pass (R25)

`scene_serializer::bind_asset_refs(world, entities, assets, outIds)`: for each entity,
`mask_of(e).for_each_set(id)`; for each `component_info::assetRefFieldOffsets` entry, `bytes = world.get_raw(e,
id, true)`, view the field as `asset_ref_raw { assets::asset_id id; const assets::asset_slot *slot; }` and, if
`!id.is_null()`, `slot = assets.bind_slot(id)` (main thread, never loads) and push `id` to `outIds`.
`bind_entity(world, e, assets)` is the single-entity form the editor calls after `component_from_json`. The
layout contract with `05_assets.md` 2.3 (`asset_ref<T>` = `{asset_id, const asset_slot *}`, standard layout,
24 bytes) is checked by `static_assert`s on `asset_ref<binary_asset>` and a `CONJURE_VERIFY` self-test in
`register_components` that constructs a ref and compares it with the raw view.

### 3.11 Loading and unloading scenes (`scene_manager`)

```text
load(assetId, mode)                       [main, synchronous; startup step 20]
    handle = assets.load_blocking(assetId); doc = handle.get<scene_asset>()   // dependencies terminal (05 D9)
    return apply_document(doc, mode, batch{handle})
load_file(fs, path, mode)                 [main, synchronous; editor scene.open, tests]
    doc = parse_document(json::parse(fs.read_text_file(path)), registry)
    report = apply_document(doc, mode, {}); record.batch = assets.load_batch(report.assetIds)
request_load(assetId, mode)               [main, deferred] push {id, mode, handle = assets.load(assetId)}
scene.apply_pending_loads (FRAME_BEGIN, EXCLUSIVE):
    for each pending unload: unload(sceneId)
    for each pending load in request order:
        FAILED handle -> ERROR log, on_load_failed, drop;  !batch{handle}.is_complete() -> stop
        apply_document(handle.get<scene_asset>(), mode, batch{handle}); drop
apply_document(doc, mode, batch):
    REPLACE: unload_all(); scene.settings = doc.settings; scene.name = doc.name
    record = mLoaded.emplace(scene_id, doc.name, assetId, batch); guids = REPLACE ? KEEP : REGENERATE_ON_CONFLICT
    report = instantiate_archive(scene, doc.archive, 1, {owner = record.id, guids}, roots)
    order roots per doc.roots (2.3 rules); record.roots = roots; on_loaded.emit(record.id)
unload(sceneId):
    on_before_unload.emit(sceneId, record.roots)                  // scripting runs destroy_all(true) (R29)
    for root in record.roots (reverse): scene.destroy_now(root)   // cascades through observers (3.6)
    release record (batch handles drop); assets.collect()
```

Additive loads never touch `settings` (one DEBUG log when they differ). A root reparented under another
scene's root changes owner implicitly (`root_entry::owner` exists only while the entity is a root). `unload_all`
unloads in reverse load order then `scene.clear()`s remaining authored entities (shutdown step 2, REPLACE loads).

### 3.12 Prefabs (`prefab.h`, namespace `conjure::scene::prefab`)

- `build_document(scene, root, name)`: `capture_subtree` in hierarchy order with `keepExternalReferences =
  false` (outside references become null, DEBUG log), single root enforced; `write_prefab` emits 2.4. The
  editor writes the bytes through `asset_database::create_asset(path, "conjure.scene.prefab", bytes)` and then
  calls `mark_instance(scene, root, assets.bind<prefab_asset>(id))`, which adds `prefab_instance` to the root
  and `prefab_link{guid_of(e)}` to every entity of the subtree (the document's guids at this moment).
- `instantiate` / `instantiate_many`: 3.9 with `REGENERATE_ALL`; `prefab_link` is already in the archive,
  the root gets `prefab_instance` bound to the asset if the archive root lacks one (first instantiation of a
  freshly authored prefab). Nested markers inside the archive are copied as-is.
- `instance_root_of`: nearest ancestor (inclusive) with `prefab_instance`.
- `diff(scene, root)`: requires `prefab_instance.prefab.get()` (else `PREFAB_NOT_LOADED`). Match instance
  entities to asset entries by `prefab_link::assetGuid` <-> entry `guid`; compare each non-TRANSIENT component
  through its `component_serializer::writeJson` (canonical strings) -> `MODIFIED` with component names;
  unmatched instance entities -> `ADDED`; unmatched entries -> `REMOVED`. Baked nested instances are part of
  the outer diff.
- `revert(scene, root)`: remember root guid, name, local transform, parent, index, owner; `destroy_now(root)`;
  `instantiate` at the same place with `KEEP` for the root guid (the rest regenerated) and the remembered
  name/transform.
- `apply(scene, root)`: `build_document` from the instance with `prefab_link` guids written back as entry
  `guid`s (new entities get fresh ones) and `prefab_instance` dropped from the root entry; the editor writes the
  source and reimports. `refresh_instances(scene, assetId, onlyUnmodified)` reverts every other root of that
  prefab whose `diff` is empty.
- `unpack(scene, root, recursive)`: remove `prefab_instance` from the root and `prefab_link` from the subtree,
  stopping at nested instance roots unless `recursive`.

### 3.13 `model_asset -> prefab` (`prefab_builder`, scene; converter in `engine`)

`prefab_builder` (4.6): `add_node` (parent index or `NO_PARENT`, name, `local_transform`),
`add_component_json` (resolved through `component_registry::find` + the component's `readJson`; unknown names
fail with `ecs::UNKNOWN_COMPONENT`), `add_component_raw`, `add_tag`, `build()` (guids generated, `parent` +
`siblingIndex` from insertion order, exactly one root). The engine's `renderer_subsystem`
(`engine/src/engine/subsystems/model_prefab_converter.cpp`) maps `renderer::model_node` -> node (name, TRS;
matrices decomposed with `transform::from_matrix`), mesh node -> `conjure.renderer.mesh_renderer {mesh,
materials}` by sub-asset uuid, `KHR_lights_punctual` -> `conjure.renderer.light`, cameras only on request. The
editor's `asset.import` of a glTF writes the resulting `<model>.prefab.json` next to the source.

### 3.14 Snapshot capture and restore (`scene_snapshot.h`)

`capture(const scene &)`: collect every entity in hierarchy order (iterative over `mRoots` and `children`),
`entity_archive::capture(world, entities, {keepExternalReferences = false})`, record root indices and owners,
deep-copy `settings`, name, version. No JSON; allocation only for the archive's own vectors (reserved up front).
An entity whose `layer` index is 32 or more fails the capture with `BAD_FORMAT` naming it (S15): `restore` would
refuse it only after its `clear()`, so play/stop would lose the scene. `capture_subtree` and `save_scene` run the
same check.

`restore(scene &, const scene_snapshot &)`: `scene.clear()` (destroys every root through 3.6, resets the
index; `scene_manager` records are untouched because the editor restores the same loaded set), then 3.9 with
`KEEP` guids (empty world, no collisions), restore `settings` + `name`, reorder roots and reassign
`root_entry::owner` from `rootOwners`, bind pass. Budget: capture + restore of 10k entities < 50 ms
(`04_editor.md` 3.7), guarded by `bench_scene_snapshot`.

---

## 4. Public API (`engine/include/conjure/scene/`)

| Header | Main types |
|---|---|
| `log_category.h` | `CONJURE_DECLARE_LOG_CATEGORY(LOG_SCENE);` (defined in `log_category.cpp` as `"scene"`) |
| `scene_error.h` | `scene_error`, `scene_error_name`, `CONJURE_DECLARE_ERROR_CATEGORY(scene_error, "scene", scene_error_name)` |
| `components.h` | every component of 2.1, `depth_tag<D>`, `scene_component_ids`, `MAX_HIERARCHY_DEPTH`, `APPEND_CHILD`, `SCENE_FORMAT_VERSION` |
| `hierarchy.h` | `hierarchy` (tree queries and mutation over a world; owned by `scene`) |
| `guid_index.h` | `guid_index` |
| `scene.h` | `scene`, `scene_id`, `root_entry`, `guid_policy`, `instantiate_options`, `instantiate_report` |
| `transform_system.h` | `transform_system`, `propagation_stats`, `compute_world_transform` |
| `scene_serializer.h` | `scene_document`, `scene_asset`, `prefab_document`, `scene_serializer` (static API) |
| `scene_snapshot.h` | `scene_snapshot`, `capture`, `restore` |
| `prefab.h` | `prefab_asset`, `prefab_diff`, `entity_diff`, namespace `prefab` functions, `prefab_builder` |
| `scene_manager.h` | `scene_manager`, `load_mode`, `load_request_id`, `loaded_scene_info`, `layer_table` |
| `register.h` | `register_components`, `register_asset_types`, `register_systems` |

Private (`engine/src/scene/`): one `.cpp` per header plus `scene_observers.cpp`, `scene_link_pass.cpp`,
`transform_propagation.cpp`, `scene_json.{h,cpp}` (header read/write), `prefab_importer.{h,cpp}`,
`scene_loader.{h,cpp}` (asset loaders), `asset_ref_raw.h`, `log_category.cpp`, `README.md`.

### 4.1 `scene_error.h`

`enum class scene_error : u16` (one enumerator per line in the header) with `NONE = 0`, `INVALID_ENTITY`
(dead, null or foreign), `SELF_PARENT`, `HIERARCHY_CYCLE`, `HIERARCHY_TOO_DEEP`, `INVALID_SIBLING_INDEX`,
`NO_TRANSFORM`, `DUPLICATE_GUID`, `GUID_NOT_FOUND`, `BAD_FORMAT` (header `format` missing or wrong, or a `layer`
index of 32 or more), `UNSUPPORTED_VERSION`, `PREFAB_MULTIPLE_ROOTS`, `PREFAB_NOT_LOADED`, `NOT_A_PREFAB_INSTANCE`,
`SCENE_NOT_LOADED`, `LOAD_FAILED` (asset FAILED while a request waited), `BUILDER_INVALID`;
`[[nodiscard]] std::string_view scene_error_name(scene_error) noexcept;` and, at namespace scope,
`CONJURE_DECLARE_ERROR_CATEGORY(conjure::scene::scene_error, "scene", conjure::scene::scene_error_name);`.

Errors are built with `core::make_error(scene_error::X, "detail {}", ...)`; ECS and asset errors propagate
unchanged (`ENTITY_NOT_ALIVE`, `ARCHIVE_CORRUPT`, `SERIALIZATION_FAILED`, `NOT_FOUND`, ...).

### 4.2 `hierarchy.h` and `guid_index.h`

```cpp
namespace conjure::scene {

    class hierarchy {              // non-owning: ecs::world &, scene_component_ids; used through scene
        public:
            [[nodiscard]] ecs::entity parent_of(ecs::entity target) const noexcept;        // NULL for roots/dead
            [[nodiscard]] std::span<const ecs::entity> children_of(ecs::entity target) const noexcept;
            [[nodiscard]] u32 sibling_index(ecs::entity target) const noexcept;
            [[nodiscard]] u8 depth_of(ecs::entity target) const noexcept;                 // 0 for roots
            [[nodiscard]] bool is_ancestor_of(ecs::entity ancestor, ecs::entity target) const noexcept;
            [[nodiscard]] ecs::entity root_of(ecs::entity target) const noexcept;
            [[nodiscard]] u8 subtree_height(ecs::entity target) const noexcept;
            // Depth-first in sibling order, iterative (explicit stack), never recursive.
            template <typename Callable> void for_each_in_subtree(ecs::entity target, Callable &&callable) const;
            void collect_subtree(ecs::entity target, std::vector<ecs::entity> &out) const;
    };

    class guid_index {             // flat_hash_map<core::uuid, ecs::entity>; main thread; ctor/dtor in .cpp
        public:
            [[nodiscard]] ecs::entity find(const core::uuid &id) const noexcept;
            [[nodiscard]] bool contains(const core::uuid &id) const noexcept;
            [[nodiscard]] u32 size() const noexcept;
            void reserve(u32 count);
            // Used by scene's observers and the link pass only: insert returns false on collision (first wins).
            [[nodiscard]] bool insert(const core::uuid &id, ecs::entity target);
            void erase(const core::uuid &id, ecs::entity expected) noexcept;
            void rebind(ecs::entity target, const core::uuid &oldId, const core::uuid &newId);
            void clear() noexcept;
    };
}
```

### 4.3 `scene.h`

```cpp
namespace conjure::scene {

    enum class guid_policy : u8 { KEEP, REGENERATE_ON_CONFLICT, REGENERATE_ALL };

    struct instantiate_options {
        ecs::entity parent = ecs::NULL_ENTITY;
        u32 index = APPEND_CHILD;
        scene_id owner = {};                          // {} = inherit from parent / SCENE_ID_AUTHORED
        guid_policy guids = guid_policy::REGENERATE_ALL;
        std::optional<core::string_id> rootName = {};
        std::optional<local_transform> rootTransform = {};
        bool bindAssets = true;
    };

    struct instantiate_report {
        u32 entityCount = 0;
        u32 unknownComponents = 0;
        std::vector<assets::asset_id> assetIds = {};   // every bound reference, deduplicated
    };

    class scene {
        public:
            scene(ecs::world &world, assets::asset_manager *assets);   // assets may be null in pure ECS tests
            ~scene();                                                   // unobserves; does not clear the world
        public:
            [[nodiscard]] ecs::world &world() noexcept;
            [[nodiscard]] const scene_component_ids &ids() const noexcept;
            [[nodiscard]] const hierarchy &tree() const noexcept;
            [[nodiscard]] const guid_index &guids() const noexcept;
            [[nodiscard]] u64 structure_version() const noexcept;
            [[nodiscard]] std::string_view name() const noexcept;      void set_name(std::string_view value);
            [[nodiscard]] core::json::document &settings() noexcept;   void set_settings(core::json::document value);

            // Entities (owner thread, world unlocked).
            [[nodiscard]] core::result<ecs::entity> create_entity(core::string_id name, ecs::entity parent = {}, u32 index = APPEND_CHILD);
            // templateEntity == NULL_ENTITY creates plain entities exactly like create_entity (entity_create_many, R55).
            [[nodiscard]] core::result<instantiate_report> create_entities(u32 count, ecs::entity templateEntity, ecs::entity parent, std::span<ecs::entity> out);
            // capture_subtree + instantiate_archive(REGENERATE_ALL) under `parent` (default: the root's own parent) at `index`;
            // scripts deep-copied by script_component's ops (R29). Backs entity_duplicate (R55).
            [[nodiscard]] core::result<ecs::entity> duplicate_subtree(ecs::entity root, ecs::entity parent = ecs::NULL_ENTITY, u32 index = APPEND_CHILD);
            void destroy(ecs::entity target);                                   // deferred to scene.destroy_pending
            [[nodiscard]] core::result<void> destroy_now(ecs::entity target);   // immediate cascade
            void clear();                                                       // every entity, roots, index
            [[nodiscard]] bool contains(ecs::entity target) const noexcept;    // alive in this world

            // Hierarchy.
            [[nodiscard]] core::result<void> set_parent(ecs::entity child, ecs::entity newParent, bool keepWorld = true, u32 index = APPEND_CHILD);
            // Same contract through a command buffer while the world is locked (3.7, R55); applied at the flush,
            // children/depth repaired by scene.apply_hierarchy_changes without a WARN.
            [[nodiscard]] core::result<void> deferred_set_parent(ecs::command_buffer &commands, ecs::entity child, ecs::entity newParent, bool keepWorld = true, u32 index = APPEND_CHILD);
            [[nodiscard]] core::result<void> set_sibling_index(ecs::entity target, u32 index);
            [[nodiscard]] std::span<const root_entry> roots() const noexcept;
            [[nodiscard]] u8 max_depth_in_use() const noexcept;
            [[nodiscard]] ecs::entity find_path(std::string_view path) const;   // "Root/Child/Leaf" by name

            // Transforms. world_transform_of is a fresh ancestor walk; world_bounds = local_bounds * world matrix.
            [[nodiscard]] core::result<void> add_transform(ecs::entity target, const local_transform &value = {});
            [[nodiscard]] core::result<void> set_local_transform(ecs::entity target, const local_transform &value);
            // Forwards local_transform::from_transform, so local_transform_of feeds it directly (S16); `{}` must name its type.
            [[nodiscard]] core::result<void> set_local_transform(ecs::entity target, const math::transform &value);
            [[nodiscard]] core::result<void> set_world_transform(ecs::entity target, const math::transform &value);
            [[nodiscard]] std::optional<math::transform> local_transform_of(ecs::entity target) const noexcept;
            [[nodiscard]] std::optional<math::transform> world_transform_of(ecs::entity target) const;
            [[nodiscard]] std::optional<math::aabb> world_bounds(ecs::entity target) const;
            [[nodiscard]] std::optional<math::aabb> subtree_bounds(ecs::entity target) const;

            // Identity and attributes. set_enabled toggles `disabled` and the subtree's `inactive`.
            [[nodiscard]] core::result<core::uuid> ensure_guid(ecs::entity target);
            [[nodiscard]] std::optional<core::uuid> guid_of(ecs::entity target) const noexcept;
            [[nodiscard]] ecs::entity find_guid(const core::uuid &id) const noexcept;
            [[nodiscard]] core::string_id name_of(ecs::entity target) const noexcept;
            [[nodiscard]] core::result<void> set_name(ecs::entity target, core::string_id value);   // also set_tag, set_layer(u8)
            [[nodiscard]] core::result<void> set_enabled(ecs::entity target, bool enabled);
            [[nodiscard]] bool is_enabled(ecs::entity target) const noexcept;                  // self
            [[nodiscard]] bool is_active(ecs::entity target) const noexcept;                   // self and ancestors
            void find_by_name(core::string_id value, std::vector<ecs::entity> &out) const;    // also find_by_tag, find_by_layer

            [[nodiscard]] core::event<> &on_structure_changed() noexcept;   // main thread
            [[nodiscard]] core::event<> &on_settings_changed() noexcept;

            // Used by the systems and serializer (owner thread).
            void apply_hierarchy_changes();          // 3.7
            void destroy_pending();                  // 3.6
            [[nodiscard]] core::result<instantiate_report> instantiate_archive(
              const std::shared_ptr<const ecs::entity_archive> &archive,
              u32 count,
              const instantiate_options &options,
              std::span<ecs::entity> outRoots
            );
            [[nodiscard]] core::result<void> validate() const;   // section 6
    };
}
```

### 4.4 `transform_system.h`

```cpp
namespace conjure::scene {

    struct propagation_stats { u32 entitiesVisited = 0; u32 worldRecomputed = 0; u32 renderRecomputed = 0;
                               u32 chunksVisited = 0; u32 chunksSkipped = 0; u8 levels = 0; };

    class transform_system {       // explicit transform_system(scene &); ~transform_system()
        public:
            void run(ecs::system_context &context);                 // body of scene.propagate_transforms
            void set_interpolation_enabled(bool enabled) noexcept; // engine_time.interpolatePhysics
            [[nodiscard]] bool interpolation_enabled() const noexcept;
            [[nodiscard]] const propagation_stats &last_stats() const noexcept;
            [[nodiscard]] ecs::system_desc describe();            // name, phase, flags, reads/writes
    };

    /// <summary>Exact world transform from a fresh ancestor walk; identity for transformless ancestors.</summary>
    [[nodiscard]] math::transform compute_world_transform(const scene &target, ecs::entity entity);
}
```

### 4.5 `scene_serializer.h` and `scene_snapshot.h`

```cpp
namespace conjure::scene {

    struct scene_document { /* 2.4 */ };   using scene_asset = scene_document;
    struct prefab_document { std::string name; std::shared_ptr<const ecs::entity_archive> archive; };
    CONJURE_DECLARE_ASSET_TYPE(conjure::scene::scene_asset, "conjure.scene.scene");

    class scene_serializer {       // static API; no state; every function [[nodiscard]] core::result<...>
        public:
            static result<void> save_scene(const scene &source, core::json::writer &out);
            static result<void> save_scene_file(const scene &source, core::vfs::file_system &fs, std::string_view path);
            static result<scene_document> parse_document(const core::json::value &in, const ecs::component_registry &registry);
            static result<prefab_document> parse_prefab(const core::json::value &in, const ecs::component_registry &registry);
            static result<void> write_prefab(const prefab_document &document, core::json::writer &out);
            static result<instantiate_report> load_scene(scene &target, const scene_document &document, scene_id owner, guid_policy guids);
            static result<instantiate_report> load_scene_file(scene &target, core::vfs::file_system &fs, std::string_view path, scene_id owner, guid_policy guids);
            static result<void> save_subtree(const scene &source, ecs::entity root, core::json::writer &out);   // prefab format
            static result<ecs::entity> instantiate_subtree(scene &target, const core::json::value &in, ecs::entity parent, bool keepGuids, u32 index = APPEND_CHILD);
            static result<ecs::entity_archive> capture_subtree(const scene &source, ecs::entity root, ecs::archive_options options = {});
            static void bind_asset_refs(ecs::world &target, std::span<const ecs::entity> entities, assets::asset_manager &assets, std::vector<assets::asset_id> *outIds);
            static void bind_entity(ecs::world &target, ecs::entity entity, assets::asset_manager &assets);
    };

    [[nodiscard]] core::result<scene_snapshot> capture(const scene &source);
    [[nodiscard]] core::result<void> restore(scene &target, const scene_snapshot &snapshot);
}
```

### 4.6 `prefab.h`

```cpp
namespace conjure::scene {
    struct prefab_asset { /* 2.4 */ };   CONJURE_DECLARE_ASSET_TYPE(conjure::scene::prefab_asset, "conjure.scene.prefab");
    enum class entity_diff_kind : u8 { ADDED, REMOVED, MODIFIED };
    struct entity_diff { core::uuid assetGuid; ecs::entity instanceEntity; entity_diff_kind kind; std::vector<core::string_id> components; };
    struct prefab_diff { std::vector<entity_diff> entries; [[nodiscard]] bool empty() const noexcept; };

    namespace prefab {
        [[nodiscard]] core::result<prefab_document> build_document(const scene &source, ecs::entity root, std::string_view name);
        [[nodiscard]] core::result<void> mark_instance(scene &target, ecs::entity root, assets::asset_ref<prefab_asset> asset);
        [[nodiscard]] core::result<ecs::entity> instantiate(scene &target, const prefab_asset &asset, const instantiate_options &options = {});
        [[nodiscard]] core::result<void> instantiate_many(scene &target, const prefab_asset &asset, u32 count, const instantiate_options &options, std::span<ecs::entity> outRoots);
        [[nodiscard]] ecs::entity instance_root_of(const scene &source, ecs::entity entity) noexcept;
        [[nodiscard]] core::result<prefab_diff> diff(const scene &source, ecs::entity root);
        [[nodiscard]] core::result<void> revert(scene &target, ecs::entity root);
        [[nodiscard]] core::result<prefab_document> apply(const scene &source, ecs::entity root);
        [[nodiscard]] u32 refresh_instances(scene &target, const assets::asset_id &prefab, bool onlyUnmodified = true);
        [[nodiscard]] core::result<void> unpack(scene &target, ecs::entity root, bool recursive = false);
    }

    class prefab_builder {         // 3.13; explicit prefab_builder(const ecs::component_registry &)
        public:
            static constexpr u32 NO_PARENT = 0xFFFFFFFFu;
            [[nodiscard]] u32 add_node(u32 parentIndex, std::string_view name, const local_transform &transform = {});
            [[nodiscard]] core::result<void> add_component_json(u32 node, core::string_id component, const core::json::value &value);
            [[nodiscard]] core::result<void> add_component_raw(u32 node, ecs::component_id component, std::span<const std::byte> bytes);
            [[nodiscard]] core::result<void> add_tag(u32 node, ecs::component_id component);
            [[nodiscard]] core::result<prefab_document> build(std::string_view name) const;
    };
}
```

### 4.7 `scene_manager.h` and `register.h`

```cpp
namespace conjure::scene {

    enum class load_mode : u8 { REPLACE, ADDITIVE };
    struct load_request_id { u32 value = 0; };
    struct loaded_scene_info { scene_id id; assets::asset_id asset; std::string name; std::string path; std::vector<ecs::entity> roots; };

    class scene_manager {          // scene_manager(ecs::world &, assets::asset_manager &); ~scene_manager()
        public:
            [[nodiscard]] scene &active() noexcept;                       // the one scene over the world
            [[nodiscard]] layer_table &layers() noexcept;
            [[nodiscard]] core::result<scene_id> load(const assets::asset_id &id, load_mode mode = load_mode::REPLACE);
            [[nodiscard]] core::result<scene_id> load_file(core::vfs::file_system &fs, std::string_view path, load_mode mode = load_mode::REPLACE);
            [[nodiscard]] load_request_id request_load(const assets::asset_id &id, load_mode mode = load_mode::ADDITIVE);
            void request_unload(scene_id id);
            [[nodiscard]] core::result<void> unload(scene_id id);
            void unload_all();
            void apply_pending_loads();                                   // body of scene.apply_pending_loads
            [[nodiscard]] bool is_loaded(scene_id id) const noexcept;    // also find_loaded(asset_id), loaded_scenes(), pending_count()
            [[nodiscard]] bool is_load_pending(const assets::asset_id &id) const noexcept;   // requested, not yet applied (scene_is_load_pending, R55)
            [[nodiscard]] core::event<scene_id, std::span<const ecs::entity>> &on_before_unload() noexcept;
            [[nodiscard]] core::event<scene_id> &on_loaded() noexcept;
            [[nodiscard]] core::event<load_request_id, const core::error &> &on_load_failed() noexcept;
    };

    [[nodiscard]] core::result<void> register_components(ecs::component_registry &registry);
    [[nodiscard]] core::result<void> register_asset_types(
      assets::asset_type_registry &types,
      assets::importer_registry &importers,
      const ecs::component_registry &registry
    );
    [[nodiscard]] core::result<void> register_systems(ecs::scheduler &scheduler, scene_manager &scenes);
}
```

`register_systems` adds, in order (four systems; `ARCHITECTURE.md` 3 and `06_engine_loop.md` 3.1/8 list the same
four, R46): `scene.apply_pending_loads` (FRAME_BEGIN, EXCLUSIVE | RUNS_WHILE_PAUSED),
`scene.apply_hierarchy_changes` (TRANSFORM, EXCLUSIVE | RUNS_WHILE_PAUSED, `before("scene.propagate_transforms")`),
`scene.propagate_transforms` (TRANSFORM, PARALLEL, RUNS_WHILE_PAUSED, masks from `transform_system::describe()`),
`scene.destroy_pending` (FRAME_END, EXCLUSIVE | RUNS_WHILE_PAUSED, `after("scripting.frame_end")`). The
`transform_system` is owned by the `scene_manager`. `register_asset_types` registers the two asset types
(version 1, no flags; loaders hold `const component_registry &`, run `parse_document` / `parse_prefab` on the
worker since registry lookups are thread-safe, trivial `finalise`) and the importers `conjure.scene.scene_importer`
/ `conjure.scene.prefab_importer` (`.scene.json` / `.prefab.json`, version 1): parse, validate the header,
`read_archive_json`, collect every non-null `asset_ref` uuid as a dependency (sorted, unique), re-emit canonical
JSON as the payload with unknown components kept, warn per unresolved component name.

---

## 5. Threading and ownership

1. `scene`, `scene_manager`, `guid_index`, prefab functions and the serializer are **main/owner-thread only**
   (`world::owner_thread()`); Debug asserts `core::jobs::is_main_thread()`, otherwise the ECS `WRONG_THREAD`
   error applies. Events fire on the main thread.
2. `scene.propagate_transforms` is the only parallel code: jobs write `world_transform`,
   `render_world_transform`, `transform_state` of their own rows and read parents' columns; level joins order
   parent-before-child; stats are relaxed atomics; command buffers are per grain (scheduler-owned).
3. Asset loaders and importers run on workers and touch only their inputs, `component_registry` lookups and
   the archive they build; payloads are immutable after publish and `instantiate` copies from the archive.
4. Observers run synchronously inside ECS structural calls on the owner thread and only enqueue into the
   deferred buffer or push into scene-side queues.
5. Ownership: `engine::scene_subsystem` owns `scene_manager`; `scene_manager` owns `scene`,
   `transform_system`, `layer_table`, loaded-scene records and their `asset_load_batch`es; `scene` owns its
   queues, scratch buffer, index and observer registrations (unregistered in its destructor; `scene_manager`
   dies before `world`, which dies before `component_registry`). Archives are `std::shared_ptr<const
   entity_archive>` shared by assets, command buffers and snapshots. No raw owning pointers.

---

## 6. Failure handling

| Situation | Behaviour |
|---|---|
| Dead/null entity to query methods (`parent_of`, `children_of`, `name_of`, `guid_of`, `is_active`) | NULL / empty span / empty optional / false; never asserts |
| Dead entity to mutators | `INVALID_ENTITY` (or the ECS `ENTITY_NOT_ALIVE` when it occurs inside the ECS) |
| `set_parent` to self / descendant / beyond depth 31 | `SELF_PARENT` / `HIERARCHY_CYCLE` / `HIERARCHY_TOO_DEEP`; nothing changes |
| `set_sibling_index` out of range / `keepWorld` on a transformless entity | clamped to last / ignored; `INVALID_SIBLING_INDEX` only when the target has no siblings list (dead parent) |
| Raw `add/remove<parent>` outside the scene API | repaired at `scene.apply_hierarchy_changes`, WARN once per frame |
| Guid collision at instantiate under `KEEP` / from a raw `add<guid>` | `DUPLICATE_GUID` naming the first collision, nothing created / first mapping kept, WARN |
| Scene header wrong / newer | `BAD_FORMAT` / `UNSUPPORTED_VERSION` before any entity exists |
| `layer` index of 32 or more on an entity (a raw write such as the editor's `component.set`) or in an archive (a file or prefab) | a capture (`capture`, `capture_subtree`, `save_scene`) refuses it first with `BAD_FORMAT` naming the entity, so no snapshot, undo payload, duplicate or file carries it; instantiation refuses an archive entry with `BAD_FORMAT` naming the entry before any entity exists, and a REPLACE load and `prefab::revert` check it before they remove the current content; `restore` of a snapshot that carries one anyway (built by hand) has cleared the scene first and leaves it empty (3.14 contract) (S15) |
| Unknown component name in a file | preserved via `unresolved_components`; one WARN per load with the count (ECS logs INFO per name) |
| Malformed component JSON | ECS `SERIALIZATION_FAILED` with path; load fails whole, nothing created |
| `asset_ref` uuid unknown to the manager | slot bound anyway (binding never fails, `05_assets.md` 3.2); load reports FAILED later and `get()` returns null |
| Deferred load whose scene asset FAILED / `request_unload` of an unknown id | request dropped, ERROR log, `on_load_failed` / ignored, DEBUG log |
| Prefab archive with several roots; `diff`/`revert` with unloaded asset; `unpack` on a non-instance | `PREFAB_MULTIPLE_ROOTS`; `PREFAB_NOT_LOADED`; `NOT_A_PREFAB_INSTANCE` |
| `restore` failure mid-way | world cleared, error returned, scene left empty (editor shows an empty scene, never crashes; `04_editor.md` 3.7) |
| `prefab_builder` without root or unknown node | `BUILDER_INVALID` |

Every assert path also returns an error (Distribution stays defined). `scene::validate()` (Debug tool, also
used by every test fixture): every `parent::value` is alive and lists the entity in its `children` at
`siblingIndex`; roots have no `parent` and appear exactly once in `mRoots`; depth tags equal computed depth;
`inactive` equals "some ancestor disabled"; `guid_index` is a bijection with live `guid` components.

---

## 7. Headless and null behaviour

The scene module has no backend. It depends on a `job_system *` only through the scheduler (`nullptr` or 0
workers = inline, identical semantics) and on `asset_manager *` only for binding and residency: a `scene`
built with `assets == nullptr` skips the bind pass. File tests use `core::vfs::memory_file_system` and
`assets::memory_asset_source`. `tests/scene/scene_test_fixture.h` provides a registry with the scene components
plus `test_asset_holder { asset_ref<binary_asset> }` and `test_entity_holder { ecs::entity other; }`, a world, a
`scene`, a scheduler with the scene systems, `step(frames)` (runs TRANSFORM and FRAME_END), `for_each_job_mode`
from the ECS fixture, and `REQUIRE(scene.validate())` at the end of every case.

---

## 8. Integration points

**Needs from `core` (10_core.md):** `result`/`error` + `CONJURE_DECLARE_ERROR_CATEGORY`; `uuid`; `string_id`
interning in every configuration; `small_vector`, `flat_hash_map`; `math::transform` (`operator*`, `inverse`,
`lerp`, `to_matrix`, `from_matrix`), `aabb::transformed`; `json::writer::write_raw_json`,
`json::document::to_canonical_string`, `json::value::to<uuid>`; `vfs::file_system::read_text_file` /
`write_file_atomic`; `core::event`; `CONJURE_REFLECT_STRUCT` with `readOnly`; `reflect` kinds `ENTITY`,
`ASSET_REF`, `STRING_ID`, `UUID`, `QUAT`.

**Needs from `ecs` (01_ecs.md):** `CONJURE_COMPONENT`, `register_component<T>(flags)` with TRANSIENT and
EDITOR_HIDDEN, generated `component_ops` for `children`, `component_info::assetRefFieldOffsets`;
`world::create_entity(mask)`, `instantiate_batch`, `get`/`get_raw(forWrite)`, `mask_of`,
`observe`/`observe_destroyed` with the deferred buffer draining until empty, `command_buffer` with
one-move-per-entity flush; `system_context::jobs`, `query::for_each_chunk`, `world::lock_iteration`,
`chunk_view::column_version/has/column_data`,
`since_version()`, `frame().interpolationAlpha`; `entity_archive::capture/validate`, `write_archive_json/
read_archive_json`, `unresolved_components`; `EXCLUSIVE | RUNS_WHILE_PAUSED`, `after/before` labels.

**Needs from `assets` (05_assets.md):** `asset_ref<T>` layout `{asset_id, const asset_slot *}` (standard
layout, 24 bytes), `asset_manager::bind_slot`, `load`, `load_blocking`, `load_batch` + `is_complete`,
`collect`, `asset_handle::get<T>`; dependency auto-loading so a LOADED scene handle implies resident
dependencies; `asset_type_registry::register_type`, `importer_registry::register_importer`,
`CONJURE_DECLARE_ASSET_TYPE`, `typed_asset_loader`, `import_context::settings/source_bytes/add_warning`.

**Offers to `engine` (06_engine_loop.md 3.1, 3.4, 8):** `scene_manager(world, assets)`, `register_components`
and `register_asset_types` at startup step 10 (before `scan()` at 17, R33), `register_systems` (step 18),
synchronous `load` at step 20, `unload_all` at shutdown step 2, `set_interpolation_enabled(config.
interpolatePhysics)`, `layer_table` from `project.json`, `propagation_stats` for `frame_stats`. The
`renderer_subsystem` sets `local_bounds` from mesh bounds, reads `render_world_transform` during EXTRACT and
implements `model_asset -> prefab` over `prefab_builder` (3.13). `physics` reads `local_transform`/`parent` on
push (its own ancestor walk, `14_physics.md` 3.3), adds and removes `interpolated_transform` through the deferred
buffer (R47) and writes `interpolated_transform` + `local_transform` on pull; scene never adds that component.

**Offers to `scripting` (03_scripting.md 8) and `bindings`:** stable 40-byte `local_transform` (layout hash
checked by the C# mirror); `set_parent`, `parent_of`, `children_of`, `find_path`, `find_by_*`, `set_enabled`,
`destroy` (deferred, matches `OnDestroy` batching), `duplicate_subtree`, `create_entities` with a null template,
`deferred_set_parent` for the locked-scope route (R5, R55), `prefab::instantiate(_many)` (parent + `rootTransform`
in `instantiate_options`), `scene_manager::request_load/unload/is_loaded/is_load_pending/loaded_scenes` for the
`prefab_*` / `scene_*` entries (paths resolve to asset ids through `asset_manager::find`, `loaded_scene_info::path`
the other way); the `prefab_instance { asset_ref<prefab_asset> prefab }` layout (24 bytes) is what the C#
`PrefabInstance` mirror carries; `on_before_unload` so `destroy_all(true)` runs before entities die (R29);
`entity_remap` reaches `script_component` through the ECS.

**Offers to `editor` (04_editor.md 4.4, 8):** `find_guid` + `find_path` for `entity` arguments;
`structure_version()` (`scene.info`); `set_parent(keepWorld, index)`; `create_entities` (`entity.create_many`);
`save_scene_file`/`load_scene_file` (`scene.save/open`); `save_subtree`/`instantiate_subtree` (duplicate,
clipboard) and `capture_subtree` with `keepExternalReferences = true` (undo payloads); `capture`/`restore`
(`play.start/stop`); `world_bounds`/`subtree_bounds` (`entity.bounds`, picking); the `prefab` functions
(`prefab.*`); `bind_entity` after `component_from_json`; `settings()` (`scene.set_settings`);
`on_structure_changed` for hierarchy panel invalidation.

**Offers to `modules`:** plugin components round-trip through `unresolved_components` while unloaded.

**Build:** `conjure_add_module(scene SOURCES ... PUBLIC_DEPS conjure_ecs conjure_assets)`;
`conjure_add_test_module(scene SOURCES ... DEPS conjure_test_support)`; `tests/benchmarks/bench_scene.cpp`.

---

## 9. Implementation plan

Each step lands with its tests green and `scene::validate()` passing in every fixture.

1. **Skeleton.** `CMakeLists.txt`, `log_category.{h,cpp}`, `scene_error.{h,cpp}`, `components.h` (all
   types, `depth_tag` names, `scene_component_ids::resolve`), `register_components`, `asset_ref_raw.h` +
   layout self-check. Tests: `test_scene_error.cpp`, `test_components.cpp`.
2. **Hierarchy and scene core.** `hierarchy`, `guid_index`, `scene`, `scene_observers.cpp`: create/destroy,
   roots, `set_parent` (cycle rejection, `keepWorld`), sibling order, depth retag, enabled/inactive,
   observers, repair queue, `validate`. Tests: `test_hierarchy`, `test_guid_index`, `test_scene`,
   `test_name_tag_layer`.
3. **Transform propagation.** `transform_system`, `transform_propagation.cpp`, `compute_world_transform`,
   `register_systems` (without loads). Tests: `test_transform_system`, `test_transform_propagation`.
4. **Serialisation.** `scene_json`, `scene_serializer`, `scene_link_pass.cpp` (`instantiate_archive`), bind
   pass, `save_subtree`/`instantiate_subtree`. Tests: `test_scene_serializer`, `test_scene_serialisation`.
5. **Snapshot.** `scene_snapshot`. Test: `test_scene_snapshot`; `bench_scene_snapshot`.
6. **Prefabs.** `prefab`, `prefab_builder`, `prefab_importer`, `scene_loader`, `register_asset_types`. Test:
   `test_prefab`.
7. **Scene manager.** `scene_manager`, `layer_table`, pending loads, additive, unload, `on_before_unload`,
   full `register_systems`. Tests: `test_scene_manager`, `test_register`.
8. **Finish.** `README.md`, `bench_scene.cpp`, style review, sanitizer run, `conjure_header_check(scene)`;
   hand-off notes for `engine` (`model_prefab_converter`, `scene_subsystem`) and the integration scenarios of section 10.

---

## 10. Tests (`tests/scene/`, target `conjure_scene_tests`, all headless)

One file per public header plus the topic files required by `08_testing.md` 5.2 (R16). Every case ends with
`REQUIRE(scene.validate())` and `REQUIRE(world.validate())`; files that run systems use `for_each_job_mode`.

- `test_scene_error.cpp`: `every code has a name`, `category name is scene`, `make_error formats detail`.
- `test_components.cpp`: `local_transform matches math transform layout`, `register_components assigns all ids and is idempotent`, `depth tags are transient hidden tags`, `children is non trivial and transient`, `asset_ref_raw layout matches asset_ref`.
- `test_hierarchy.cpp`: `set_parent links and unlinks`, `cycle rejected and nothing changes`, `self parent rejected`, `depth 31 accepted depth 32 is HIERARCHY_TOO_DEEP`, `keepWorld preserves world transform within epsilon`, `keepWorld false keeps local`, `sibling index insert renumbers`, `set_sibling_index clamps`, `root order after reparent to null`, `subtree iteration is depth first sibling order without recursion (depth 31)`, `transformless ancestor contributes identity`.
- `test_guid_index.cpp`: `insert find erase`, `collision keeps first`, `rebind`, `observer indexes on add and removes on destroy`, `reserve prevents allocation during batch`.
- `test_scene.cpp`: `create_entity has guid name transform companions and dirty`, `create under parent gets depth tag`, `destroy is deferred until destroy_pending`, `destroy_now cascades 10k subtree without recursion`, `destroy unlinks from alive parent`, `raw remove parent repaired same frame with one WARN`, `raw add parent repaired`, `set_enabled propagates inactive`, `reparent under disabled parent sets inactive`, `find_path`, `find_by_name tag layer`, `world_bounds and subtree_bounds`, `structure_version increments once per operation`, `clear empties roots and index`, `create_entities 100 from template`, `set_local_transform takes the math::transform local_transform_of returns`.
- `test_name_tag_layer.cpp`: `names survive round trip as strings`, `tag query`, `layer index 31 max`, `layer_table lookup`, `an archive layer of 32 or more fails before anything is created` (file, snapshot, prefab), `a capture refuses a layer of 32 or more, naming the entity` (snapshot, save, subtree, prefab document, duplicate).
- `test_transform_system.cpp`: `describe declares reads and writes`, `stats count visited and skipped`, `interpolation disabled uses current pose`.
- `test_transform_propagation.cpp`: `depth 8 chain matches glm product` (random TRS per level, compare `world_transform` with `glm` matrix products, epsilon 1e-4), `moving root updates leaf only through stamps`, `unchanged chunks are skipped`, `dirty propagation touches only the subtree` (10k entities in two trees; move one root; `worldRecomputed == subtree size`), `10k entities allocation-free after frame 2` (`CONJURE_CHECK_NO_ALLOCATIONS` around frames 3..10, both job modes), `whole TRANSFORM phase through the scheduler allocation-free after frame 2` (the same gate around `scheduler::run_phase(TRANSFORM)`, so the scheduler's per-stage bookkeeping and the command buffer flushes are inside it; the ECS-level gate with job closures is `test_scheduler.cpp`'s `a warmed-up phase allocates nothing of its own`), `interpolated entity renders lerp at alpha half`, `child of interpolated parent follows render parent`, `root without pose render equals world`, `pose removed restores render equals world`, `reparent same depth sets dirty and recomputes`, `inline and parallel give identical matrices`.
- `test_scene_serializer.cpp`: `header keys in order`, `bad format and newer version rejected before creating entities`, `roots order from header`, `missing root listed appended with WARN`, `parent local index remapped`, `asset_ref written as uuid and bound after load`, `bind_entity rebinds edited uuid`, `save_subtree instantiate_subtree duplicate regenerates guids`, `keepGuids true collision is DUPLICATE_GUID`, `unknown component preserved verbatim with one WARN`, `script-like custom serializer component round trips` (test component with custom hooks).
- `test_scene_serialisation.cpp`: `save load save byte identical` (programmatic 500-entity scene with every scene component, nested hierarchy, asset refs, an unknown component), `roundtrip.scene.json fixture byte identical`, `regression - code sweep F606 a scene file in component id order saves in name order`, `load into additive scene keeps existing entities`, `empty scene round trip`.
- `test_scene_snapshot.cpp`: `capture restore preserves guids names hierarchy transforms`, `restore after mutation reverts`, `10k capture plus restore under 50 ms` (Development only; `[perf]` label, skipped in Debug), `restore failure leaves empty scene`, `roots owners preserved`.
- `test_prefab.cpp`: `build_document single root`, `multiple roots rejected`, `mark_instance adds links`, `instantiate regenerates guids keeps links`, `instantiate_many 100 lands under parent in order`, `root transform and name overrides`, `nested prefab baked and inner marker preserved`, `diff reports modified added removed`, `revert restores asset state keeps root guid`, `revert to an asset with a layer of 32 or more keeps the instance`, `apply writes links as guids`, `refresh_instances reverts only unmodified`, `unpack removes markers stops at nested`, `prefab_builder builds hierarchy and components by name`, `importer extracts asset_ref dependencies and canonicalises`, `loader parses on worker`.
- `test_scene_manager.cpp`: `load replace sets settings and roots`, `additive load keeps existing entities`, `additive guid conflict regenerates with one WARN`, `request_load applies at apply_pending_loads when complete`, `requests apply in order`, `failed asset drops request and fires on_load_failed`, `unload destroys only that scene`, `on_before_unload fires before entities die with roots span` (handler asserts every root alive and records order: before_unload, then destroyed observer, then collect), `unload_all reverse order then clear`, `load_file from memory vfs`, `collect called after unload` (memory source stats), `reparented root changes owner`.
- `test_register.cpp`: `register_systems adds four systems in expected phases and flags`, `scheduler finalise succeeds`, `destroy_pending runs after scripting.frame_end when present`, `register_asset_types registers two types and two importers`.
- `tests/benchmarks/bench_scene.cpp` (not ctest): `propagate_10k_flat`, `propagate_10k_depth8`, `set_parent_subtree_1k`, `instantiate_prefab_100`, `snapshot_capture_restore_10k`, `destroy_cascade_10k`. Targets: propagation 10k both matrices < 0.3 ms on 8 workers, snapshot capture + restore < 50 ms.

Integration (`08_testing.md` 6.1, wave 4): `test_hierarchy_propagation.cpp`, `test_scene_roundtrip.cpp`,
`test_prefab_spawn_destroy.cpp`, `test_scene_switch.cpp`, `test_render_transform.cpp` (`06_engine_loop.md` 10).

---

## 11. Out of scope / future

- Prefab override records and propagation of prefab edits to modified instances (v1: `diff`, `revert`,
  `refresh_instances` of unmodified ones).
- Binary cooked scene format (JSON only; `scene_snapshot` is in-memory); scene asset hot reload of a loaded
  scene (the editor re-opens).
- Streaming or distance-based scene loading; scene-level LOD; multiple worlds per process.
- Hierarchy depth beyond 31; shear-preserving transforms under non-uniform parent scale.
- Entity-granular change detection in the ECS (scene lives with chunk granularity plus the stamp).
- Per-scene `destroy_all` for scripting on additive unload (the roots span is passed so scripting can refine).
- Static subtrees with baked world matrices; `changed<world_transform>` consumers in the renderer.

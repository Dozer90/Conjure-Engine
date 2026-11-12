# Scenes and the hierarchy

📘 **Public API wiki** · 🛠 Handbook: [Scene module](../../handbook/modules/scene.md)

How to create entities in a scene, parent them, move them, name them and find them again, what the scene
components are, and how transforms reach the renderer every frame. Headers: `conjure/scene/scene.h`,
`conjure/scene/components.h`, `conjure/scene/hierarchy.h`, `conjure/scene/guid_index.h`,
`conjure/scene/transform_system.h` and `conjure/scene/register.h`. Everything is in `namespace conjure::scene`.

## At a glance

| You want to... | Use |
|---|---|
| Register the scene components once at start-up | `register_components(registry)` |
| Create an entity with a name, guid and transform | `scene.create_entity(name, parent, index)` |
| Create many copies of a template subtree | `scene.create_entities(count, templateEntity, parent, out)` |
| Copy a subtree | `scene.duplicate_subtree(root, parent, index)` |
| Destroy at the end of the frame / now | `scene.destroy(entity)` / `scene.destroy_now(entity)` |
| Reparent, keeping the world pose | `scene.set_parent(child, newParent, keepWorld = true, index)` |
| Reparent from inside a system (world locked) | `scene.deferred_set_parent(commands, child, newParent, ...)` |
| Reorder among siblings or roots | `scene.set_sibling_index(entity, index)`, `scene.set_root_order(ordered)` |
| Edit the local transform | `scene.set_local_transform(entity, value)`, `scene.set_world_transform(entity, value)` |
| Read a transform | `scene.local_transform_of(entity)`, `scene.world_transform_of(entity)` (fresh walk) or the `world_transform` component (last frame's pass) |
| Name, tag, layer, enabled | `set_name`, `set_tag`, `set_layer`, `set_enabled`; `name_of`, `tag_of`, `layer_of`, `is_enabled`, `is_active` |
| Find entities | `scene.find_path("Root/Child")`, `find_by_name`, `find_by_tag`, `find_by_layer`, `find_guid`, `resolve_guid` |
| Walk the tree | `scene.tree().parent_of(e)`, `children_of(e)`, `depth_of(e)`, `root_of(e)`, `for_each_in_subtree(e, fn)` |
| Add the four scene systems to a scheduler | `register_systems(scheduler, sceneManager)` |

## Quick example

```cpp
#include "conjure/scene/scene.h"

using namespace conjure;

core::result<void> spawn_crate(scene::scene &level, ecs::entity shelf) {
    CONJURE_TRY_ASSIGN(const ecs::entity crate, level.create_entity(core::string_id("crate"), shelf));
    scene::local_transform pose = {};
    pose.position = math::vec3(0.0f, 0.5f, 0.0f);
    CONJURE_TRY(level.set_local_transform(crate, pose));
    CONJURE_TRY(level.set_layer(crate, 2));

    // The exact world pose now, from a walk up the tree.
    const std::optional<math::transform> world = level.world_transform_of(crate);
    CONJURE_ASSERT(world.has_value(), "a created entity always has a transform");

    // Children follow when the shelf moves; the frame's transform pass writes world_transform.
    CONJURE_TRY(level.set_parent(crate, ecs::NULL_ENTITY, true));      // now a root at the same place
    return core::result<void>::success();
}
```

## The scene components

Every scene component is a plain struct registered under `"conjure.scene.<name>"`. You read and write them like
any other component; the scene keeps the ones that describe structure consistent for you.

| Component | Purpose | Who writes it |
|---|---|---|
| `parent { entity value; u32 siblingIndex; }` | the link to the parent; roots have none | the scene (`set_parent`); a raw edit is repaired by `scene.apply_hierarchy_changes` with one WARN |
| `children` | cache of the children in sibling order (`small_vector<entity, 4>`), transient | the scene |
| `local_transform { position, rotation, scale }` | the pose relative to the parent; byte-identical to `math::transform` | you |
| `world_transform { mat4 matrix }` | the simulation pose, transient | `scene.propagate_transforms` |
| `render_world_transform { mat4 matrix }` | what the renderer reads: the world matrix, or the interpolated physics pose, transient | `scene.propagate_transforms` |
| `interpolated_transform { previous, current }` | the last two fixed-step poses, transient | physics only (defined here, added and written there) |
| `transform_state`, `transform_dirty`, `depth_tag<1..31>` | propagation bookkeeping and the depth of every entity, transient and hidden | the scene |
| `name { string_id value }`, `tag { string_id value }`, `layer { u8 index }` | identity and grouping; layer bit `1 << index` matches the renderer's layer masks | you (through the setters) |
| `guid { uuid value }` | the stable editor identity, indexed by the scene | the scene (`create_entity`, `ensure_guid`, `set_guid`) |
| `disabled`, `inactive` | the entity is switched off / an ancestor is | `set_enabled` |
| `local_bounds { aabb box }` | local-space bounds; `scene.world_bounds(e)` transforms them, transient | engine subsystems |
| `prefab_instance`, `prefab_link` | prefab markers, see [Scene files, prefabs and snapshots](scene-files-and-prefabs.md) | the prefab functions |

`scene_component_ids::resolve(registry)` gives you every id at once (`parent`, `localTransform`, `depth[d]`,
...) when you write a system over these columns; `is_complete()` tells you `register_components` has run.

> [!TIP]
> Use `scene.set_parent` rather than writing `parent` yourself. A raw write still works (the scene repairs the
> `children` cache and depth tags at the next TRANSFORM phase) but costs a warning and a frame of inconsistency.
> From inside a system, where the world is locked, record the change with `deferred_set_parent` into the
> system's command buffer; the repair is then silent.

## Creating, moving and destroying

`create_entity(name, parent = NULL_ENTITY, index = APPEND_CHILD)` creates the entity in its final archetype in
one go: guid, name, `local_transform` and its companions, `transform_dirty`, the `parent` link and the depth tag.
It fails with `INVALID_ENTITY` for a dead parent and `HIERARCHY_TOO_DEEP` beyond depth 31 (`MAX_HIERARCHY_DEPTH`).

`set_parent(child, newParent, keepWorld = true, index = APPEND_CHILD)` checks everything first (`SELF_PARENT`,
`HIERARCHY_CYCLE`, `HIERARCHY_TOO_DEEP`, `INVALID_ENTITY`) and then moves the whole subtree in one flush, so every
touched entity changes archetype at most once. With `keepWorld` the local transform is rewritten so the world
pose does not jump. Passing `NULL_ENTITY` makes the entity a root at `index` among the roots.

`destroy(entity)` queues the entity for `scene.destroy_pending` at FRAME_END; `destroy_now` destroys it and its
subtree immediately, without recursion, through the ECS deferred buffer. `clear()` empties the whole world and
keeps the settings and name.

`create_entities(count, templateEntity, parent, out)` stamps `count` copies of a template subtree with fresh
guids (`NULL_ENTITY` as template gives plain entities); either way it is one structural change, and a failure
leaves no entity of the call behind. `duplicate_subtree(root, parent, index)` copies one subtree, deep-copying
components that have their own copy hooks (scripts included).

> [!WARNING]
> Every method of `scene` is for the world's owner thread while the world is unlocked, exactly like structural
> ECS calls. Inside a system use the command buffer route (`deferred_set_parent`, `context.commands()`).

## Finding things

| Function | Returns |
|---|---|
| `find_path("Root/Child/Leaf")` | the entity by names, first among the roots, then down the children; `NULL_ENTITY` on a miss |
| `find_by_name(id, out)`, `find_by_tag(id, out)`, `find_by_layer(index, out)` | appends every match in world order (a full scan of that column); `find_by_layer(0, ...)` lists only entities carrying a `layer` component, although `layer_of` reports 0 for the others |
| `find_guid(uuid)` / `resolve_guid(uuid)` | the entity, or `NULL_ENTITY` / `GUID_NOT_FOUND` |
| `guid_of(e)`, `ensure_guid(e)` | the guid; `ensure_guid` generates and indexes one when missing |
| `roots()` | every root in order with its owner (`root_entry {entity, owner}`) |
| `contains(e)` | alive in this world |

`scene.guids()` exposes the `guid_index` itself (`find`, `contains`, `size`); the scene keeps it in step through
observers, so you only read it.

## Walking the tree

`scene.tree()` returns the `hierarchy`, a read-only view over the `parent` and `children` columns:
`parent_of(e)`, `children_of(e)` (a span in sibling order), `sibling_index(e)` (0 for a root: the root order is
`scene.roots()`), `depth_of(e)`, `is_ancestor_of(a, e)`, `root_of(e)`, `subtree_height(e)`, `subtree_size(e)`,
`collect_subtree(e, out)` and `for_each_in_subtree(e, fn)`. Every walk is iterative, so a 10,000-entity chain is
fine.

## Transforms every frame

`register_systems(scheduler, sceneManager)` adds the four scene systems, all `RUNS_WHILE_PAUSED` so the editor
keeps rendering while paused:

| System | Phase | What it does |
|---|---|---|
| `scene.apply_pending_loads` | FRAME_BEGIN | applies queued scene loads and unloads |
| `scene.apply_hierarchy_changes` | TRANSFORM (exclusive, first) | repairs raw and deferred `parent` edits |
| `scene.propagate_transforms` | TRANSFORM (parallel) | writes `world_transform` and `render_world_transform` for every entity whose local transform or ancestor changed |
| `scene.destroy_pending` | FRAME_END | destroys what `destroy()` queued |

Propagation runs one hierarchy level at a time over chunks, so it is parallel and recomputes only what changed:
a moved root touches its subtree and nothing else, and a steady scene costs a few chunk-version comparisons.
`sceneManager.transforms().last_stats()` reports what the last run did (`propagation_stats`:
`entitiesVisited`, `worldRecomputed`, `renderRecomputed`, `chunksVisited`, `chunksSkipped`, `levels`);
`set_interpolation_enabled(bool)` switches the physics interpolation of `render_world_transform` (the engine
forwards its `interpolatePhysics` setting). `compute_world_transform(scene, entity)` is the same maths for one
entity, computed now.

> [!WARNING]
> `world_transform` is last frame's result until the TRANSFORM phase has run. When you need the pose right after
> editing a local transform, ask `scene.world_transform_of(entity)`, which walks the ancestors fresh.

## API summary

| Signature (short) | Purpose |
|---|---|
| `scene(ecs::world &, assets::asset_manager *)` | binds a scene to a world (the manager is for the bind pass; null in pure ECS tests) |
| `result<entity> create_entity(string_id name, entity parent = {}, u32 index = APPEND_CHILD)` | one entity in its final archetype |
| `result<instantiate_report> create_entities(u32 count, entity templateEntity, entity parent, span<entity> out)` | copies of a template |
| `result<entity> duplicate_subtree(entity root, entity parent = {}, u32 index = APPEND_CHILD)` | deep copy |
| `void destroy(entity)`, `result<void> destroy_now(entity)`, `void clear()` | destruction |
| `result<void> set_parent(entity child, entity parent, bool keepWorld = true, u32 index = APPEND_CHILD)` | reparent |
| `result<void> deferred_set_parent(ecs::command_buffer &, entity child, entity parent, bool keepWorld = true, u32 index = APPEND_CHILD)` | reparent from a locked world |
| `result<void> set_sibling_index(entity, u32)`, `set_root_order(span<const entity>)` | ordering |
| `result<void> add_transform(entity, const local_transform & = {})` | gives an entity a transform and its companions |
| `result<void> set_local_transform(entity, const local_transform &)`, `set_world_transform(entity, const math::transform &)` | pose edits |
| `optional<math::transform> local_transform_of(entity) const`, `world_transform_of(entity) const` | pose reads |
| `optional<math::aabb> world_bounds(entity) const`, `subtree_bounds(entity) const` | bounds from `local_bounds` |
| `result<uuid> ensure_guid(entity)`, `result<void> set_guid(entity, const uuid &)`, `optional<uuid> guid_of(entity) const` | identity |
| `entity find_guid(const uuid &) const`, `result<entity> resolve_guid(const uuid &) const`, `entity find_path(string_view) const` | lookups |
| `result<void> set_name / set_tag(entity, string_id)`, `set_layer(entity, u8)`, `set_enabled(entity, bool)` | attributes |
| `string_id name_of / tag_of(entity) const`, `u8 layer_of(entity) const`, `bool is_enabled / is_active(entity) const` | attribute reads |
| `span<const root_entry> roots() const`, `const hierarchy &tree() const`, `const guid_index &guids() const` | views |
| `core::event<> &on_structure_changed()`, `on_settings_changed()` | notifications (main thread) |
| `u64 structure_version() const`, `core::json::document &settings()`, `string_view name()` | state |
| `result<void> register_components(ecs::component_registry &)` | `register.h` |
| `result<void> register_systems(ecs::scheduler &, scene_manager &)` | `register.h` |

## See also

- 📘 API: [Scene files, prefabs and snapshots](scene-files-and-prefabs.md), [Queries, views and systems](../ecs/queries-views-and-systems.md), [Math](../core/math.md)
- 🛠 Handbook: [Scene module](../../handbook/modules/scene.md)

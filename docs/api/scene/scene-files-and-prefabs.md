# Scene files, prefabs and snapshots

📘 **Public API wiki** · 🛠 Handbook: [Scene module](../../handbook/modules/scene.md)

How a scene becomes a `.scene.json` file and comes back, how prefabs are built, instantiated, diffed and applied,
how a scene is snapshotted for play and stop, and how the `scene_manager` loads scenes as assets. Headers:
`conjure/scene/scene_serializer.h`, `conjure/scene/prefab.h`, `conjure/scene/scene_snapshot.h` and
`conjure/scene/scene_manager.h`. Everything is in `namespace conjure::scene`.

## At a glance

| You want to... | Use |
|---|---|
| Save the scene to a file | `scene_serializer::save_scene_file(scene, fs, "scenes/main.scene.json")` |
| Load a file into the scene | `scene_manager.load_file(fs, path, load_mode::REPLACE)` or `scene_serializer::load_scene_file(...)` |
| Load a scene asset by uuid (dependencies first) | `scene_manager.load(assetId, load_mode::REPLACE)` |
| Load additively, later, without blocking | `scene_manager.request_load(assetId, load_mode::ADDITIVE)`; applied at `scene.apply_pending_loads` |
| Unload a loaded scene | `scene_manager.unload(sceneId)`, `request_unload(sceneId)`, `unload_all()` |
| Turn a subtree into a prefab document | `prefab::build_document(scene, root, "name")` |
| Instantiate a prefab asset | `prefab::instantiate(scene, asset, options)`, `prefab::instantiate_many(...)` |
| See what changed in an instance / undo it / keep it | `prefab::diff`, `prefab::revert`, `prefab::apply` |
| Stop treating entities as an instance | `prefab::unpack(scene, root, recursive)` |
| Build a prefab without naming component types (model import) | `prefab_builder` |
| Snapshot for play, restore on stop | `capture(scene)` then `restore(scene, snapshot)` |
| Register the scene asset types and importers | `register_asset_types(types, importers, registry)` |

## Quick example

```cpp
#include "conjure/scene/prefab.h"
#include "conjure/scene/scene_manager.h"
#include "conjure/scene/scene_serializer.h"

using namespace conjure;

core::result<void> stamp_crates(scene::scene_manager &scenes, const scene::prefab_asset &crate) {
    scene::scene &level = scenes.active();
    std::array<ecs::entity, 10> roots = {};
    scene::instantiate_options options = {};
    options.parent = level.find_path("shelf");
    CONJURE_TRY(scene::prefab::instantiate_many(level, crate, 10, options, roots));

    // Save the result; a later load reproduces it byte for byte.
    core::vfs::memory_file_system fs = {};
    CONJURE_TRY(scene::scene_serializer::save_scene_file(level, fs, "scenes/warehouse.scene.json"));
    CONJURE_TRY_ASSIGN(const scene::scene_id loaded, scenes.load_file(fs, "scenes/warehouse.scene.json"));
    return scenes.unload(loaded);
}
```

## Scene files

A `.scene.json` file is a small header plus the ECS archive of every entity in hierarchy order:

```json
{"format":"conjure.scene","version":1,"name":"main","settings":{...},
 "roots":["<guid>", ...],"archive":{"entities":[{"components":{"conjure.scene.name":{...}, ...}}, ...]}}
```

`save_scene(scene, writer)` and `save_scene_file(scene, fs, path)` write it (the file write is atomic);
`parse_document(value, registry)` reads it into a `scene_document {name, settings, roots, archive}`, and
`load_scene(scene, document, owner, guids)` instantiates it: every entity in its final archetype, children and
depth tags linked, the roots ordered as the header lists them, and every `asset_ref` field bound through the
asset manager (the bind pass). `load_scene_file` does all three. Save, load, save again is byte-identical;
components the registry does not know survive untouched in `unresolved_components`.

Errors: `BAD_FORMAT` (wrong format name, missing archive, malformed roots or settings), `UNSUPPORTED_VERSION`,
`DUPLICATE_GUID` with `guid_policy::KEEP`, `HIERARCHY_CYCLE`, `HIERARCHY_TOO_DEEP`, and the ECS's
`SERIALIZATION_FAILED` for a malformed component. Nothing is created when a load fails.

`guid_policy` says what happens to the file's guids: `KEEP` (a collision fails the load), `REGENERATE_ON_CONFLICT`
(colliding entries get fresh guids with one WARN) or `REGENERATE_ALL` (prefabs and copies).

> [!IMPORTANT]
> Scripts, bodies and renderer components in a file are just components to the scene. A component that owns
> external state (the scripting module's `script_component`) carries its own serializer hooks, so it rides along;
> the scene never calls into those modules.

## The scene manager

`scene_manager(world, assets)` owns the one `scene` over the world (`active()`), the loaded-scene records and
their asset handles, the `layer_table` (32 layer names the engine fills from `project.json`) and the transform
system the scheduler runs.

| Call | What happens |
|---|---|
| `load(assetId, mode)` | `load_blocking` of the scene asset and its dependencies, then the document is applied; `LOAD_FAILED` when the asset is not a scene |
| `load_file(fs, path, mode)` | parses a file, applies it, and loads the referenced assets as a batch |
| `request_load(assetId, mode)` | queues; `apply_pending_loads()` (the FRAME_BEGIN system) applies it once the asset is `LOADED`, in request order; a `FAILED` asset drops the request with ERROR and `on_load_failed` |
| `unload(sceneId)` | fires `on_before_unload(id, roots)` with the roots still alive, destroys them in reverse order, drops the handles and runs `asset_manager::collect()` |
| `unload_all()` | every scene in reverse load order, then the remaining authored entities |
| `is_loaded(id)`, `find_loaded(assetId)`, `loaded_scenes()`, `info(id)`, `is_load_pending(assetId)` | queries; `loaded_scene_info` lists the current roots on every call |

`load_mode::REPLACE` first checks that the new file would load (so a bad file leaves the current content in
place), unloads everything, and takes the file's name and settings; `ADDITIVE` keeps what is there and regenerates
colliding guids with one WARN. Ids retired by an unload or a REPLACE are never reused.

> [!WARNING]
> `on_before_unload` is where modules that own per-entity state outside the ECS (scripting's managed instances)
> clean up. Subscribe before the first load; the event fires on the main thread inside `unload`.

## Prefabs

A prefab is a `prefab_document {name, archive}` whose entry 0 is the single root. `prefab::build_document(scene,
root, name)` captures a subtree (references to entities outside the subtree become null); the `conjure.scene.prefab`
asset type publishes it as `prefab_asset {archive, name, rootGuid, dependencies, id}`.

Instances are fully materialised: `prefab::instantiate(scene, asset, options)` creates real entities with fresh
guids, puts `prefab_instance {asset_ref<prefab_asset>}` on the root and `prefab_link {assetGuid}` on every
entity, and binds the asset references. `instantiate_many(scene, asset, count, options, outRoots)` does it in one
batch. `instantiate_options` places the copies (`parent`, `index`, `owner`), may rename the root or override its
transform, and can skip the bind pass.

| Function | Purpose |
|---|---|
| `prefab::instance_root_of(scene, entity)` | the nearest ancestor (itself included) carrying `prefab_instance` |
| `prefab::diff(scene, root)` | `prefab_diff` of `entity_diff {assetGuid, instanceEntity, kind, components}` entries: `ADDED`, `REMOVED` or `MODIFIED` with the component names; components are compared as canonical JSON with local references remapped, and the parent link by parent guid and sibling index |
| `prefab::revert(scene, root)` | replaces the instance with a fresh one at the same place, keeping the root's guid, name and transform |
| `prefab::apply(scene, root)` | the instance as a new document: link guids become entry guids, the root loses its marker |
| `prefab::refresh_instances(scene, prefabId, onlyUnmodified = true)` | reverts every instance of the asset; returns how many |
| `prefab::unpack(scene, root, recursive = false)` | removes the markers, stopping at nested instance roots unless recursive |
| `prefab::mark_instance(scene, root, asset_ref)` | marks an existing subtree as an instance (guids generated where missing) |

Errors: `NOT_A_PREFAB_INSTANCE`, `PREFAB_NOT_LOADED` (the root's `prefab_instance` reference is not resident),
`PREFAB_MULTIPLE_ROOTS` for a document whose entry 0 is not the only root.

`prefab_builder(registry)` builds a document without naming any C++ component type: `add_node(parentIndex, name,
transform)`, then `add_component_json(node, componentName, value)`, `add_component_raw(node, id, bytes)` or
`add_tag(node, id)`, and `build(name)`. The engine's model importer turns a glTF node tree into a prefab this way.
`BUILDER_INVALID` names a bad parent, a second root or a component the builder writes itself (`parent`, `name`,
`local_transform`, guids, depth tags).

## Snapshots

`capture(scene)` returns a `scene_snapshot`: the typed archive of every entity in hierarchy order plus the root
order, owners, settings, name and structure version; no JSON is involved, so it is fast enough for play and stop.
`restore(scene, snapshot)` clears the scene and instantiates the snapshot with the guids kept; on failure the
scene is left empty rather than half-restored. `snapshot.entity_count()` says how much it holds.

## API summary

| Signature (short) | Purpose |
|---|---|
| `static result<void> scene_serializer::save_scene(const scene &, core::json::writer &)` | write the scene format |
| `static result<void> scene_serializer::save_scene_file(const scene &, vfs::file_system &, string_view path)` | atomic file write |
| `static result<scene_document> scene_serializer::parse_document(const json::value &, const ecs::component_registry &)` | header and archive |
| `static result<prefab_document> scene_serializer::parse_prefab(const json::value &, const ecs::component_registry &)` | prefab format |
| `static result<void> scene_serializer::write_prefab(const prefab_document &, json::writer &)` / `write_document(const scene_document &, json::writer &)` | writers |
| `static result<instantiate_report> scene_serializer::load_scene(scene &, const scene_document &, scene_id owner, guid_policy)` | instantiate |
| `static result<instantiate_report> scene_serializer::load_scene_file(scene &, vfs::file_system &, string_view, scene_id, guid_policy)` | read, parse, load |
| `static result<void> scene_serializer::save_subtree(const scene &, entity root, json::writer &)` | a subtree in prefab format |
| `static result<entity> scene_serializer::instantiate_subtree(scene &, const json::value &, entity parent, bool keepGuids, u32 index = APPEND_CHILD)` | parse and place |
| `static result<ecs::entity_archive> scene_serializer::capture_subtree(const scene &, entity root, ecs::archive_options = {})` | hierarchy-ordered capture |
| `static void scene_serializer::bind_asset_refs(ecs::world &, span<const entity>, assets::asset_manager &, vector<asset_id> *outIds)` / `bind_entity(world &, entity, asset_manager &)` | the bind pass |
| `result<instantiate_report> scene::instantiate_archive(const shared_ptr<const entity_archive> &, u32 count, const instantiate_options &, span<entity> outRoots)` | the primitive behind all of the above |
| `scene_manager(ecs::world &, assets::asset_manager &)` | construction |
| `result<scene_id> scene_manager::load(const asset_id &, load_mode = REPLACE)`, `load_file(vfs::file_system &, string_view, load_mode = REPLACE)` | synchronous loads |
| `load_request_id scene_manager::request_load(const asset_id &, load_mode = ADDITIVE)`, `void request_unload(scene_id)`, `void apply_pending_loads()` | deferred |
| `result<void> scene_manager::unload(scene_id)`, `void unload_all()` | unloads |
| `core::event<scene_id, span<const entity>> &on_before_unload()`, `event<scene_id> &on_loaded()`, `event<load_request_id, const error &> &on_load_failed()` | events |
| `result<prefab_document> prefab::build_document(const scene &, entity root, string_view name)` | capture |
| `result<entity> prefab::instantiate(scene &, const prefab_asset &, const instantiate_options & = {})` | one instance |
| `result<void> prefab::instantiate_many(scene &, const prefab_asset &, u32 count, const instantiate_options &, span<entity> outRoots)` | many |
| `result<prefab_diff> prefab::diff(const scene &, entity root)`, `result<void> revert(scene &, entity root)`, `result<prefab_document> apply(const scene &, entity root)` | overrides |
| `result<scene_snapshot> capture(const scene &)`, `result<void> restore(scene &, const scene_snapshot &)` | play / stop |
| `result<void> register_asset_types(assets::asset_type_registry &, assets::importer_registry &, const ecs::component_registry &)` | `conjure.scene.scene` (`.scene.json`) and `conjure.scene.prefab` (`.prefab.json`) |

## See also

- 📘 API: [Scenes and the hierarchy](scenes-and-hierarchy.md), [Archives and serialisation](../ecs/archives-and-serialization.md), [Database, importers and hot reload](../assets/database-and-import.md)
- 🛠 Handbook: [Scene module](../../handbook/modules/scene.md)

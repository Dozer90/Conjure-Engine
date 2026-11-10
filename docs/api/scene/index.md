# Scenes, hierarchy and prefabs

📘 **Public API wiki** · 🛠 Handbook: [Scene module](../../handbook/modules/scene.md)

The scene module is where a game's level lives: a tree of entities with names, tags, layers and stable ids,
local and world transforms that the engine keeps in step every frame, `.scene.json` files that load and save
byte-for-byte, prefabs that stamp out copies, and snapshots for play and stop. Everything on these pages lives in
`namespace conjure::scene` and sits on top of the ECS: a scene is one `ecs::world` plus the bookkeeping that
makes it a hierarchy. There is no scene graph object to walk; the hierarchy is data in components, and the
`scene` class is the facade that keeps that data consistent.

## The mental model in five sentences

1. A `scene` wraps one `ecs::world`. You create entities through it (`create_entity(name, parent)`), reparent
   them (`set_parent`), name and tag them, and destroy them; the scene writes the `parent`, `children`, depth and
   transform components so the ECS side always agrees with the tree.
2. Every entity you give a transform carries `local_transform` (what you edit) and `world_transform` (what the
   engine computes). The `scene.propagate_transforms` system recomputes world matrices once per frame, in
   parallel, only for the entities that changed.
3. Every scene entity has a `guid`, a stable id that survives saving and loading. Editors and the AI protocol
   refer to entities by guid or by path (`"Root/Child/Leaf"`), never by the raw ECS id.
4. A scene file is the ECS archive of the tree plus a small header, so anything the ECS can serialise is in the
   file, including components from modules that are not loaded (they survive untouched).
5. A prefab is a saved subtree. Instantiating it creates real entities marked with `prefab_instance` and
   `prefab_link`, so you can diff an instance against its asset, revert it, or apply it back.

The `scene_manager` owns residency: it loads scene assets through the asset manager, keeps their asset handles
alive, and unloads them with one call.

## Where to go next

| Page | Read it when you want to... |
|---|---|
| [Scenes and the hierarchy](scenes-and-hierarchy.md) | create, name, parent and move entities; read transforms; find entities by path, name, tag or layer; understand the scene components and the transform system |
| [Scene files, prefabs and snapshots](scene-files-and-prefabs.md) | save and load `.scene.json`, build and instantiate prefabs, diff and revert instances, snapshot a scene for play and stop, load scenes through the asset manager |

## Headers

| Header | What it holds |
|---|---|
| `conjure/scene/components.h` | the scene components: `parent`, `children`, `local_transform`, `world_transform`, `render_world_transform`, `interpolated_transform`, `transform_state`, `transform_dirty`, `depth_tag<D>`, `name`, `tag`, `layer`, `guid`, `disabled`, `inactive`, `local_bounds`, `prefab_instance`, `prefab_link`, `scene_component_ids` |
| `conjure/scene/scene.h` | `scene`, `scene_id`, `root_entry`, `guid_policy`, `instantiate_options`, `instantiate_report` |
| `conjure/scene/hierarchy.h` | `hierarchy`: read-only tree queries (`parent_of`, `children_of`, `depth_of`, `root_of`, subtree walks) |
| `conjure/scene/guid_index.h` | `guid_index`: the guid to entity map the scene maintains |
| `conjure/scene/transform_system.h` | `transform_system`, `propagation_stats`, `compute_world_transform()` |
| `conjure/scene/scene_serializer.h` | `scene_document`, `prefab_document`, `scene_serializer` (save, parse, load, subtrees, the asset bind pass) |
| `conjure/scene/prefab.h` | `prefab_asset`, `prefab_diff`, the `prefab::` functions, `prefab_builder` |
| `conjure/scene/scene_snapshot.h` | `scene_snapshot`, `capture()`, `restore()` |
| `conjure/scene/scene_manager.h` | `scene_manager`, `load_mode`, `loaded_scene_info`, `layer_table` |
| `conjure/scene/register.h` | `register_components()`, `register_asset_types()`, `register_systems()` and the system names |

## Quick tour

```cpp
#include "conjure/scene/register.h"
#include "conjure/scene/scene.h"
#include "conjure/scene/scene_manager.h"

using namespace conjure;

core::result<void> build_level(ecs::component_registry &registry, assets::asset_manager &assets) {
    CONJURE_TRY(scene::register_components(registry));
    ecs::world world(registry);
    scene::scene_manager scenes(world, assets);
    scene::scene &level = scenes.active();

    CONJURE_TRY_ASSIGN(const ecs::entity player, level.create_entity(core::string_id("player")));
    CONJURE_TRY_ASSIGN(const ecs::entity lamp, level.create_entity(core::string_id("lamp"), player));
    scene::local_transform offset = {};
    offset.position = math::vec3(0.0f, 2.0f, 0.0f);
    CONJURE_TRY(level.set_local_transform(lamp, offset));
    CONJURE_TRY(level.set_tag(lamp, core::string_id("Light")));

    // Reparenting keeps the world pose by default; "player/lamp" resolves by name.
    CONJURE_TRY(level.set_parent(lamp, ecs::NULL_ENTITY));
    return core::result<void>::success();
}
```

`scenes.active()` is the one scene over the world; `register_systems(scheduler, scenes)` (see
[Scenes and the hierarchy](scenes-and-hierarchy.md)) adds the four systems that keep it running every frame. The
engine does both at start-up; a tool or a test does them by hand, as above.

> [!NOTE]
> The scene module never includes the renderer, physics or scripting. Rendering components, bodies and scripts
> are ordinary components on scene entities; their modules read the scene's `parent` and transform columns.

## Local terms

| Term | Meaning |
|---|---|
| Root | an entity without a `parent` component; the scene keeps the roots in order (`roots()`) |
| Depth | distance from the root, 0 for roots; encoded as one tag component per level so transform propagation runs one level at a time |
| Owner | the loaded scene asset a root belongs to (`scene_id`); hand-made entities belong to `SCENE_ID_AUTHORED` |
| Bind pass | after a load, every `asset_ref` field in the new entities is bound to its asset slot through the asset manager |
| Instance | the entities created from a prefab; the root carries `prefab_instance`, every entity a `prefab_link` to its entry in the asset |

## See also

- 📘 API: [Entities, components and systems](../ecs/index.md), [Archives and serialisation](../ecs/archives-and-serialization.md), [Referencing and loading assets](../assets/referencing-and-loading.md)
- 🛠 Handbook: [Scene module](../../handbook/modules/scene.md)

# Archives and serialisation

📘 **Public API wiki** · 🛠 Handbook: [ECS module](../../handbook/modules/ecs.md)

How to copy a group of entities out of a world and put copies back in (prefabs, scene files, copy and paste,
undo, spawning from scripts), how that copy becomes JSON and comes back, and what the archetypes and chunks you
may meet in tools and profilers are. Headers: `conjure/ecs/entity_archive.h`, `conjure/ecs/serialization.h` and
`conjure/ecs/archetype.h`. Everything is in `namespace conjure::ecs`.

## At a glance

| You want to... | Use |
|---|---|
| Copy entities out of a world | `entity_archive::capture(world, entities, options)` |
| Spawn one copy | `world.instantiate(archive)` returns the root entity |
| Spawn many copies at once | `world.instantiate_batch(archive, roots)` |
| Spawn from inside a system | `world.instantiate(archive, &context.commands())` or `commands.instantiate(archive)` |
| Save an archive as JSON | `write_archive_json(archive, writer)` |
| Load it back | `read_archive_json(value, registry)` |
| Show or edit one live component as JSON (inspector, AI protocol) | `component_to_json(world, id, componentId, writer)` / `component_from_json(world, id, componentId, value)` |
| Keep data from modules that are not loaded | nothing: unknown names survive in `unresolved_components` automatically |
| Look at how entities are stored | `world.archetypes()`, `archetype::mask()`, `layout()`, `entity_count()`, `chunk_count()` |

## Quick example

```cpp
#include "conjure/core/json/document.h"
#include "conjure/core/json/writer.h"
#include "conjure/ecs/entity_archive.h"
#include "conjure/ecs/serialization.h"
#include "conjure/ecs/world.h"

using namespace conjure;
using namespace conjure::ecs;

core::result<std::string> save_prefab(const world &game, std::span<const entity> members) {
    CONJURE_TRY_ASSIGN(const entity_archive archive, entity_archive::capture(game, members));
    core::json::writer out = {};
    CONJURE_TRY(write_archive_json(archive, out));
    return out.finish();
}

core::result<entity> spawn_prefab(world &game, std::string_view text) {
    CONJURE_TRY_ASSIGN(const core::json::document document, core::json::document::parse(text));
    CONJURE_TRY_ASSIGN(entity_archive loaded, read_archive_json(document.root(), game.registry()));
    const std::shared_ptr<const entity_archive> archive =
      std::make_shared<const entity_archive>(std::move(loaded));
    return game.instantiate(archive);                       // entry 0 of the archive is the root
}
```

`core::json::writer` and `core::json::document` are described on 📘 API: [JSON](../core/json.md).

## Archives: a typed copy of entities

`entity_archive` is an in-memory copy of N entities: for each one its component set and the bytes of every
non-`TRANSIENT` component, produced with the type's copy constructor (or `memcpy`). It is bound to the
`component_registry` whose ids it uses, move-only, and destroys its payloads when it dies. Entry 0 is the root.

`entity_archive::capture(world, std::span<const entity>, archive_options = {})` copies the given entities in
order. Every `entity` field of a reflected component is rewritten: a reference to another member of the set
becomes a **local index** into the archive, a reference to anything else becomes `NULL_ENTITY`. Set
`archive_options::keepExternalReferences = true` (editor copy and paste inside one world) to keep outside
references as they are. `ENTITY_NOT_ALIVE` when a member is dead; `STRUCTURE_LOCKED` while a view is iterating.

| Member | Purpose |
|---|---|
| `entity_count()`, `entries()` | the `archive_entry{mask, components}` list in capture order |
| `component_bytes(entryIndex, componentId)` | the object image of one component (empty for a tag or when absent) |
| `registry()` | the registry the ids belong to |
| `validate()` | `ARCHIVE_CORRUPT` when an id is unknown, a payload has the wrong size or a local index points outside the archive |
| `push_entry(entry)` | appends an entry whose payloads are moved in (loaders and tests) |

> [!IMPORTANT]
> Only `entity` fields that reflection can see are rewired. A component that hides references in a type the
> serialiser does not walk must supply `component_serializer::remapEntities` or lose them on instantiation.

## Instantiating

`world.instantiate(archive, commands = nullptr)` creates one copy and returns its root; `instantiate_batch(archive,
std::span<entity> outRoots, commands = nullptr)` creates `outRoots.size()` copies and fills the roots. Both take
the archive as `std::shared_ptr<const entity_archive>` so a command buffer can hold it until the flush. Every
copy's entities are placed directly in their final archetypes, internal references are remapped to the new ids
(`entity_remap` does local index to real entity), `ADDED` observers fire last with the remapped data in place.

With a command buffer (`world.instantiate(archive, &commands)` or `commands.instantiate(archive)`), the ids are
reserved immediately and the root is returned now; the copy is made at flush. This is how a system spawns a
prefab. The buffer must belong to the same world: one created on another world is refused with
`INVALID_ARGUMENT` (asserted in Debug and Development) and nothing is reserved. If the copy never applies (the
buffer is cleared or destroyed, or the flush fails to place it), its ids are released with a bumped generation,
so the root you were handed stays dead instead of naming a later entity.

> [!WARNING]
> An archive is validated against the world's registry before anything is created: `ARCHIVE_CORRUPT` means the
> world is untouched. An archive captured on one registry cannot be instantiated into a world on another.

```cpp
core::result<void> spawn_wave(world &game, const std::shared_ptr<const entity_archive> &enemy, u32 count) {
    std::vector<entity> roots(count);
    CONJURE_TRY(game.instantiate_batch(enemy, roots));
    for (const entity root : roots) {
        position *place = game.get<position>(root);
        if (place != nullptr) {
            place->x = static_cast<f32>(root.index % 10) * 2.0f;
        }
    }
    return core::result<void>::success();
}
```

## JSON

Archives serialise as `{"entities":[{"components":{"<name>":{...}}}]}`, one object per entry keyed by the
registered component name. Inside a component the default serialiser walks the reflection description:

| Field kind | JSON |
|---|---|
| bool, integers, floats, `std::string`, `string_id`, `uuid`, enums | the scalar (`string_id` and `uuid` as text) |
| math types (`vec3`, `quat`, matrices) and `std::array<T, N>` | arrays |
| nested reflected struct | an object |
| `entity` | the integer local index of the referenced entry; `null` for `NULL_ENTITY`; `{"external":"index:generation"}` for a kept outside reference |
| asset reference (`ASSET_REF` kind) | the asset uuid as text |
| a tag | `{}` |

Any other field kind fails registration with `UNSUPPORTED_FIELD` unless the type is `TRANSIENT` or registers its
own hooks. A type can replace the default by filling a `component_serializer` (`writeJson`, `readJson`,
`remapEntities` function pointers); `make_default_serializer<T>()` returns the reflection-driven one for a C++
type.

| Function | Purpose |
|---|---|
| `write_archive_json(archive, writer)` | emits the document above; on failure (`ARCHIVE_CORRUPT`, or a component serialiser's error) the writer holds partial, unbalanced output: `reset()` or discard it before reuse |
| `read_archive_json(value, registry)` | parses it into an `entity_archive`; `SERIALIZATION_FAILED` with an `<entity index>/<component>/<field>` path on malformed input, `ARCHIVE_CORRUPT` on a bad reference |
| `write_component_json(info, component, writer)` / `read_component_json(info, component, value)` | one component instance through its serialiser (tools working on raw bytes) |
| `component_to_json(world, entity, componentId, writer)` | one live component; outside references as `{"external": ...}` |
| `component_from_json(world, entity, componentId, value)` | updates a live component (missing members keep their value) and stamps it as changed |

> [!NOTE]
> A component name no registered type resolves is kept **verbatim** in the built-in `unresolved_components`
> component (id 0, hidden in the editor) and written back under its original name, so a scene authored with a
> module that is not loaded today survives a load and save round trip. One `INFO` log per unknown name tells you
> it happened. The one exception: once that name is registered and the entity holds the live component, the live
> value is written and the preserved copy is dropped, so the name never appears twice in one entity. `TRANSIENT`
> components are skipped on save and ignored on load.

## Archetypes and chunks

You do not create or manage archetypes; the world does. You meet them when inspecting a world, in profilers and
in tests. `world.archetypes()` lists every archetype in creation order (pointers stay valid for the world's
lifetime); each `archetype` exposes its component set and storage read-only:

| Member | Purpose |
|---|---|
| `mask()`, `has(id)` | the component set |
| `entity_count()`, `chunk_count()`, `chunk_entity_count(chunkIndex)`, `chunk_entities(chunkIndex)` | how full it is and who is in it |
| `layout()` | the `chunk_layout`: `capacity` rows per chunk, `components` per column, `columnOffsets`, `columnSizes`, `column_of(id)`, `row_bytes()`, `block_end()` |
| `chunk_column(chunkIndex, id)`, `chunk_column_version(chunkIndex, id)` | read-only column start and its change stamp |
| `index()` | position in `world.archetypes()` |

Every chunk is `CHUNK_SIZE` (16 KiB) bytes, `CHUNK_ALIGNMENT` (64) aligned; columns start at multiples of
`COLUMN_ALIGNMENT` (16). The entity column sits first, then one column per non-tag component ordered by
descending alignment, and the capacity is the largest row count that fits: an empty archetype holds 2048 rows per
chunk, one with a 64-byte component 227. `chunk_layout::compute(mask, registry)` returns the layout for any
set; fewer than `SMALL_CAPACITY_WARNING` (8) rows per chunk logs a `WARN` because the archetype wastes space.

> [!TIP]
> Many small archetypes are fine; one archetype whose row is thousands of bytes is not. If an entity needs a
> 4 KiB blob, keep the blob behind an asset or a pool handle and store the handle in the component.

## API summary

| Signature (short) | Purpose |
|---|---|
| `static result<entity_archive> entity_archive::capture(const world &, span<const entity>, archive_options = {})` | copy out |
| `struct archive_options { bool keepExternalReferences = false; }` | capture options |
| `u32 entity_count()` / `span<const archive_entry> entries()` / `span<const byte> component_bytes(u32, component_id)` / `result<void> validate()` / `void push_entry(archive_entry)` | archive contents |
| `struct archive_entry { component_mask mask; vector<archive_component> components; }`; `struct archive_component { component_id id; vector<byte> bytes; }` | the entries |
| `result<entity> world::instantiate(shared_ptr<const entity_archive>, command_buffer * = nullptr)` | one copy |
| `result<void> world::instantiate_batch(shared_ptr<const entity_archive>, span<entity> outRoots, command_buffer * = nullptr)` | many copies |
| `result<void> write_archive_json(const entity_archive &, json::writer &)` / `result<entity_archive> read_archive_json(const json::value &, const component_registry &)` | archive JSON |
| `result<void> write_component_json(const component_info &, const void *, json::writer &)` / `read_component_json(info, void *, const json::value &)` | one component by bytes |
| `result<void> component_to_json(const world &, entity, component_id, json::writer &)` / `component_from_json(world &, entity, component_id, const json::value &)` | one live component |
| `component_serializer make_default_serializer<T>()`; `struct component_serializer { writeJson, readJson, remapEntities }` | custom serialisation |
| `struct unresolved_components { vector<unresolved_component> entries; }`; `UNRESOLVED_COMPONENTS_NAME` | preserved unknown data |
| `class entity_remap`: `resolve(entity)`, `remap(entity)`, `target(u32)`, `set(u32, entity)`, `size()`, `index_targets()` | local index to real entity, and back (`remap`); `index_targets()` makes the real-to-local lookups of `remap` a binary search |
| `span<archetype *const> world::archetypes()`; `archetype::mask()` / `layout()` / `entity_count()` / `chunk_count()` / `chunk_entities(u32)` / `chunk_column(u32, id)` | storage inspection |
| `struct chunk_layout`; `static result<chunk_layout> compute(const component_mask &, const component_registry &)`; `CHUNK_SIZE`, `CHUNK_ALIGNMENT`, `COLUMN_ALIGNMENT`, `SMALL_CAPACITY_WARNING` | layout |

## See also

- 📘 API: [Entities and components](entities-and-components.md) for reflected `entity` fields and `TRANSIENT`
- 📘 API: [Command buffers and observers](command-buffers-and-observers.md) for deferred instantiation
- 📘 API: [JSON](../core/json.md) and [Reflection](../core/reflection.md)
- 📘 API: [Glossary](../glossary.md)
- 🛠 Handbook: [ECS module](../../handbook/modules/ecs.md) for the chunk layout algorithm and the capture and
  instantiate internals

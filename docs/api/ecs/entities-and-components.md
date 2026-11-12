# Entities and components

📘 **Public API wiki** · 🛠 Handbook: [ECS module](../../handbook/modules/ecs.md)

How to define a component type, register it, create entities and attach data to them. Headers:
`conjure/ecs/entity.h` (the id), `conjure/ecs/component.h` (`CONJURE_COMPONENT`, ids, flags, masks),
`conjure/ecs/component_registry.h`, `conjure/ecs/world.h` (create, destroy, add, get, remove),
`conjure/ecs/entity_handle.h` (the object-style facade) and `conjure/ecs/component_type_desc.h` (for module
authors). Everything is in `namespace conjure::ecs`.

## At a glance

| You want to... | Use |
|---|---|
| Make a struct usable as a component | `CONJURE_REFLECT_STRUCT(T, "game.name", fields...)` then `CONJURE_COMPONENT(T, "game.name")` |
| Register it | `registry.register_component<T>()` once, before any world uses it |
| A marker with no data | an empty struct; it becomes a **tag** automatically |
| Create or destroy an entity | `world.create_entity()`, `world.destroy_entity(id)` |
| Attach, read, write, replace, detach a component | `world.add<T>(id, value)`, `world.get<T>(id)`, `world.set(id, value)`, `world.remove<T>(id)` |
| Ask without failing on a stale id | `world.is_alive(id)`, `world.has<T>(id)`, `world.get<T>(id)` (nullptr) |
| Hold "an entity plus its world" in one object | `world.handle(id)` returns an `entity_handle` |
| Keep a reference to one component of one entity | `handle.component<T>()` returns a `component_ref<T>` |
| Work by id instead of type (bindings, tools) | `add_raw`, `get_raw`, `has_raw`, `remove_raw` with a `component_id` |
| Export component types from a module | `describe_component<T>()` and `component_type_desc` |

## Quick example

```cpp
#include "conjure/core/reflect/reflect.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/world.h"

using namespace conjure;
using namespace conjure::ecs;

struct health { i32 value = 100; i32 maximum = 100; };
struct player_tag {};
CONJURE_REFLECT_STRUCT(health, "game.health", CONJURE_FIELD(value), CONJURE_FIELD(maximum));
CONJURE_COMPONENT(health, "game.health");
CONJURE_COMPONENT(player_tag, "game.player_tag");          // tags need no reflection

core::result<entity> spawn_player(world &game) {
    CONJURE_TRY_ASSIGN(const entity player, game.create_entity());
    CONJURE_TRY(game.add<health>(player));                   // default value: 100 / 100
    CONJURE_TRY(game.add<player_tag>(player));
    return player;
}

void apply_damage(world &game, entity target, i32 amount) {
    health *hitPoints = game.get<health>(target);           // nullptr when dead or missing
    if (hitPoints != nullptr) {
        hitPoints->value -= amount;
    }
}
```

The registry and the world are created once (`component_registry registry = {}; world game(registry);`) and the
registry must outlive every world built on it.

## Defining a component type

A component is a plain struct: no virtual functions, no base class, member initialisers for defaults. Two macros
at global namespace scope make it a component:

- `CONJURE_REFLECT_STRUCT(T, "game.name", CONJURE_FIELD(a), ...)` describes the fields so the serialiser, the
  editor and scripts can see them (📘 API: [Reflection](../core/reflection.md)).
- `CONJURE_COMPONENT(T, "game.name")` gives the type its **registered name**. Use the same string for both.

The name is the identity: two registries, two worlds or two shared libraries that register `"game.health"` agree
on the same `component_id`, and re-registering the same name with the same layout returns the existing id.

| Kind of type | What you write | What you get |
|---|---|---|
| Plain data (`f32`, `i32`, `string_id`, math types, `entity`, nested reflected structs, `std::array`) | the two macros | `TRIVIAL`: copied with `memcpy`, zero-cost moves between archetypes |
| Empty struct | `CONJURE_COMPONENT` only | `TAG`: occupies no bytes, lives only in the entity's component mask |
| Owns resources (`std::string`, `small_vector`) | the two macros | non-trivial: constructed, moved and destroyed through its special members |
| Runtime-only data that must never be saved | `register_component<T>(component_flags::TRANSIENT)` | skipped by archives and JSON; reflection is optional |

Limits: at most 1024 component types per registry (`MAX_COMPONENTS`), 4096 bytes per component
(`MAX_COMPONENT_SIZE`; put big data behind an asset handle) and an alignment that is a power of two no greater
than 64 (`MAX_COMPONENT_ALIGNMENT`). Non-trivial types may need at most 16-byte alignment.

> [!IMPORTANT]
> `entity` is `{u32, u32}` and aligns to 4, so a component may place entity fields at any 4-byte aligned offset;
> no `alignas(8)` is needed. Registration fails with `UNSUPPORTED_FIELD` if an entity field sits at an offset that
> breaks the component's own alignment, because the reflection description cannot be built. Reflected entity fields are what makes prefabs rewire references on
> instantiation (📘 API: [Archives and serialisation](archives-and-serialization.md)).

> [!WARNING]
> A reflected field of a kind the serialiser does not know fails registration with `UNSUPPORTED_FIELD` unless the
> type is `TRANSIENT` or supplies its own `component_serializer` hooks. The supported kinds are listed on the
> archives page.

## Registering types: `component_registry`

`component_registry` owns the description of every registered type and hands out dense ids. It is non-copyable,
non-movable, thread-safe, and must be destroyed after every world created on it.

| You want to... | Use |
|---|---|
| Register a C++ type | `register_component<T>(flags = NONE)` returns `result<component_id>` |
| The id of a type | `id_of<T>()` (cached; `component_id::INVALID` when not registered) |
| The id of a name | `find(string_id)` |
| Everything known about an id | `info(id)` returns `const component_info &` (name, size, alignment, flags, reflection) |
| Enumerate (editor, tools) | `types()`, `count()`, `is_registered(id)` |
| Know when a type appears | `on_registered()` is an `event<const component_info &>` |
| Ask whether any world still uses a type | `has_instances(id)` |

Errors: `NAME_CONFLICT` (same name, different layout or flags; for a runtime-described type also different
fields), `COMPONENT_LIMIT_REACHED`, `COMPONENT_TOO_LARGE`, `BAD_ALIGNMENT`, `UNSUPPORTED_FIELD`, and
`INVALID_ARGUMENT` for a hand-filled `component_type_desc` whose size and `TAG` flag disagree. Every registry
starts with one built-in type at id 0,
`unresolved_components`, which preserves data saved by modules that are not loaded.

> [!TIP]
> Register all component types at start-up (module entry, game initialisation) on the owner thread, then treat
> the registry as read-only. `id_of<T>()` is one atomic load after the first lookup, so there is no reason to cache
> ids yourself.

## Entities

`entity` is `{u32 index; u32 generation;}`: 8 bytes, trivially copyable, comparable, hashable (`std::hash`) and
printable with `std::format("{}", id)` as `index:generation`. `NULL_ENTITY` is all zeros and `is_null()` tests
for it; zero-initialised memory therefore holds null entities for free. `to_u64()` / `from_u64()` pack it into
one word for scripts.

| You want to... | Use |
|---|---|
| Create one empty entity | `create_entity()` returns `result<entity>` |
| Create one entity with components already in place | `create_entity(mask)` with a `component_mask` built from `registry.id_of<T>()` |
| Create many at once | `create_entities(mask, std::span<entity> out)` fills `out` chunk by chunk |
| Destroy | `destroy_entity(id)`; `ENTITY_NOT_ALIVE` for a dead or stale id |
| Ask | `is_alive(id)`, `entity_count()`, `mask_of(id)` (empty for a dead entity) |

A destroyed index is reused with a higher generation, so every copy of the old id stops matching. The read-only
calls (`is_alive`, `has`, `get`, `mask_of`) answer false, nullptr or empty for a stale id and never assert; the
mutating calls return `ENTITY_NOT_ALIVE`.

> [!WARNING]
> Create, destroy, add and remove are **structural** and must run on the world's owner thread while no view is
> iterating. Inside a `for_each` callback, a system or an observer they fail with `STRUCTURE_LOCKED`; record the
> change in a `command_buffer` instead (📘 API: [Command buffers and observers](command-buffers-and-observers.md)).
> `set`, `get`, `has` and `is_alive` are not structural.

## Components on an entity

| You want to... | Use | Notes |
|---|---|---|
| Attach with a value | `add<T>(id, value = {})` returns `result<T *>` | `COMPONENT_ALREADY_PRESENT` if it is there; nullptr result for a tag, or when an `ADDED` observer's deferred work removed it again or destroyed the entity |
| Read | `get<T>(id)` on a `const world &`, or `get<const T>(id)` on any world, returns `const T *` | nullptr when dead or absent; never marks the column as changed |
| Write in place | `get<T>(id)` with a non-const `T` on a non-const world returns `T *` | marks the chunk column as changed |
| Replace the whole value | `set(id, value)` | `COMPONENT_NOT_PRESENT` when absent; not structural |
| Ask | `has<T>(id)` | false for a dead entity |
| Detach | `remove<T>(id)` | `COMPONENT_NOT_PRESENT` when absent |

> [!WARNING]
> A `T *` from `add` or `get` is valid only until the next structural change anywhere in the world: adding a
> component to any entity can move rows. Fetch the pointer, use it, drop it. Keep the `entity` (or a
> `component_ref<T>`) instead and look the component up again.

> [!TIP]
> Call `get<const T>` (or `get<T>` through a `const world &`, or `component_ref::read()`) when you only read. A
> non-const `T` stamps the chunk as changed, which makes every `changed<T>` filter downstream revisit it.

## The handle facade: `entity_handle` and `component_ref`

`world.handle(id)` returns an `entity_handle`: the entity plus its world in one copyable value that re-validates
on every call, so code that keeps handles around (gameplay scripts, UI) reads a dead entity as dead.

```cpp
#include "conjure/ecs/entity_handle.h"

void heal_fully(world &game, entity id) {
    entity_handle handle = game.handle(id);
    if (!handle.is_alive()) {
        return;                                              // a stale id touches nothing
    }
    component_ref<health> hitPoints = handle.component<health>();
    if (health *value = hitPoints.write(); value != nullptr) {
        value->value = value->maximum;
    }
}
```

| `entity_handle` | `component_ref<T>` |
|---|---|
| `id()`, `owner()` (the `world *`), `is_alive()` | `valid()` (alive and present), `owner()` (the entity) |
| `add<T>(value)` returns `result<component_ref<T>>` | `read()` returns `const T *`, never stamps |
| `component<T>()` returns a `component_ref<T>` (check `valid()`) | `write()` returns `T *` and stamps the column |
| `has<T>()`, `remove<T>()`, `destroy()` | `set(value)` returns `result<void>` |

A `component_ref` follows the component when the entity moves to another archetype, turns invalid when the
component is removed and valid again when it is re-added. Both types are small values: pass them by value.

## Untyped access by `component_id`

Bindings, serialisers and editor tools work with ids instead of types: `add_raw(id, componentId, bytes = {})`
(empty bytes mean the default value) returns `result<void *>` (nullptr for a tag, or when an `ADDED` observer's
deferred work removed the component again or destroyed the entity), `set_raw` lives on command buffers, `get_raw(id,
componentId, forWrite)` returns `void *`, `has_raw`, `remove_raw`, `instance_count(componentId)` counts holders in
this world and `remove_from_all(componentId)` strips a type from every entity (module unload, hot reload). The
byte span passed to `add_raw` must be a complete object image of the registered size.

`component_mask` is the set of ids an entity has: `set(id)`, `clear(id)`, `test(id)`, `contains_all`,
`contains_any`, `none()`, `count()`, `for_each_set(fn)` and the bit operators. `mask_of(id)` returns it, and
`create_entity(mask)` places an entity straight into its final archetype.

## World housekeeping

`world(registry, world_desc desc = {})` takes a `name`, an `initialEntityCapacity` (records reserved up front),
`chunksPerSlab` and `maxChunkSlabs` (0 = unlimited; beyond the cap structural calls fail with `OUT_OF_MEMORY`
and change nothing). `stats()` returns `world_stats{aliveEntities, archetypes, chunks, chunkBytes, version}`,
`validate()` runs a full invariant check (tests call it in their fixture's destructor) and `clear()` destroys every
entity while keeping archetypes, observers and the id space, so stale handles stay stale. `name()`, `registry()`
and `archetypes()` are read-only accessors.

## For module authors: `component_type_desc`

A module cannot hand the module system a `component_registry` call, so it exports trivially copyable records
instead. `describe_component<T>(flags = NONE)` fills a `component_type_desc` (name, size, alignment, flags,
lifecycle hooks, reflection pointer) for a type named with `CONJURE_COMPONENT`; the engine applies it on attach
through `registry.register_described(desc)`, deep-copying the reflection description so the module may unload.
The record's `EXTENSION_POINT` is `"ecs.component"`. `system_extension_desc` (`"ecs.system"`) is the matching
record for a system: name, phase, flags, `reads`/`writes` as arrays of component **names**, `after`/`before`
as arrays of system names, and a plain function pointer `run(system_context &)`. Both records are described in
🛠 Handbook: [Module system](../../handbook/module-system/index.md).

Scripts register components without a C++ type through `register_dynamic(dynamic_component_desc)` (name, size,
alignment 1 to 16, a field list of primitive, math or `entity` kinds, optional default bytes); the result is always
`DYNAMIC | TRIVIAL`, accessed with the raw functions above. Registering a known name again returns the same id
only when the fields match too: other fields (order, names, types or offsets) at the same size are
`NAME_CONFLICT`. `redefine_dynamic(id, desc)` changes the layout and fields while keeping the id, and refuses with
`COMPONENT_IN_USE` while a world still holds an instance. The hot-reload sequence
is on the 🛠 Handbook: [ECS module](../../handbook/modules/ecs.md) page.

## API summary

| Signature (short) | Purpose |
|---|---|
| `struct entity { u32 index; u32 generation; }` with `is_null()`, `to_u64()`, `from_u64()`; `NULL_ENTITY` | the id |
| `CONJURE_COMPONENT(Type, "module.name")` | names a component type (global scope) |
| `enum class component_flags { TAG, TRIVIAL, DYNAMIC, TRANSIENT, EDITOR_HIDDEN }` | type properties; `TAG`/`TRIVIAL` are derived from the type |
| `struct component_id { u16 value; }` with `is_valid()`; `class component_mask` | dense type id and id set |
| `result<component_id> component_registry::register_component<T>(flags = {})` | registers a C++ type |
| `component_id id_of<T>()` / `find(string_id)` / `const component_info &info(id)` / `types()` | lookups |
| `result<component_id> register_dynamic(const dynamic_component_desc &)` / `result<void> redefine_dynamic(id, desc)` | script-defined types |
| `result<component_id> register_described(const component_type_desc &)` | module extension records |
| `world(const component_registry &, world_desc = {})` | a world; registry must outlive it |
| `result<entity> create_entity(const component_mask & = {})` / `result<void> create_entities(mask, span<entity>)` / `result<void> destroy_entity(entity)` | lifecycle |
| `bool is_alive(entity)` / `u32 entity_count()` / `component_mask mask_of(entity)` | queries |
| `result<T *> add<T>(entity, T = {})` / `result<void> remove<T>(entity)` / `result<void> set(entity, T)` | typed structural and value calls |
| `T *get<T>(entity)` / `const T *get<T>(entity) const` / `bool has<T>(entity)` | typed access |
| `result<void *> add_raw(entity, component_id, span<const byte> = {})` / `remove_raw` / `get_raw(entity, id, bool forWrite)` / `has_raw` / `instance_count(id)` / `remove_from_all(id)` | untyped access |
| `entity_handle handle(entity)` / `world_stats stats()` / `result<void> validate()` / `void clear()` | facade and housekeeping |
| `class entity_handle`: `id()`, `is_alive()`, `add<T>()`, `component<T>()`, `has<T>()`, `remove<T>()`, `destroy()` | object-style facade |
| `class component_ref<T>`: `valid()`, `read()`, `write()`, `set(T)` | re-resolving component reference |
| `component_type_desc describe_component<T>(flags = {})`; `struct system_extension_desc` | module extension records |

## See also

- 📘 API: [Queries, views and systems](queries-views-and-systems.md) to iterate what you created
- 📘 API: [Command buffers and observers](command-buffers-and-observers.md) for structural changes from systems
- 📘 API: [Reflection](../core/reflection.md) for `CONJURE_REFLECT_STRUCT`; [Errors and results](../core/errors-and-results.md) for `result<T>` and `CONJURE_TRY`
- 📘 API: [Glossary](../glossary.md)
- 🛠 Handbook: [ECS module](../../handbook/modules/ecs.md)

# Entities, components and systems
<!-- doc-template:wiki-index -->
[API](../index.md)

<!-- doc-content:purpose:start -->
The ECS is where your game's objects live. An **entity** is a small id, a **component** is a plain struct of
data attached to an entity, and a **system** is a function that runs once per frame over every entity that has
a given set of components. There is no base class to inherit from and no virtual `update()`: you describe your
data, and the engine stores it so that systems stream through it quickly and in parallel. Everything on these
pages lives in `namespace conjure::ecs`; the umbrella header `conjure/ecs/ecs.h` pulls in the whole module and is
meant for tests and samples, so include the individual headers named on each page in game code.
<!-- doc-content:purpose:end -->

<!-- doc-content:body:start -->
## The mental model in five sentences

1. A `component_registry` knows every component type by **name** (`"game.position"`). You register your types
   once at start-up.
2. A `world` holds entities. You create an entity, `add` components to it, `get` them back and `destroy` it; a
   stale id is detected and reported, never dereferenced.
3. A `view<...>` iterates every entity holding a set of components. `const T` in the list means "read",
   plain `T` means "write".
4. A **system** is a named function registered with the `scheduler` for one **phase** of the frame
   (`FIXED_UPDATE`, `UPDATE`, `LATE_UPDATE`,...). The scheduler runs systems whose data does not overlap at the
   same time, on the job system.
5. While a system runs, the world is **locked**: you cannot create or destroy entities or add and remove components
   directly. You record those changes in a `command_buffer` and the scheduler applies them when the stage ends.

Under the hood, entities with the same component set share an **archetype** and sit side by side in 16 KiB
chunks, one column per component. You never have to think about that to use the ECS, but every page shows the
"chunk" escape hatch second for when you need to process thousands of entities as tightly as possible.

## Where to go next

| Page | Read it when you want to... |
|---|---|
| [Entities and components](entities-and-components.md) | define a component type, register it, create entities, add, get, set and remove components, use the `entity_handle` facade, publish component types from a module |
| [Queries, views and systems](queries-views-and-systems.md) | iterate entities with `view`, go chunk by chunk with `chunk_view`, write a system, pick its phase, understand what runs in parallel and what the `changed<T>` filter sees |
| [Command buffers and observers](command-buffers-and-observers.md) | spawn or destroy from inside a system, react to a component being added or removed, understand the iteration lock and the owner thread, hash a world in a determinism test |
| [Archives and serialisation](archives-and-serialization.md) | copy a group of entities (prefabs, copy and paste, undo), instantiate it many times, save it as JSON and load it back, look at archetypes and chunks |

Read them in that order the first time; each builds on the one before.

## Quick tour

```cpp
#include "conjure/core/reflect/reflect.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/world.h"

using namespace conjure;
using namespace conjure::ecs;

struct position { f32 x = 0.0f; f32 y = 0.0f; f32 z = 0.0f; };
struct velocity { f32 x = 0.0f; f32 y = 0.0f; f32 z = 0.0f; };
CONJURE_REFLECT_STRUCT(position, "game.position", CONJURE_FIELD(x), CONJURE_FIELD(y), CONJURE_FIELD(z));
CONJURE_REFLECT_STRUCT(velocity, "game.velocity", CONJURE_FIELD(x), CONJURE_FIELD(y), CONJURE_FIELD(z));
CONJURE_COMPONENT(position, "game.position");
CONJURE_COMPONENT(velocity, "game.velocity");

core::result<void> run_once() {
    component_registry registry = {};
    CONJURE_TRY(registry.register_component<position>());
    CONJURE_TRY(registry.register_component<velocity>());
    world game(registry);                                   // the registry must outlive the world

    CONJURE_TRY_ASSIGN(const entity player, game.create_entity());
    CONJURE_TRY(game.add<position>(player));
    CONJURE_TRY(game.add<velocity>(player, velocity{1.0f, 0.0f, 0.0f}));

    game.view<const velocity, position>().for_each([](entity, const velocity &v, position &p) {
        p.x += v.x;
    });
    return core::result<void>::success();
}
```

Three things to notice: every call that can fail returns a `result` (the ECS never throws, see
📘 API: [Errors and results](../core/errors-and-results.md)); a component is a plain struct described once with
`CONJURE_REFLECT_STRUCT` (📘 API: [Reflection](../core/reflection.md)) and named once with `CONJURE_COMPONENT`;
and the view's callback receives references straight into the storage, so there is nothing to copy back.

> [!NOTE]
> The ECS depends only on core. The engine module will own the registry, the world and the scheduler
> and call `run_phase` for you in frame order; today you build them yourself, as the tests and the examples do.

## Terms used on these pages

| Term | Meaning |
|---|---|
| Entity | An 8-byte id (`index` + `generation`); `NULL_ENTITY` is all zeros and never alive |
| Component | A registered plain struct attached to an entity; a **tag** is an empty component that only marks |
| Archetype | The set of components an entity has; every entity with the same set is stored together |
| Chunk | A 16 KiB block of one archetype holding one column per component; the unit of iteration and of parallel work |
| Phase | One slot of the frame (`FRAME_BEGIN`... `FRAME_END`) a system is registered for |
| Stage | A group of systems in one phase that the scheduler runs concurrently because their data does not overlap |
| Structural change | Create, destroy, add or remove: anything that moves an entity between archetypes |
| Owner thread | The thread a world was created on; structural changes happen there, reads happen anywhere while locked |
| Version | A counter the world bumps before every system run and structural change; writes stamp it on a chunk column so `changed<T>` filters can skip untouched chunks |

The full engine-wide list is the 📘 API: [Glossary](../glossary.md).

## Supporting headers

| Include path | Purpose |
|---|---|
| `conjure/ecs/ecs_error.h` | Declares `ecs_error`, the module failure codes carried by `core::error` and `core::result<T>`. |
| `conjure/ecs/log_category.h` | Declares the module log category used to filter its diagnostics through core logging. |
<!-- doc-content:body:end -->

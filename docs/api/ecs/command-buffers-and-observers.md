# Command buffers and observers

📘 **Public API wiki** · 🛠 Handbook: [ECS module](../../handbook/modules/ecs.md)

How to change the structure of a world from places where direct changes are refused (systems, view callbacks,
observers, job threads), how to react when a component appears or disappears, what the iteration lock and the
owner thread mean for you, and how to prove two runs produced the same world. Headers:
`conjure/ecs/command_buffer.h`, `conjure/ecs/observer.h` and `conjure/ecs/world_hash.h`; the lock lives in
`conjure/ecs/world.h`. Everything is in `namespace conjure::ecs`.

## At a glance

| You want to... | Use |
|---|---|
| Spawn, destroy, add or remove from inside a system | `context.commands().create_entity()`, `.destroy_entity(id)`, `.add<T>(id, value)`, `.remove<T>(id)` |
| The same from your own loop or thread | `command_buffer commands(world);` then `world.flush(commands)` on the owner thread |
| Use a not-yet-created entity in later commands | the `entity` returned by `commands.create_entity()` is real at once |
| Run code when a component is added or removed | `world.observe(id, component_event::ADDED, fn)` / `REMOVED` |
| Run code before an entity is destroyed | `world.observe_destroyed(fn)` |
| Make an observer trigger more changes | enqueue into the `command_buffer &deferred` it receives |
| Hand a span to code that must not see it move | hold `world.lock_iteration()` while it is in use |
| Check two worlds (or two runs) are identical | `world_hash(world)` |

## Quick example

```cpp
#include "conjure/ecs/command_buffer.h"
#include "conjure/ecs/scheduler.h"
#include "conjure/ecs/system.h"

using namespace conjure;
using namespace conjure::ecs;

system_desc make_reaper() {
    return make_system<const health>(core::string_id("game.reap"), system_phase::LATE_UPDATE,
      [](system_context &context) {
          context.view<const health>().for_each([&](entity id, const health &hitPoints) {
              if (hitPoints.value <= 0) {
                  context.commands().destroy_entity(id);         // applied when the stage ends
              }
          });
      });
}

core::result<void> spawn_batch(world &game, u32 count) {
    command_buffer commands(game);
    for (u32 index = 0; index < count; ++index) {
        const entity spawned = commands.create_entity();           // a real id, usable right away
        commands.add<position>(spawned, position{static_cast<f32>(index), 0.0f, 0.0f});
        commands.add<health>(spawned);
    }
    const flush_report report = game.flush(commands);              // owner thread, world unlocked
    if (report.skipped != 0) {
        return make_error(ecs_error::OUT_OF_MEMORY, "some spawns could not be placed");
    }
    return core::result<void>::success();
}
```

## Why deferred changes exist

A view hands you references into chunk columns. If a callback could destroy an entity or add a component, rows
would move under every other reference in flight, and a system on another thread would read freed memory. So the
world is **locked** during iteration and during system stages: structural calls (`create_entity`,
`create_entities`, `destroy_entity`, `add`, `remove`, `add_raw`, `remove_raw`, `remove_from_all`, `flush`,
`clear`, `instantiate` without a buffer, observer registration) fail with `STRUCTURE_LOCKED` and assert in Debug.
`set`, `get`, `has`, `is_alive` and `mask_of` are not structural and keep working.

A `command_buffer` records the change now and the world applies it later, in one pass, when nobody is iterating.

## Recording: `command_buffer`

`command_buffer commands(world);` binds a buffer to one world. It is move-only and single-threaded: one buffer
belongs to one thread at a time, but different threads may each own a buffer on the same world (that is what
`parallel_for_chunks` does for you).

| Member | Records |
|---|---|
| `create_entity()` | a new entity; returns its real `entity` immediately (`NULL_ENTITY` when the id space is exhausted) |
| `destroy_entity(id)` | destruction; wins over every other command on that entity in this buffer |
| `add<T>(id, value = {})` | adding `T` with a value (moved in); on an entity that already has `T` it acts like `set` |
| `set<T>(id, value)` | replacing the value of a `T` the entity has at flush |
| `remove<T>(id)` | removing `T`; cancels a pending `add<T>` in this buffer |
| `add_raw(id, componentId, bytes = {})`, `set_raw(...)`, `remove_raw(id, componentId)` | the same by id |
| `instantiate(archive)` | one copy of an `entity_archive`; the root id is returned now |
| `append(std::move(other))` | every command of `other`, in order, behind this buffer's |
| `empty()`, `command_count()`, `reserved_count()`, `clear()`, `target()` | inspection and reset |

The entity from `create_entity()` is not alive yet (`is_alive` is false, `get` returns nullptr) but it is a real,
unique id: later commands in **any** buffer of the same world may use it, scripts may hold it, and observers will
see it once the flush runs. Each buffer reserves ids in blocks of `RESERVE_BLOCK` (64), so creating from job
threads costs one lock per 64 entities.

> [!WARNING]
> A buffer destroyed with commands still in it logs a `WARN`, destroys the payloads and applies nothing. Flush
> it, or `clear()` it on purpose. Typed `add`/`set`/`remove` of a type the registry does not know assert and
> record nothing.

## Applying: `world::flush`

`world.flush(commands)` runs on the owner thread while the world is unlocked and returns
`flush_report{applied, skipped}`; the buffer is empty afterwards. The scheduler calls it for every system's buffer
at the end of each stage, so commands recorded in stage 1 are visible to systems in stage 2 of the same phase.

What a flush does with the commands on one entity, whatever their order:

| Situation | Outcome |
|---|---|
| Several adds and removes | **one** move to the final archetype, observers fire for the net difference only |
| `add<T>` then `remove<T>` | cancelled; the payload is destroyed, nothing moves |
| `remove<T>` then `add<T>` | the component ends up present with the new value |
| `destroy_entity` anywhere | the entity is destroyed; every other command on it is dropped |
| `set<T>` on an entity without `T` at flush | skipped and counted |
| any command on a dead or stale entity | skipped, counted in `skipped`, logged at `DEBUG` |
| placement fails (`ARCHETYPE_TOO_LARGE`, `OUT_OF_MEMORY`) | that entity's commands are all counted as skipped; a reserved-only id is released |

Order between entities is the recording order; destroys run last; archive instantiations run before the commands
that use their root ids.

> [!TIP]
> Do not keep a flush's `applied` count as a success test for scripts: a stale id is a normal event, and
> `skipped` tells you how many commands it swallowed. Check `is_alive` before recording when it matters.

## Reacting: observers

Observers run **synchronously on the owner thread inside the structural call** that triggered them: direct API,
flush or archive instantiation alike.

```cpp
#include "conjure/core/reflect/reflect.h"
#include "conjure/ecs/observer.h"
#include "conjure/ecs/world.h"

#include <algorithm>

struct link { entity target = {}; entity other = {}; };                  // entity fields align to 4
CONJURE_REFLECT_STRUCT(link, "game.link", CONJURE_FIELD(target), CONJURE_FIELD(other));
CONJURE_COMPONENT(link, "game.link");

// Fires after the value is in place, so the component can be read and adjusted.
observer_id clamp_health(world &game, component_id healthId) {
    return game.observe(healthId, component_event::ADDED,
      [](world &owner, entity target, component_id, command_buffer &) {
          health *hitPoints = owner.get<health>(target);
          hitPoints->value = std::min(hitPoints->value, hitPoints->maximum);
      });
}

// Destroying an entity destroys whatever its link points at: a chain of flushes, not recursion.
observer_id cascade_links(world &game) {
    return game.observe_destroyed([](world &owner, entity target, command_buffer &deferred) {
        const link *current = owner.get<link>(target);
        if (current != nullptr && !current->target.is_null()) {
            deferred.destroy_entity(current->target);
        }
    });
}
```

Keep the returned `observer_id`s to `unobserve()` them later (a module's detach, a level's teardown).

| Member | Purpose |
|---|---|
| `observe(componentId, component_event::ADDED, fn)` | `fn(world &, entity, component_id, command_buffer &deferred)` after the component exists |
| `observe(componentId, component_event::REMOVED, fn)` | the same before the component is destroyed (the value is still readable) |
| `observe_destroyed(fn)` | `fn(world &, entity, command_buffer &deferred)` before an entity's components go |
| `unobserve(observer_id)` | stops delivery; stale ids are ignored |

The `deferred` buffer is the only way an observer may change structure. The world drains it in a loop once the
outermost structural call has finished, so an observer that destroys a child, whose observer destroys a
grandchild, costs one flush per level and never recurses. An observer may read any component (`get` const),
write values (`set`, non-const `get`) and register or remove observers.

> [!CAUTION]
> Calling `destroy_entity`, `add`, `remove` or `flush` on the world from inside an observer asserts and returns
> `STRUCTURE_LOCKED`; the change does not happen. Always go through `deferred`.

> [!NOTE]
> Observers fire for the **difference** between the old and the new component set. A `set` fires nothing; a
> flush that adds and removes the same component fires nothing; `world::clear()` fires `REMOVED` and destroyed
> observers for every entity; the world's destructor fires nothing.

## The iteration lock and the owner thread

`world.lock_iteration()` returns an `iteration_scope` (RAII, movable, `release()` to end it early,
`is_active()`) that keeps the world locked until it dies; `is_locked()` tells. Views, `for_each_chunk` and system
stages take one for you. Take one yourself when you hand chunk spans to code outside a callback (a scripting
`Span<T>`, a render extraction buffer) so nothing can move the rows while they are in use.

Threading rules, in one table:

| Operation | Where |
|---|---|
| Structural calls, `flush`, `observe`, `run_phase`, `clear`, `set_owner_thread` | owner thread, world unlocked (`WRONG_THREAD` / `STRUCTURE_LOCKED` otherwise) |
| `is_alive`, `has`, `mask_of`, `get` const, reading and writing columns you were handed | any thread while the world is locked |
| `lock_iteration()`, `version()`, `bump_version()` | any thread |
| `command_buffer` use | one thread at a time per buffer; `create_entity()` is safe from job threads |
| `world_hash`, `entity_archive::capture`, `validate()`, `stats()` | any time no structural change is in flight |

The owner thread is captured when the world is constructed and `set_owner_thread(id)` moves it while unlocked.

## Determinism tests: `world_hash`

`world_hash(const world &)` returns a `u64` over every alive entity and every component value, visiting
archetypes in an order that does not depend on when they were created. Equal worlds hash equal; one flipped float
bit changes the result. It is the tool for "serial and parallel runs agree" tests: run the same systems with
`scheduler_desc{parallelSystems = false, parallelChunks = false}` and with a job system, compare the hashes.
Trivially copyable components contribute their reflected fields (padding never takes part), non-trivial ones
their serialised JSON, tags their name only. An asset reference field contributes its uuid alone, never its bound
slot, so the hash is the same in every process and whether or not references are bound yet. Entity ids are part of
the hash: ids created through command buffers from parallel chunks or from systems that run concurrently are not
reproducible between runs, so create from `EXCLUSIVE` or `MAIN_THREAD` systems when hashes are compared across
runs. Call it while no structural change is in flight.

## API summary

| Signature (short) | Purpose |
|---|---|
| `command_buffer(world &)`; move-only | a recording bound to one world |
| `entity create_entity()` / `void destroy_entity(entity)` | lifecycle commands |
| `void add<T>(entity, T = {})` / `set<T>(entity, T)` / `remove<T>(entity)` | typed component commands |
| `void add_raw(entity, component_id, span<const byte> = {})` / `set_raw(entity, id, bytes)` / `remove_raw(entity, id)` | untyped commands |
| `result<entity> instantiate(shared_ptr<const entity_archive>)` | deferred archive copy |
| `void append(command_buffer &&)` / `bool empty()` / `u32 command_count()` / `u32 reserved_count()` / `void clear()` / `world &target()` | buffer management |
| `flush_report world::flush(command_buffer &)`; `struct flush_report { u32 applied; u32 skipped; }` | applying |
| `enum class component_event { ADDED, REMOVED }` | observer events |
| `observer_id world::observe(component_id, component_event, component_observer_fn)` / `observe_destroyed(entity_observer_fn)` / `void unobserve(observer_id)` | observers |
| `iteration_scope world::lock_iteration()` / `bool is_locked()`; `iteration_scope::release()`, `is_active()` | the lock |
| `std::thread::id owner_thread()` / `result<void> set_owner_thread(std::thread::id)` | ownership |
| `u64 world_hash(const world &)` | deterministic state hash |

## See also

- 📘 API: [Queries, views and systems](queries-views-and-systems.md) for `context.commands()` and per-grain buffers
- 📘 API: [Archives and serialisation](archives-and-serialization.md) for `instantiate` through a buffer
- 📘 API: [Logging and assertions](../core/logging-and-assertions.md) for what an assert means in each build
- 📘 API: [Glossary](../glossary.md)
- 🛠 Handbook: [ECS module](../../handbook/modules/ecs.md) for the flush plan and the observer drain loop

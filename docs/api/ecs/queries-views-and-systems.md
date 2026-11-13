# Queries, views and systems

📘 **Public API wiki** · 🛠 Handbook: [ECS module](../../handbook/modules/ecs.md)

How to iterate entities and how to turn that iteration into a system the engine runs every frame. Headers:
`conjure/ecs/view.h` (typed iteration), `conjure/ecs/query.h` and `conjure/ecs/chunk_view.h` (untyped,
chunk-by-chunk access), `conjure/ecs/system.h` (`system_desc`, `system_context`, `make_system`),
`conjure/ecs/system_phase.h` (the frame phases and `frame_info`) and `conjure/ecs/scheduler.h`. Everything is in
`namespace conjure::ecs`.

## At a glance

| You want to... | Use |
|---|---|
| Visit every entity with components A and B | `world.view<const A, B>().for_each([](entity, const A &, B &) {...})` |
| Make a component optional | `maybe<C>` in the list; the callback receives `C *` (null when absent) |
| Require a tag, exclude a component | `.with<Tag>()`, `.without<T>()` |
| Visit only chunks written since a version | `.changed<T>(sinceVersion)` |
| Process a whole column at once | `.for_each_chunk([](chunk_view &chunk) {...})` and `chunk.get<T>()` |
| Count | `.entity_count()` |
| Run the iteration every frame | wrap it in a system: `make_system<const A, B>(name, phase, run)` and `scheduler.add_system(...)` |
| Spread one system over worker threads | `context.parallel_for_chunks(query, fn)` or `view.parallel_for_each_chunk(context, fn)` |
| Order two systems | `desc.after.push_back(name)` / `desc.before.push_back(name)` |

## Quick example

```cpp
#include "conjure/ecs/view.h"
#include "conjure/ecs/world.h"

#include <algorithm>

using namespace conjure;
using namespace conjure::ecs;

void integrate(world &game, f32 deltaSeconds) {
    game.view<const velocity, position>().for_each([&](entity, const velocity &v, position &p) {
        p.x += v.x * deltaSeconds;
        p.y += v.y * deltaSeconds;
    });
}

void regenerate_players(world &game) {
    game.view<health, maybe<const velocity>>().with<player_tag>().for_each(
      [](entity, health &hitPoints, const velocity *speed) {
          if (speed == nullptr) {                                  // standing still regenerates
              hitPoints.value = std::min(hitPoints.value + 1, hitPoints.maximum);
          }
      });
}
```

`position`, `velocity`, `health` and `player_tag` are the component types defined on 📘 API:
[Entities, components and systems](index.md) and [Entities and components](entities-and-components.md).

## Views

`view<Pack...>` is a typed iterator over every entity holding the required components of its pack. The pack also
declares the access: `const T` reads, `T` writes (and marks the column as changed), `maybe<T>` / `maybe<const T>`
is optional. `world.view<Pack...>()` builds one; builder calls return the same view so they chain. A tag has no
data, so it belongs in `with<Tag>()`: a tag in the pack, plain or as `maybe<Tag>`, is a compile error.

| Member | Purpose |
|---|---|
| `with<Tag>()` | requires a component without receiving it (typically a tag) |
| `without<T>()` | excludes entities holding `T` |
| `changed<T>(sinceVersion)` | visits only chunks whose `T` column was written after `sinceVersion` |
| `for_each(fn)` | `fn(entity, Pack&...)` per entity; `maybe<T>` arrives as `T *` |
| `for_each_chunk(fn)` | `fn(chunk_view &)` per chunk |
| `parallel_for_each_chunk(context, fn)` | `fn(chunk_view &, command_buffer &)` per chunk on the job system; needs `conjure/ecs/system.h` |
| `entity_count()` | matching entities |
| `access()` | the derived `view_access{reads, writes}` masks |
| `underlying()` | the `query` behind the view |
| `is_valid()` | false when the pack or a builder call named an unregistered type: such a view matches nothing |

> [!WARNING]
> Iteration locks the world. Inside the callback, `create_entity`, `destroy_entity`, `add` and `remove` fail with
> `STRUCTURE_LOCKED` (and assert in Debug). Record them in a `command_buffer` and flush it after the loop, or let
> the scheduler flush it for you (📘 API: [Command buffers and observers](command-buffers-and-observers.md)).

> [!TIP]
> Mark everything you do not write as `const`. Only non-const columns are stamped as changed, and the scheduler
> uses the same constness to decide which systems can run side by side.

A view is a cheap value: it caches the archetypes it matched and refreshes incrementally, but you may also
build it fresh each frame.

## Chunk by chunk: `query` and `chunk_view`

Views are built on `query`, an untyped match described by a `query_desc` (`with`, `without`, `optional`,
`writes`, `changed` masks and `sinceVersion`) that `world.create_query(desc)` returns. Matching uses `with`,
`without` and the `changed` filter; `optional` and `writes` only record what a view's pack reads optionally and
writes (stamping follows the constness of each span you take). Its `for_each_chunk(fn)`
hands out a `chunk_view` per non-empty matching chunk; `chunk_count()`, `entity_count()`, `chunk(index)` (a flat
index for bindings; hold `world.lock_iteration()` while using the result), `matches()`, `desc()`, `reset(desc)`
and `refresh()` complete it.

> [!NOTE]
> `chunk_count()`, `entity_count()` and `matches()` refresh the cached matches before they answer, so they are not
> `const`, and neither is a view's `entity_count()`. Call them on a query or view you hold by value or by non-const
> reference. They stay non-const on purpose: a `const` call that quietly updated the cache would race when two
> threads read the same query.

A `chunk_view` is one chunk of one archetype: up to a few hundred entities whose components sit in contiguous
columns.

```cpp
#include "conjure/ecs/chunk_view.h"
#include "conjure/ecs/view.h"

void integrate_chunks(world &game, f32 deltaSeconds) {
    game.view<const velocity, position>().for_each_chunk([&](chunk_view &chunk) {
        const std::span<const velocity> velocities = chunk.get<const velocity>();   // no stamp
        std::span<position> positions = chunk.get<position>();                       // stamps once
        for (u32 row = 0; row < chunk.count(); ++row) {
            positions[row].x += velocities[row].x * deltaSeconds;
        }
    });
}
```

| `chunk_view` member | Purpose |
|---|---|
| `count()`, `entities()` | rows in the chunk and their ids |
| `get<T>()` | the column as `std::span<T>`; `const T` never stamps; asserts when the archetype lacks `T`; a tag does not compile (test it with `has(id)`) |
| `get_optional<T>()` | like `get`, but an absent component yields an empty span; a tag does not compile either |
| `has(id)`, `column_data(id, forWrite)`, `column_stride(id)`, `column_version(id)` | untyped access for bindings |
| `owner()`, `chunk_index()`, `stamp_version()`, `is_valid()` | where the chunk lives and which version a write stamps |

> [!CAUTION]
> Spans from a `chunk_view` point into the chunk. They are valid only inside the callback (or while you hold the
> `iteration_scope` you took yourself); a structural change afterwards may move or free the rows.

## Systems

A system is a `system_desc`: a `name` (`core::string_id`), a `phase`, `flags`, the `reads` and `writes`
component masks, `after` / `before` lists of system names and the `run` function. `make_system<Pack...>(name,
phase, run)` builds one whose `reads` and `writes` derive from the pack exactly as a view's do, so the common
one-view system declares nothing by hand.

```cpp
#include "conjure/ecs/scheduler.h"
#include "conjure/ecs/system.h"

core::result<void> add_movement(scheduler &systems) {
    CONJURE_TRY(systems.add_system(make_system<const velocity, position>(
      core::string_id("game.integrate"),
      system_phase::UPDATE,
      [](system_context &context) {
          context.view<const velocity, position>().for_each([&](entity, const velocity &v, position &p) {
              p.x += v.x * context.delta_time();
          });
      }
    )));
    return systems.finalise();                         // reports cycles before the first frame
}
```

Inside `run`, `system_context` is everything the system may touch:

| Member | Purpose |
|---|---|
| `world()` | the world, **locked** (unless the system is `EXCLUSIVE`) |
| `delta_time()`, `frame()` | `frame_info::deltaTime` and the full timing block |
| `since_version()` | the world version at the start of this system's previous run: `changed<T>(context.since_version())` sees every write since, never the system's own previous writes made through `view<...>()` or `parallel_for_chunks` (those stamp `run_version()`); writes through `world().get<T>()`, `set` or `component_ref` stamp the current world version, which a sibling or a structural call may have bumped, so the next run can see them |
| `commands()` | the system's `command_buffer`, flushed when its stage ends |
| `view<Pack...>()` | a view checked against the declared access in Debug and Development (an undeclared component asserts naming the system; the checks compile out in Distribution); its writes stamp `run_version()` in every build |
| `parallel_for_chunks(query, fn)` | `fn(chunk_view &, command_buffer &)` per chunk over the job system (below) |
| `jobs()`, `desc()`, `run_version()`, `last_grain_count()` | the job system (nullptr when inline), the description, the stamp version, the grain count of the last dispatch |

`system_flags`: none set means the system runs as a job alongside the others of its stage; `MAIN_THREAD` runs
it on the thread that called `run_phase`, last in its stage; `EXCLUSIVE` runs it alone with an **unlocked** world
so it may make structural changes directly; `DISABLED` skips it; `RUNS_WHILE_PAUSED` marks it for
`run_phase_filtered` while the game is paused.

> [!IMPORTANT]
> Declare every component the body reads or writes. `make_system<Pack...>` does it from the pack; with a
> hand-built `system_desc`, fill `reads` and `writes` yourself. Debug builds assert on an undeclared write; in
> every build an undeclared access is a data race waiting for another system to share the stage.

## Phases and the scheduler

`system_phase` lists the slots of one frame in order: `FRAME_BEGIN`, `FIXED_UPDATE`, `PHYSICS_STEP`,
`FIXED_END` (the fixed-step trio runs zero or more times per frame), `UPDATE`, `LATE_UPDATE`, `TRANSFORM`,
`SYNC`, `EXTRACT`, `FRAME_END`. `frame_info` carries `deltaTime`, `fixedDeltaTime`, `interpolationAlpha`,
`time`, `frameIndex` and `inFixedStep`; the engine fills it before every `run_phase`. Gameplay belongs in
`UPDATE` and `LATE_UPDATE`; fixed-rate simulation in `FIXED_UPDATE`.

`scheduler(world, jobs, scheduler_desc = {})` owns the systems. `add_system(desc)` returns a `system_id`
(`SYSTEM_CYCLE` when `after`/`before` would loop; unknown names in those lists are ignored because the module may
be compiled out, with one `DEBUG` log each when the system is added), `remove_system(id)` and
`set_enabled(id, bool)` take it back, `finalise()` builds every
phase's stages now, and `run_phase(phase, frame)` runs the phase. `stages(phase)` shows the `stage_info` groups,
`last_timings(phase)` the microseconds per system, `system(id)` and `system_count()` the registrations. Like the
query counters, `stages(phase)` is not `const`: it first rebuilds a phase whose systems changed since the last
build, and returns an empty span when that rebuild fails.

```cpp
scheduler systems(game, &jobs);                        // jobs: a started core::jobs::job_system
CONJURE_TRY(add_movement(systems));
frame_info frame = {};
frame.deltaTime = 1.0f / 60.0f;
systems.run_phase(system_phase::UPDATE, frame);        // owner thread, world unlocked
```

What happens inside `run_phase`: systems of the phase are packed into **stages**; two systems share a stage when
neither writes what the other reads or writes and no `after`/`before` edge orders them. Each stage runs its
systems as jobs over the locked world, waits, then flushes every system's command buffer in stage order. A
`scheduler_desc` with `parallelSystems = false` and `parallelChunks = false` runs the same stages strictly
serially, which is how headless tests reproduce the engine's behaviour.

> [!TIP]
> Pass `nullptr` as the job system in unit tests: everything runs inline on the calling thread with identical
> stage semantics and flush order, and `world_hash` lets you prove the two modes agree.

## Going wide: `parallel_for_chunks`

`context.parallel_for_chunks(query, fn)` splits the matching chunks into grains of about
`scheduler_desc::minEntitiesPerJob` entities, runs each grain as one job with its **own** `command_buffer` and
appends the grain buffers to the system's buffer in grain order after the join. Command application therefore
never depends on how many workers ran.

```cpp
make_system<const velocity, position>(core::string_id("game.integrate"), system_phase::UPDATE,
  [](system_context &context) {
      view<const velocity, position> moving = context.view<const velocity, position>();
      context.parallel_for_chunks(moving.underlying(), [&](chunk_view &chunk, command_buffer &commands) {
          const std::span<const velocity> velocities = chunk.get<const velocity>();
          std::span<position> positions = chunk.get<position>();
          for (u32 row = 0; row < chunk.count(); ++row) {
              positions[row].x += velocities[row].x * context.delta_time();
              if (positions[row].x > 20.0f) {
                  commands.add<frozen_tag>(chunk.entities()[row]);      // the grain's buffer, not context.commands()
              }
          }
      });
  })
```

Rules for the callable: touch only the chunk you were handed and the `command_buffer` you were handed; never
`context.commands()` from inside it; protect any shared accumulator with an atomic or a mutex. Entity ids
reserved from inside grains (`commands.create_entity()`) are unique but their **values** differ between runs
with different worker counts; gameplay must not depend on them.

> [!WARNING]
> Column version stamps are safe to race on (relaxed atomics), but component data is not. Two systems in one
> stage never write the same component because the scheduler checked the masks; inside one system that is your
> job: one grain, one set of rows.

## Change detection

The world has a `version()` counter that is bumped before every system run, every flush and every direct
structural change (`bump_version()` does it by hand). Writing a column through a non-const span,
`world::get<T>()` with a non-const `T` (`get<const T>()` only reads and never stamps), `component_ref::write()`,
`set` or a row moving into the chunk stamps that chunk's column. Outside a system the stamp is the current
version; inside one, the chunk views of `context.view<...>()` and `parallel_for_chunks` stamp the system's
`run_version()`, so its own writes never pass its next `since_version()` even when a structural call or a sibling
system bumped the world version during the run. Writes a system makes through `context.world()` (`get<T>()`, `set`,
`component_ref::write()`) still stamp the current world version and can show up in its next run when the version
moved on meanwhile; write through the context's views to stay invisible to yourself. `changed<T>(since)` visits a chunk when its stamp is greater than
`since`: never a false negative, at most one chunk of false positives per real write. In a system, use
`context.since_version()`; outside one, remember `world.version()` from the previous pass.

## API summary

| Signature (short) | Purpose |
|---|---|
| `view<Pack...> world::view<Pack...>()`; `struct maybe<T>`; `struct view_access { reads, writes }` | typed iteration |
| `view &with<Tag>()` / `without<T>()` / `changed<T>(u64 since)` | narrowing |
| `void for_each(fn)` / `for_each_chunk(fn)` / `parallel_for_each_chunk(system_context &, fn)`; `u32 entity_count()`; `query &underlying()` | iteration |
| `struct query_desc { with, without, optional, writes, changed; u64 sinceVersion; }`; `query world::create_query(desc)` | untyped match |
| `query::for_each_chunk(fn)` / `chunk_count()` / `entity_count()` / `chunk(u32)` / `matches()` / `refresh()` | untyped iteration |
| `chunk_view::count()` / `entities()` / `span<T> get<T>()` / `get_optional<T>()` / `column_data(id, bool forWrite)` / `column_stride(id)` / `column_version(id)` | one chunk |
| `enum class system_phase { FRAME_BEGIN, ..., FRAME_END, COUNT }`; `to_string(phase)`; `struct frame_info` | the frame |
| `struct system_desc { name, phase, flags, reads, writes, after, before, run }` | a system |
| `system_desc make_system<Pack...>(string_id name, system_phase, std::function<void(system_context &)>)` | access derived from the pack |
| `system_context::world()` / `delta_time()` / `frame()` / `since_version()` / `commands()` / `view<Pack...>()` / `parallel_for_chunks(query &, fn)` | inside a system |
| `scheduler(world &, job_system *, scheduler_desc = {})`; `struct scheduler_desc { parallelSystems, parallelChunks, minEntitiesPerJob }` | the scheduler |
| `result<system_id> add_system(system_desc)` / `result<void> remove_system(id)` / `set_enabled(id, bool)` / `result<void> finalise()` | registration |
| `void run_phase(system_phase, const frame_info &)` / `run_phase_filtered(phase, frame, system_flags required)` | running |
| `span<const stage_info> stages(phase)` / `span<const system_timing> last_timings(phase)` / `const system_desc *system(id)` / `u32 system_count()` | inspection |

## See also

- 📘 API: [Command buffers and observers](command-buffers-and-observers.md) for `context.commands()` and the iteration lock
- 📘 API: [Entities and components](entities-and-components.md) for defining the types a view names
- 📘 API: [Jobs and threading](../core/jobs.md) for the `job_system` a scheduler runs on
- 📘 API: [Glossary](../glossary.md)
- 🛠 Handbook: [ECS module](../../handbook/modules/ecs.md) for stage packing and grain sizing;
  [Data-oriented design](../../handbook/data-oriented-design.md) for why chunks

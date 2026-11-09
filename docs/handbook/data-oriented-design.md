# Data-oriented design

🛠 **Engineering handbook** · 📘 API: [Jobs and threading](../api/core/jobs.md), [Memory](../api/core/memory.md), [Containers](../api/core/containers.md)

Conjure keeps its internals data-oriented and hides them behind an object facade for casual users. This
chapter explains the practices every module follows: why component storage is archetype chunks iterated as
columns, when SoA beats AoS, how change detection and command buffers keep parallel systems safe, how work is
spread over the job system, how per-frame memory is handed out without locks, why names are `string_id`s and
handles are `slot_map` keys, and what to avoid. The `core` pieces exist today (`conjure::core::jobs`,
`conjure::core::memory`, the containers); the ECS pieces are implemented by the wave-2 `conjure_ecs` module
(headers under `conjure/ecs/`) following the binding design in [`../design/01_ecs.md`](../design/01_ecs.md).
The sections that describe them cite both, and the headers are the source of truth for names and signatures.

## Contents
- [1. Why data-oriented](#1-why-data-oriented)
- [2. Archetype storage and chunk iteration](#2-archetype-storage-and-chunk-iteration)
- [3. SoA versus AoS](#3-soa-versus-aos)
- [4. Change detection](#4-change-detection)
- [5. Command buffers](#5-command-buffers)
- [6. Job-parallel systems](#6-job-parallel-systems)
- [7. Frame memory](#7-frame-memory)
- [8. `string_id` over strings](#8-string_id-over-strings)
- [9. `slot_map` handles over pointers](#9-slot_map-handles-over-pointers)
- [10. What to avoid](#10-what-to-avoid)
- [Related](#related)

## 1. Why data-oriented

The customer requirement is 10,000 dynamic entities, each with a transform, a physics body or a script update,
at 60 Hz on a mid-range desktop without frame loss (`00_brief.md`, key requirement 2). That budget is spent on
memory traffic, not arithmetic: a system that touches one 64-byte component per entity reads 640 KiB per frame
only if those bytes are contiguous and nothing else is pulled into the cache alongside them. Conjure therefore
lays data out for the loops that read it, moves per-entity decisions (virtual dispatch, allocation, pointer
following) out of the hot path, and gives the job system whole chunks as the unit of work. The OOP facade
(`entity_handle`, `component_ref<T>`, C# `Entity`) is a thin view over the same storage, so casual users never
pay for the abstraction twice and power users can drop to `chunk_view` / `Span<T>` (`03_scripting.md` 3.10).

## 2. Archetype storage and chunk iteration

> [!NOTE]
> Wave 2 (`01_ecs.md` D1-D3, 2.3; `conjure/ecs/archetype.h`, `conjure/ecs/chunk_view.h`, `conjure/ecs/world.h`).
> The numbers below come from the record's layout algorithm.

Every live entity belongs to exactly one **archetype**, the set of component types it carries, encoded as a
`component_mask` of 16 x `u64` (1024 dense component ids). An archetype stores its entities in fixed
**16 KiB chunks** (`CHUNK_SIZE = 16 * 1024`, 64-byte aligned, from a `core::memory::pool_allocator`). Inside a
chunk every component is a separate column: the entity column at offset 0, then the components ordered by
descending alignment, each at `align_up(previousEnd, max(alignment, 16))`. The chunk capacity is the largest row
count that fits, so a `{transform 64 B}` archetype holds 227 rows per chunk, `{transform, sprite 32 B, velocity
12 B, health 8 B}` holds 132, and 10,000 such entities occupy 76 chunks. Rows are packed: only the last chunk
of an archetype is partial, and `remove_row` swap-removes by relocating the last row into the hole.

Chunks are the job granule and the iteration unit. A query matches archetypes with two mask operations
(`mask.contains_all(with) && !mask.contains_any(without)`), caches the matching `archetype *` (the world's
archetype list is append-only, so pointers are stable) and visits chunk after chunk, handing each system a
`chunk_view` whose `get<T>()` returns the contiguous column. A random-access lookup from the facade goes through
one dense `entity_record` table (`archetype *, chunkIndex, row, generation`), 24 bytes per entity, O(1).

## 3. SoA versus AoS

A chunk is structure-of-arrays at the component level and array-of-structures inside a component: `transform`
stays one 40-byte struct (`conjure/core/math/transform.h`, trivially copyable, mirrored by C#) rather than
ten float arrays. The trade-offs that decide where the line sits:

| Layout | Wins when | Costs |
|---|---|---|
| SoA per component (chunk columns) | a system reads two of five components: only those two columns are streamed; `parallel_for` splits by row range with no pointer chasing; a column can be stamped as changed | adding or removing a component moves the row to another archetype (one `move_entity`) |
| AoS inside a component (`transform`, `aabb`, `rigid_body`) | fields are read together, must stay memcpy-compatible with C# and Jolt, and are small enough to fit a cache line | a system that needs only `position` still streams `rotation` and `scale` |
| Separate components instead of one fat struct | the fields are written by different systems in different phases (`local_transform` vs `world_transform` vs `render_world_transform`) | more archetype moves if they are added at different times |

Rule of thumb: split by *who writes it and when*, keep what is always read together in one plain struct,
and keep components under `MAX_COMPONENT_SIZE = 4096` bytes; bulk data belongs behind an asset handle.

## 4. Change detection

> [!NOTE]
> Wave 2 (`01_ecs.md` D5, 3.3), implemented by `conjure_ecs`; the headers win over the record for names.

Detection is chunk-granular. The world keeps a monotonic `u64` version that is bumped at the start of every
system run, every flush and every direct structural call. Acquiring a non-const column (`chunk_view::get<T>()`,
`component_ref::write()`, `world::get<T>()`) stores the current version into that chunk's column stamp (a
relaxed atomic); a row moving into a chunk stamps all of its columns. A query with `changed<T>(sinceVersion)`
visits a chunk only when `max(columnVersion[c])` for the listed columns exceeds `sinceVersion`, which a system
sets to the version it observed at the start of its *previous* run. There is no per-row bookkeeping, no false
negatives and at most one chunk of false positives per real write; the cost is a handful of atomic stores per
chunk per frame instead of a dirty bit per entity.

## 5. Command buffers

> [!NOTE]
> Wave 2 (`01_ecs.md` D6, 2.5, 3.4; the `STRUCTURE_LOCKED` code is `conjure/ecs/ecs_error.h`). The arena it is
> built on, `core::memory::arena`, exists today.

Structural changes (create, destroy, add, remove) are refused while the world is locked for iteration: the call
asserts and returns `STRUCTURE_LOCKED`. Systems and observers record into a `command_buffer` instead. Records
live in a `core::memory::arena` (`conjure/core/memory/arena.h`; stable addresses until `reset()`), payloads are
relocated in with `ops.relocate` or memcpy, and `create_entity()` returns a *real* id immediately from a 64-id
block reserved per buffer, so a deferred create can be used in later commands and handed to C# at once. Flush
plans once over the records (destroy wins, add clears remove and vice versa) and applies **one archetype move
per touched entity**, in record order, with the delta fired to observers. Per-grain buffers from a parallel
system are appended in grain order, so the result is deterministic whatever worker ran which grain.

## 6. Job-parallel systems

Today's primitive is `conjure::core::jobs::job_system` (`conjure/core/jobs/job_system.h`): a shared,
priority-laned queue in front of `workerCount` threads, with `wait()` that *helps* (the waiting thread executes
queued jobs, so waiting inside a job never deadlocks) and `parallel_for(count, grain, body)` that tiles
`[0, count)` into ranges of at most `grain`, queues them under one lock acquisition, runs range 0 on the caller
and helps until the stack-held counter reaches zero. The body is referenced, not copied, and once the block pool
has warmed up a dispatch allocates nothing (`stats().poolGrowths` stays constant; the jobs tests assert it).
With `workerCount = 0` everything runs inline on the caller, which is how headless tests stay deterministic.

```cpp
#include "conjure/core/jobs/job_system.h"
#include "conjure/core/jobs/parallel_for.h"
#include "conjure/core/math.h"

#include <span>

using namespace conjure;
using namespace conjure::core;

void integrate(std::span<math::vec3> positions, std::span<const math::vec3> velocities, f32 deltaSeconds) {
    jobs::job_system system = {};
    jobs::job_system_desc desc = {};
    desc.workerCount = 8;                       // 0 = inline, deterministic
    if (!system.start(desc)) {
        return;
    }

    jobs::parallel_for_range(system, positions.size(), 256, [&](usize begin, usize end) {
        for (usize index = begin; index < end; ++index) {
            positions[index] += velocities[index] * deltaSeconds;   // disjoint ranges: no synchronisation
        }
    });

    system.stop();
}
```

The scheduler of wave 2 builds on this (`01_ecs.md` D7, 3.5): systems declare read and write masks, the
scheduler packs them into conflict-free **stages** per phase (`(writes & (reads | writes)) == 0` with every
stage member), runs a stage's systems as jobs, and each system fans its chunks out with
`ctx.parallel_for_chunks(query, fn)` using `grain = max(1, minEntitiesPerJob / capacity)` chunks per job.
Main-thread systems run last in their stage on the calling thread; `EXCLUSIVE` systems run alone with the world
unlocked. System-level and chunk-level parallelism can be disabled independently, and the serial path is the
same code with stages executed inline.

> [!WARNING]
> Entity id *values* reserved from parallel code are not stable across worker counts (R41). Determinism tests
> spawn only from `EXCLUSIVE` / `MAIN_THREAD` systems or from command buffers flushed in grain order.

## 7. Frame memory

Anything that lives for one frame (culling lists, sort keys, extraction scratch) comes from
`conjure::core::memory::frame_allocator` (`conjure/core/memory/frame_allocator.h`), never from the heap. One
arena of `arenaBytes` is carved into thread blocks of `threadBlockBytes` (rounded to whole cache lines so two
threads never share a line); a thread claims a block with a single compare-exchange and then bumps inside it
through a `thread_local` slot `{owner, epoch, cursor, end, arenaExhausted}` with no synchronisation at all.
`reset()` frees the heap-overflow blocks, rewinds the arena and takes a new process-unique **epoch**, so a stale
slot (previous frame, another allocator, or an allocator that has since been destroyed) never matches and is
never bumped from. The engine's `frame_memory` facade resets it at FRAME_BEGIN when no job is in flight
(`06_engine_loop.md` 2.6); any scheduler job may allocate.

```cpp
#include "conjure/core/memory/frame_allocator.h"

#include <span>

using namespace conjure;
using namespace conjure::core::memory;

struct draw_key {
    u64 sortKey = 0;
    u32 instanceIndex = 0;
};

void build_keys(frame_allocator &frame, usize visibleCount) {
    std::span<draw_key> keys = frame.allocate_span<draw_key>(visibleCount);   // uninitialised, this frame only
    for (usize index = 0; index < keys.size(); ++index) {
        keys[index] = draw_key{index * 2, static_cast<u32>(index)};
    }
    // No free: keys die at the next frame.reset(). Size the arena from frame.high_water_bytes().
}
```

A request that does not fit the remaining block claims a new block; one larger than a block, or one made after
the arena is exhausted, falls back to `tracked_allocate` and is counted in `overflow_count()` /
`overflow_bytes()`; the engine warns once per second with `high_water_bytes()` so the budget can be raised.
Only trivially destructible types may be placed in frame memory (`allocate_span<T>` and `construct<T>`
`static_assert` it) because `reset()` runs no destructors. Longer-lived scratch uses `arena` (reset as a
whole) and fixed-size objects use `pool_allocator`; both are single-owner and report every block to the
process-wide `allocation_counters`, which is what lets tests gate a code path on zero allocations with
`CONJURE_CHECK_NO_ALLOCATIONS`.

## 8. `string_id` over strings

Names that are compared at runtime (component types, extension points, mount schemes, log categories in
tooling) are `conjure::core::string_id` (`conjure/core/string_id.h`): the 64-bit FNV-1a of the name and
nothing else, so it is 8 trivially copyable bytes, compares with one integer comparison, hashes for free and is
identical in every shared library, every session and the C# `NameHash`. The spelling is kept in a process-wide
interned table for tools only. Hot loops use `"name"_sid`, which is `consteval` and never takes the table lock;
statics use `string_id::intern("name")` so the name stays resolvable:

```cpp
#include "conjure/core/string_id.h"

using namespace conjure::core;
using namespace conjure::core::literals;

const string_id TRANSFORM_NAME = string_id::intern("conjure.scene.local_transform");   // interned: tools print it

bool is_transform(string_id componentName) {
    return componentName == TRANSFORM_NAME;
}

bool is_sprite(string_id componentName) {
    return componentName == "conjure.renderer.sprite"_sid;   // compile-time hash, never takes the lock
}
```

Component ids themselves are dense `u16`s assigned per *name* (`01_ecs.md` D3-D4), so archetype matching is a
mask operation and a plugin's components agree with the host's without any template static.

## 9. `slot_map` handles over pointers

Anything that can be destroyed while something still refers to it (GPU resources, voices, script instances,
editor requests, asset slots) is addressed by a generational key, not a pointer. `conjure::core::slot_map<T>`
(`conjure/core/containers/slot_map.h`) stores elements densely in `{key, value}` entries (swap-remove on
erase, cache-friendly iteration) behind an indirection table whose slots carry a generation that is bumped on
erase. `find(key)` is O(1), allocation-free and returns `nullptr` for a stale key even when the slot has been
reused; generation 0 is never issued, so an all-zero `slot_key` is null for free (zero-initialised memory,
`default(T)` in C#) and `to_u64()` packs a key into one opaque integer for ImGui and serialisation.

```cpp
#include "conjure/core/containers/slot_map.h"

using namespace conjure;
using namespace conjure::core;

struct voice {
    u32 clipIndex = 0;
    f32 gain = 1.0f;
};

void fade_voice(slot_map<voice> &voices, slot_key key) {
    voice *found = voices.find(key);          // O(1), allocation-free
    if (found == nullptr) {
        return;                               // stale: the voice was erased and the generation moved on
    }
    found->gain *= 0.5f;
}

void fade_all(slot_map<voice> &voices) {
    for (auto &[liveKey, liveVoice] : voices) {   // dense storage order, no indirection
        liveVoice.gain *= 0.5f;
    }
}
```

The same shape recurs everywhere: `ecs::entity` is `{index, generation}`, `modules::module_id` is a 16-bit
pair, asset slots live in a `stable_vector` so their addresses never move while handles point at them.

## 10. What to avoid

| Avoid | Because | Do instead |
|---|---|---|
| A virtual call per entity (`for (auto *object : objects) object->update()`) | indirect branch and a cache miss per element; defeats chunk iteration and vectorisation | one system per behaviour over a column; `component_ops` function pointers chosen once per *column*, never per row (`01_ecs.md` D8) |
| Heap allocation per frame (`std::vector` built and dropped inside a system, `std::function` captured per entity) | allocator locks, fragmentation, nondeterministic timing | `frame_allocator::allocate_span`, `arena` reset as a whole, `small_vector<T, N>` for short lists; gate with `CONJURE_CHECK_NO_ALLOCATIONS` |
| Pointer chasing (`std::map`, `std::list`, `std::shared_ptr` graphs, linked hierarchies walked per frame) | every hop is a potential cache miss | dense arrays plus indices, `flat_hash_map` (open addressing) or `flat_map` (sorted vector), depth-level parallel propagation over columns (`13_scene.md`) |
| Raw pointers to elements of growing containers | moved on reallocation, dangling on erase | `slot_map` keys, `stable_vector` for address stability |
| `std::string` comparisons and `std::unordered_map<std::string, ...>` on hot paths | allocation and byte-wise compare per lookup | `string_id` keys |
| Mutating the world while iterating it | iterator invalidation, placeholder-id remapping | `command_buffer`, flushed at stage end |
| `std::thread`, mutexes and condition variables in module code | ad-hoc threads fight the job system for cores and break headless determinism | `job_system::parallel_for`, `post_to_main_thread`; `dedicated_thread` only for a sanctioned blocking reader such as the editor's `--ai-stdio` stdin (R38) |
| Per-row dirty bits | state to keep consistent across swap-removes | chunk column version stamps |

> [!TIP]
> Measure before and after: `job_system::stats()`, `frame_allocator::high_water_bytes()`,
> `memory::snapshot().live_count()` and the benchmarks in `tests/benchmarks` (`08_testing.md` 7.1) exist so a
> data-layout change is a number, not an opinion.

## Related
- design records: [`../design/01_ecs.md`](../design/01_ecs.md) (archetypes, change detection, command buffer,
  scheduler), [`../design/06_engine_loop.md`](../design/06_engine_loop.md) 2.6 and 5 (frame memory, threads),
  [`../design/10_core.md`](../design/10_core.md) 2.3, 2.5, 2.7 (containers, memory, jobs),
  [`../design/09_cross_area_resolutions.md`](../design/09_cross_area_resolutions.md) R3, R5, R17, R41
- handbook: [Architecture](architecture.md) 5-6, [Core module](modules/core.md) sections 3, 5 and 7,
  [Coding standards](coding-standards.md)

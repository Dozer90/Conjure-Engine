# Data-Oriented Design
<!-- doc-template:chapter -->
[Handbook](../index.md) → [Architecture and Design](index.md)

<!-- doc-content:purpose:start -->
Conjure keeps its internals data-oriented and hides them behind an object facade for casual users. This
chapter explains the practices every module follows: why component storage is archetype chunks iterated as
columns, when SoA beats AoS, how change detection and command buffers keep parallel systems safe, how work is
spread over the job system, how per-frame memory is handed out without locks, why names are `string_id`s and
handles are `slot_map` keys, and what to avoid. The `core` pieces are `conjure::core::jobs`,
`conjure::core::memory` and the containers; the ECS pieces are `conjure_ecs` (headers under `conjure/ecs/`),
whose internals the [ECS module](../engine/ecs.md) page describes section by section and whose user-facing side is
📘 API: [Entities, components and systems](../../api/ecs/index.md). The sections below explain the *why*; the
module page and the headers are the source of truth for layout numbers, names and signatures.
<!-- doc-content:purpose:end -->

<!-- doc-content:body:start -->
<a id="1-why-data-oriented"></a>

## Why data-oriented

The customer requirement is 10,000 dynamic entities, each with a transform, a physics body or a script update,
at 60 Hz on a mid-range desktop without frame loss (key requirement 2). That budget is spent on
memory traffic, not arithmetic: a system that touches one 64-byte component per entity reads 640 KiB per frame
only if those bytes are contiguous and nothing else is pulled into the cache alongside them. Conjure therefore
lays data out for the loops that read it, moves per-entity decisions (virtual dispatch, allocation, pointer
following) out of the hot path, and gives the job system whole chunks as the unit of work. The OOP facade
(`entity_handle`, `component_ref<T>`, C# `Entity`) is a thin view over the same storage, so casual users never
pay for the abstraction twice and power users can drop to `chunk_view` / `Span<T>`.

<a id="2-archetype-storage-and-chunk-iteration"></a>

## Archetype storage and chunk iteration

> [!NOTE]
> Implemented in `conjure/ecs/archetype.h`, `conjure/ecs/chunk_view.h` and `conjure/ecs/world.h`
> ([ECS module](../engine/ecs.md) 2.3 describes the chunk layout).
> User view: 📘 API: [Archives and serialisation](../../api/ecs/archives-and-serialization.md) and
> [Queries, views and systems](../../api/ecs/queries-views-and-systems.md).

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

<a id="3-soa-versus-aos"></a>

## SoA versus AoS

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

<a id="4-change-detection"></a>

## Change detection

> [!NOTE]
> Implemented in `conjure/ecs/query.h` and `conjure/ecs/view.h` ([ECS module](../engine/ecs.md) 3.2). User view: 📘 API: [Queries, views and systems](../../api/ecs/queries-views-and-systems.md).

Detection is chunk-granular. The world keeps a monotonic `u64` version that is bumped at the start of every
system run, every flush and every direct structural call. Acquiring a non-const column (`chunk_view::get<T>()`,
`component_ref::write()`, `world::get<T>()`) stores the current version into that chunk's column stamp (a
relaxed atomic); a row moving into a chunk stamps all of its columns. A query with `changed<T>(sinceVersion)`
visits a chunk only when `max(columnVersion[c])` for the listed columns exceeds `sinceVersion`, which a system
sets to the version it observed at the start of its *previous* run. There is no per-row bookkeeping, no false
negatives and at most one chunk of false positives per real write; the cost is a handful of atomic stores per
chunk per frame instead of a dirty bit per entity.

<a id="5-command-buffers"></a>

## Command buffers

> [!NOTE]
> Implemented in `conjure/ecs/command_buffer.h` ([ECS module](../engine/ecs.md) 3.3; the `STRUCTURE_LOCKED` code is `conjure/ecs/ecs_error.h`). User view: 📘 API:
> [Command buffers and observers](../../api/ecs/command-buffers-and-observers.md).

Structural changes (create, destroy, add, remove) are refused while the world is locked for iteration: the call
asserts and returns `STRUCTURE_LOCKED`. Systems and observers record into a `command_buffer` instead. Records
live in a `core::memory::arena` (`conjure/core/memory/arena.h`; stable addresses until `reset()`), payloads are
relocated in with `ops.relocate` or memcpy, and `create_entity()` returns a *real* id immediately from a 64-id
block reserved per buffer, so a deferred create can be used in later commands and handed to C# at once. Flush
plans once over the records (destroy wins, add clears remove and vice versa) and applies **one archetype move
per touched entity**, in record order, with the delta fired to observers. Per-grain buffers from a parallel
system are appended in grain order, so the result is deterministic whatever worker ran which grain.

<a id="6-job-parallel-systems"></a>

## Job-parallel systems

Today's primitive is `conjure::core::jobs::job_system` (`conjure/core/jobs/job_system.h`): a shared,
priority-laned queue in front of `workerCount` threads, with `wait()` that *helps* (the waiting thread executes
queued jobs, so waiting inside a job never deadlocks) and `parallel_for(count, grain, body)` that tiles
`[0, count)` into ranges of at most `grain`, queues them under one lock acquisition, runs range 0 on the caller
and helps until the stack-held counter reaches zero. The body is referenced, not copied, and once the block pool
has warmed up a dispatch allocates nothing (`stats().poolGrowths` stays constant; the jobs tests assert it).
Batches a frame dispatches again and again (`schedule(jobs, fence)` with a caller-owned `job_fence`) allocate
nothing either; only `schedule(fn)` / `schedule(span)` returning a `job_handle` allocate their shared counter.
Workers and waiters spin for a bounded 50 us before they sleep and the queue lock is a spinlock that parks
after a bounded spin, so dispatches less than 50 us apart (consecutive transform levels) pay no kernel wake-up
and a queued job costs about 0.8 us with 16 workers in the round's final enforced run (`core.job_parallel_for_1m`
0.052 ms for 64 ranges; 0.6 us with the ticket lock the round first used, `engine/src/core/jobs/README.md`); a stage
boundary that runs main-thread systems or flushes between dispatches still parks the workers, and every awake
worker spins the window once the queue runs dry ([Core module](../engine/core/time-to-reflect.md) 2.2).
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

The ECS scheduler builds on this (`conjure/ecs/scheduler.h`, [ECS module](../engine/ecs.md) 3.4): systems declare read and write masks, the
scheduler packs them into conflict-free **stages** per phase (`(writes & (reads | writes)) == 0` with every
stage member), runs a stage's systems as jobs, and each system fans its chunks out with
`ctx.parallel_for_chunks(query, fn)` using `grain = max(1, minEntitiesPerJob / capacity)` chunks per job.
Main-thread systems run last in their stage on the calling thread; `EXCLUSIVE` systems run alone with the world
unlocked. System-level and chunk-level parallelism can be disabled independently, and the serial path is the
same code with stages executed inline.

> [!WARNING]
> Entity id *values* reserved from parallel code are not stable across worker counts. Determinism tests
> spawn only from `EXCLUSIVE` / `MAIN_THREAD` systems or from command buffers flushed in grain order.

<a id="7-frame-memory"></a>

## Frame memory

Anything that lives for one frame (culling lists, sort keys, extraction scratch) comes from
`conjure::core::memory::frame_allocator` (`conjure/core/memory/frame_allocator.h`), never from the heap. One
arena of `arenaBytes` is carved into thread blocks of `threadBlockBytes` (rounded to whole cache lines so two
threads never share a line); a thread claims a block with a single compare-exchange and then bumps inside it
through a `thread_local` slot `{owner, epoch, cursor, end, arenaExhausted}` with no synchronisation at all.
`reset()` frees the heap-overflow blocks, rewinds the arena and takes a new process-unique **epoch**, so a stale
slot (previous frame, another allocator, or an allocator that has since been destroyed) never matches and is
never bumped from. The engine's `frame_memory` facade resets it at FRAME_BEGIN when no job is in flight
; any scheduler job may allocate.

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

<a id="8-allocation-free-steady-state"></a>

## Allocation-free steady state

> [!NOTE]
> The gates are the `allocs` column of (`tests/benchmarks/thresholds.json`, enforced in
> Development) and the `CONJURE_CHECK_NO_ALLOCATIONS` cases next to each hot path. The performance round of
> 2026-11 ("Performance round" in section 3) closed every
> allocation breach with the four patterns below; `tests/benchmarks/README.md` has the measured table.

A frame that allocates nothing after its first few runs is not a property of any one container; it is the
sum of every per-frame object that was either built fresh each run or returned its storage at the end of
the run. The patterns the engine settled on, each with the site that uses it:

| Pattern | Rule | Where |
|---|---|---|
| **Persistent frame state** | Everything a frame fills lives on its owner and is reset *in place*: `clear()` on a vector that keeps its capacity, a pool of state objects reused by index (`renderer_state::next_view_state()` / `prepared_views()`), member scratch instead of a local (`world::mFlushPlan`, `transform_system::mLevelChunks`, `cull_scratch`). Never `vector.clear()` on a container that is destroyed at the end of the run, never a moved-to local (`std::swap` with a fresh object), never `std::stable_sort` (its buffer is a heap block; `std::sort` with an index tie-break is the stable order for free). | `renderer.cpp`, `render_graph::reset`, `world_structural.cpp`, `transform_propagation.cpp`, `command_buffer::clear()` (keeps one arena block spanning the last recording) |
| **Caller-owned job counters** | The completion state of a per-frame dispatch belongs to the dispatcher's stack or object, never to a `std::shared_ptr` the job system allocates: `parallel_for` counts on a stack `job_counter`, `job_system::schedule(jobs, fence)` signals a `core::jobs::job_fence` the ECS scheduler holds on `run_phase`'s stack (one fence serves every stage because a stage is waited for before the next is queued). `schedule(fn)` / `schedule(span)` returning a `job_handle` are for work whose handle may outlive the frame. | `ecs/scheduler.cpp`, `core/jobs/job_system.cpp` |
| **Cached queries** | An `ecs::query` is built once per component set and kept for the life of the world it matches (its match list refreshes incrementally; archetype pointers are stable). A system owns its queries as members (`transform_system::mLevelQueries`, the physics pull's `pullQuery`), the script runtime keys them by the sorted component-id set (`script_runtime::cached_query`), and a view built per run (`context.view<...>()`) stores up to `QUERY_INLINE_MATCHES` = 16 matches inline so the documented idiom stays allocation-free in the common case. | `scene/transform_system.h`, `physics_world_sync.cpp`, `scripting/script_runtime.cpp`, `ecs/query.h` |
| **Opt-in background work** | Anything that allocates on a job thread on its own schedule (the asset hot-reload watcher's 500 ms stat pass) is a switch (`assets_settings::hotReload`), off in `headless_defaults()`, because `allocation_scope` counts every thread of the process and the frame gate is the maximum over the measured iterations. | `engine/subsystems/assets_subsystem.cpp` |

Two rules follow for new code. Size by the peak and keep it: scratch that grew to its largest run stays at
that capacity for its owner's lifetime, and one explicit release point (`world::clear()`,
`command_buffer::shrink_to_fit()`, `scheduler::remove_system`) returns it, never the end of a frame. And
count the instrument too: the null device the renderer gates measure through records marker names only
after `reset_null_stats()` asked for them, so a headless run stores nothing and the gate counts device-side
blocks without excluding them.

> [!WARNING]
> MSVC's debug iterators (`_ITERATOR_DEBUG_LEVEL 2`, Debug test executables) allocate a container proxy for
> every `std::vector` *object*, an empty or moved-to one included. A gate that is zero in Development can
> therefore read one block per local vector in Debug; the integration budgets record both figures
> and the Development benchmark checks the allocation budget.

<a id="9-string_id-over-strings"></a>

## `string_id` over strings

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

Component ids themselves are dense `u16`s assigned per *name*, so archetype matching is a
mask operation and a plugin's components agree with the host's without any template static.

<a id="10-slot_map-handles-over-pointers"></a>

## `slot_map` handles over pointers

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

<a id="11-what-to-avoid"></a>

## What to avoid

| Avoid | Because | Do instead |
|---|---|---|
| A virtual call per entity (`for (auto *object : objects) object->update()`) | indirect branch and a cache miss per element; defeats chunk iteration and vectorisation | one system per behaviour over a column; `component_ops` function pointers chosen once per *column*, never per row |
| Heap allocation per frame (`std::vector` built and dropped inside a system, `std::function` captured per entity) | allocator locks, fragmentation, nondeterministic timing | `frame_allocator::allocate_span`, `arena` reset as a whole, `small_vector<T, N>` for short lists; gate with `CONJURE_CHECK_NO_ALLOCATIONS` |
| Pointer chasing (`std::map`, `std::list`, `std::shared_ptr` graphs, linked hierarchies walked per frame) | every hop is a potential cache miss | dense arrays plus indices, `flat_hash_map` (open addressing) or `flat_map` (sorted vector), depth-level parallel propagation over columns |
| Raw pointers to elements of growing containers | moved on reallocation, dangling on erase | `slot_map` keys, `stable_vector` for address stability |
| `std::string` comparisons and `std::unordered_map<std::string, ...>` on hot paths | allocation and byte-wise compare per lookup | `string_id` keys |
| Mutating the world while iterating it | iterator invalidation, placeholder-id remapping | `command_buffer`, flushed at stage end |
| `std::thread`, mutexes and condition variables in module code | ad-hoc threads fight the job system for cores and break headless determinism | `job_system::parallel_for`, `post_to_main_thread`; `dedicated_thread` only for a sanctioned blocking reader such as the editor's `--ai-stdio` stdin |
| Per-row dirty bits | state to keep consistent across swap-removes | chunk column version stamps |

> [!TIP]
> Measure before and after: `job_system::stats()`, `frame_allocator::high_water_bytes()` and
> `memory::snapshot().live_count()` exist so a data-layout change is a number, not an opinion, and the benchmark
> harness under `tests/benchmarks` turns it into a threshold ([Testing and Benchmarks](../development/testing-and-benchmarks.md#11-benchmarks)).

## Related

- handbook: -6, [Core module](../engine/core.md) sections 3, 5 and 7,
  [ECS module](../engine/ecs.md) sections 2-5, [Coding standards](../development/coding-standards.md)
- 📘 API: [Entities, components and systems](../../api/ecs/index.md)

<a id="contents"></a>
<!-- doc-content:body:end -->

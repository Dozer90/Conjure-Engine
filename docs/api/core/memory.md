# Memory
<!-- doc-template:wiki -->
[API](../index.md)

<!-- doc-content:purpose:start -->
Most game code never allocates by hand: engine objects, the [containers](containers.md) and the standard library
manage memory for you. This page is for the cases where you do: scratch memory that lives for one frame or one
load, many small objects of one size, or a budget you need to measure. Include the umbrella
`conjure/core/memory/memory.h`, or the individual header named with each tool. Everything lives in
`namespace conjure::core::memory`.
<!-- doc-content:purpose:end -->

|---|---|
| Name | Memory |
| Module | Core |
| `#include` | `conjure/core/memory/allocation_counters.h`, `conjure/core/memory/arena.h`, `conjure/core/memory/frame_allocator.h`, `conjure/core/memory/memory.h`, `conjure/core/memory/pool_allocator.h`, `conjure/core/memory/tracked_allocation.h` |
| Engineering Handbook | [Engineering handbook](../../handbook/engine/core.md) |

## Syntax

<!-- doc-content:syntax:start -->
<a id="quick-example"></a>

```cpp
#include "conjure/core/memory/arena.h"

#include <span>

using namespace conjure;
using namespace conjure::core::memory;

struct draw_command {
    u32 mesh = 0;
    u32 material = 0;
};

// Builds a command list in 'scratch'; the caller resets the arena once the list has been consumed.
std::span<draw_command> build_commands(arena &scratch, u32 count) {
    void *storage = scratch.allocate(count * sizeof(draw_command), alignof(draw_command));
    if (storage == nullptr) {
        return {};
    }
    draw_command *commands = static_cast<draw_command *>(storage);
    for (u32 index = 0; index < count; ++index) {
        commands[index] = draw_command{index, index % 4};
    }
    return std::span<draw_command>(commands, count);
}
```
<!-- doc-content:syntax:end -->

## At a glance

<!-- doc-content:at_a_glance:start -->
| You want to... | Use | Header |
|---|---|---|
| Know how many allocations the process has made | `snapshot()`, `counters()` | `conjure/core/memory/allocation_counters.h` |
| A raw, aligned heap block that is counted | `tracked_allocate(bytes, alignment)` / `tracked_free(pointer, bytes)` | `conjure/core/memory/tracked_allocation.h` |
| Round sizes to an alignment | `align_up(value, alignment)`, `is_power_of_two(value)`, `is_aligned(pointer, alignment)` | `conjure/core/memory/tracked_allocation.h` |
| Many allocations freed all at once (a parse, a command list) | `arena` | `conjure/core/memory/arena.h` |
| Many objects of one fixed size, freed individually | `pool_allocator` | `conjure/core/memory/pool_allocator.h` |
| Scratch memory for this frame, from any thread | `frame_allocator` | `conjure/core/memory/frame_allocator.h` |
<!-- doc-content:at_a_glance:end -->

## Variables and Properties

<!-- doc-content:variables:start -->
Types, fields, and state are described in the reference sections below.
<!-- doc-content:variables:end -->

<!-- doc-content:body:start -->
## Counting allocations

Every heap acquisition made through this area (and, in test executables, every `new`) bumps three process-wide
counters: `allocationCount`, `freeCount` and `allocatedBytes`. They are cumulative and never decrease, so "live
allocations" is `allocationCount - freeCount`, which `memory_snapshot::live_count()` computes. Memory the standard
library and the [containers](containers.md) obtain (`std::vector`, `small_vector`, `stable_vector`,
`flat_hash_map`,...) comes from `std::allocator` and is not counted in a game or the editor.

```cpp
#include "conjure/core/memory/allocation_counters.h"

using namespace conjure;
using namespace conjure::core::memory;

u64 allocations_during_load() {
    const memory_snapshot before = snapshot();
    // ... load a level ...
    return snapshot().allocationCount - before.allocationCount;
}
```

`counters()` returns the live atomics for tools that poll; `snapshot()` copies them. `record_allocation(bytes)` and
`record_free()` exist for code that obtains memory by another route and wants it counted.

> [!NOTE]
> A snapshot's three loads are not one atomic transaction. Compare snapshots when other threads are quiet, or
> accept a small skew.

## Counted heap blocks: `tracked_allocate`

`tracked_allocate(bytes, alignment = DEFAULT_ALIGNMENT)` returns a block whose address is a multiple of `alignment`
(any power of two the platform allocator accepts; 4096 is tested), or `nullptr` when memory is exhausted, when
`bytes` exceeds `MAX_ALLOCATION_BYTES` or when the alignment is not a power of two (that last one also asserts).
Release it with `tracked_free(pointer, bytes)`, passing the size you requested. `tracked_reallocate(pointer,
oldBytes, newBytes, alignment)` resizes a block, preserving the common prefix; it counts as one free plus one
allocation.

> [!CAUTION]
> A block from `tracked_allocate` must be released with `tracked_free`, never with `free()` or `delete`: the
> implementation is the platform's aligned allocator and mixing the two corrupts the heap.

> [!WARNING]
> `DEFAULT_ALIGNMENT` is `alignof(std::max_align_t)`, which is 8 on MSVC x64 while `operator new` guarantees 16
> there. Types with `alignas(16)` or more (SIMD vectors, aligned glm types) must pass their alignment explicitly.

## One-shot memory: `arena`

An `arena` is a bump allocator over a chain of blocks (`DEFAULT_ARENA_BLOCK_BYTES`, 64 KiB, unless you pass a
block size). Individual allocations are never freed; `reset()` recycles everything at once and keeps the first
regular-sized block for reuse, `release()` returns every block to the heap. A request larger than a block gets a
block of its own, so the current block keeps serving small requests; such an oversized block never survives a
`reset()`. It is the right tool for parsers, command buffers and per-load scratch data.

> [!TIP]
> If the first request of every cycle is larger than the block size, each cycle allocates that block again after
> `reset()`. Choose a block size that holds your largest regular request.

| Call | Behaviour |
|---|---|
| `allocate(bytes, alignment = DEFAULT_ALIGNMENT)` | bumps the cursor; `nullptr` for zero bytes, a bad alignment (asserts) or an exhausted heap |
| `construct<T>(args...)` | allocates and constructs a *trivially destructible* `T`, or returns `nullptr` |
| `reset()` | invalidates everything, keeps the first regular-sized block and frees every other one |
| `release()` | invalidates everything, frees every block; the arena stays usable |
| `bytes_allocated()` / `bytes_reserved()` / `block_count()` / `block_bytes()` / `owns(pointer)` | statistics and checks |

> [!CAUTION]
> After `reset()` or `release()` every pointer handed out by the arena is dangling. Using one is undefined
> behaviour. Because destructors never run, `construct<T>` only accepts trivially destructible types; keep strings
> and vectors out of arenas.

> [!WARNING]
> `arena` is not thread-safe. Give each thread its own, or use a `frame_allocator` for shared scratch memory.

## Fixed-size objects: `pool_allocator`

A `pool_allocator(blockSize, alignment, blocksPerSlab, maxSlabs = 0)` hands out blocks of one size from slabs of
`blocksPerSlab` blocks, growing by a slab when the free list is empty (up to `maxSlabs` when given; 0 means
unlimited). Unlike an arena, blocks are returned individually, so it suits objects with independent lifetimes:
components of one type, network packets, nodes.

```cpp
#include "conjure/core/memory/pool_allocator.h"

using namespace conjure;
using namespace conjure::core::memory;

struct projectile {
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 speed = 0.0f;
};

class projectile_pool {
    private:
        pool_allocator mBlocks = pool_allocator(sizeof(projectile), alignof(projectile), 256);

    public:
        [[nodiscard]] projectile *fire(f32 x, f32 y) {
            projectile *shot = mBlocks.construct<projectile>();   // nullptr only when the heap is exhausted
            if (shot != nullptr) {
                shot->x = x;
                shot->y = y;
                shot->speed = 40.0f;
            }
            return shot;
        }

        void retire(projectile *shot) {
            mBlocks.destroy(shot);                                // runs ~projectile, returns the block
        }
};
```

| Call | Behaviour |
|---|---|
| `allocate()` / `deallocate(block)` | raw blocks; `allocate()` returns `nullptr` at the slab cap or when the heap refuses |
| `allocate_as<T>()` | typed uninitialised storage; asserts and returns `nullptr` when `T` is larger than `block_size()` or more aligned than `alignment()` |
| `construct<T>(args...)` / `destroy(object)` | build and tear down an object in a block |
| `shrink_to_fit()` | frees every slab with no live block |
| `live_count()` / `capacity()` / `slab_count()` / `block_size()` / `alignment()` / `owns(pointer)` | statistics and checks |

> [!TIP]
> Nothing is allocated until the first `allocate()`, so a pool can be a plain member of a class that is often
> left unused. `block_size()` reports the effective block size (rounded up to the alignment and to a pointer).
> `alignment()` reports the requested alignment raised to at least `alignof(void *)`, because a free block stores
> its link in place; `allocate_as<T>()` checks `T` against that same alignment, so on a 64-bit build a pool created
> with alignment 4 still hands out blocks for a `u64`.

> [!WARNING]
> Returning a pointer the pool does not own, a pointer into the middle of a block, or a block that is already
> free asserts and is ignored (the pool keeps a free bitmap per slab, so double frees are caught). The pool is not
> thread-safe.

## Per-frame scratch memory: `frame_allocator`

A `frame_allocator(arenaBytes, threadBlockBytes)` serves short-lived allocations from any thread without locks.
The arena is carved into thread blocks; each thread bumps inside its own block, grabs another when it runs out, and
falls back to the heap when the arena is exhausted (`overflow_count()` and `overflow_bytes()` tell you). The engine
owns one and calls `reset()` at the start of every frame with no job in flight; your code only allocates from it.

| Call | Behaviour |
|---|---|
| `allocate(bytes, alignment = DEFAULT_ALIGNMENT)` | thread-safe, lock-free; valid until the next `reset()` |
| `allocate_span<T>(count)` | uninitialised storage for `count` trivially destructible `T`s, or an empty span |
| `construct<T>(args...)` | a trivially destructible `T` that lives until the next `reset()` |
| `reset()` | starts a new frame; frees overflow blocks and invalidates every block |
| `high_water_bytes()` | the largest frame seen; size the arena from it |
| `capacity_bytes()` / `used_bytes()` / `thread_block_bytes()` / `overflow_count()` / `overflow_bytes()` / `epoch()` | statistics |

```cpp
#include "conjure/core/memory/frame_allocator.h"

#include <span>

using namespace conjure;
using namespace conjure::core::memory;

// Visible indices for this frame only; the span dies with the frame, nothing to free.
std::span<u32> collect_visible(frame_allocator &frame, std::span<const bool> visible) {
    std::span<u32> indices = frame.allocate_span<u32>(visible.size());
    if (indices.size() != visible.size()) {
        return {};                              // heap fallback failed too
    }
    usize count = 0;
    for (usize index = 0; index < visible.size(); ++index) {
        if (visible[index]) {
            indices[count] = static_cast<u32>(index);
            ++count;
        }
    }
    return indices.subspan(0, count);
}
```

> [!CAUTION]
> Memory from a `frame_allocator` is valid only until the next `reset()`, which the engine performs at the start
> of the next frame. Never store a frame pointer in a component or any object that outlives the frame.

> [!WARNING]
> `reset()` must not run concurrently with `allocate()`; the engine guarantees that for its own allocator. If you
> create your own, you must guarantee it too. `allocate_span<T>()` and `construct<T>()` construct nothing you can
> rely on being destroyed, so `T` must be trivially destructible.

> [!NOTE]
> Arena-size tuning: watch `high_water_bytes()` and `overflow_count()` during a representative session; an arena
> that never overflows is big enough, and one that overflows often is paying heap costs every frame.
<!-- doc-content:body:end -->

## Functions

<!-- doc-content:functions:start -->
<a id="api-summary"></a>

| Signature (short) | Purpose |
|---|---|
| `allocation_counters &counters()` / `memory_snapshot snapshot()` | process-wide counters |
| `u64 memory_snapshot::live_count() const` | allocations not yet freed |
| `void record_allocation(usize bytes)` / `void record_free()` | count memory obtained elsewhere |
| `void *tracked_allocate(usize bytes, usize alignment = DEFAULT_ALIGNMENT)` | counted aligned block or `nullptr` |
| `void tracked_free(void *pointer, usize bytes)` | release a tracked block |
| `void *tracked_reallocate(void *pointer, usize oldBytes, usize newBytes, usize alignment = DEFAULT_ALIGNMENT)` | resize a tracked block |
| `DEFAULT_ALIGNMENT` / `MAX_ALLOCATION_BYTES` | `alignof(std::max_align_t)` / `PTRDIFF_MAX` |
| `bool is_power_of_two(usize)` / `usize align_up(usize value, usize alignment)` / `bool is_aligned(const void *, usize)` | alignment helpers |
| `arena(usize blockBytes = DEFAULT_ARENA_BLOCK_BYTES)` | bump allocator |
| `void *arena::allocate(usize bytes, usize alignment)` / `T *construct<T>(Args &&...)` / `void reset()` / `void release()` | arena operations |
| `pool_allocator(usize blockSize, usize alignment, u32 blocksPerSlab, u32 maxSlabs = 0)` | fixed-size blocks |
| `void *pool_allocator::allocate()` / `void deallocate(void *)` / `T *construct<T>(Args &&...)` / `void destroy(T *)` / `void shrink_to_fit()` | pool operations |
| `frame_allocator(usize arenaBytes, usize threadBlockBytes)` | per-frame scratch |
| `void *frame_allocator::allocate(usize bytes, usize alignment)` / `std::span<T> allocate_span<T>(usize)` / `T *construct<T>(Args &&...)` / `void reset()` | frame operations |
| `usize high_water_bytes() const` / `u32 overflow_count() const` / `usize overflow_bytes() const` | frame statistics |
<!-- doc-content:functions:end -->

------------------------------------------

**See also:**

<!-- doc-content:related:start -->
- 📘 API: [Containers](containers.md): the usual way to hold many objects without allocating by hand
- 📘 API: [Data types](data-types.md): `KIB`, `MIB`, `CACHE_LINE_SIZE`, `byte`
- 📘 API: [Glossary](../glossary.md)
<!-- doc-content:related:end -->

<!-- doc-legacy-anchors:start -->
<a id="see-also"></a>
<!-- doc-legacy-anchors:end -->

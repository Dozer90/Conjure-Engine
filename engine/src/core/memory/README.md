# core/memory

Counted heap allocation and the engine's allocators. Everything that needs heap memory outside STL
containers goes through this area, so the process-wide counters see every block the engine acquires.
Design: `docs/design/10_core.md` section 2.5. Public headers: `conjure/core/memory/`, umbrella
`conjure/core/memory/memory.h`. Namespace `conjure::core::memory`.

## Types

- `allocation_counters.h`: `allocation_counters` (three cumulative atomics), `memory_snapshot`,
  `counters()`, `snapshot()`, `record_allocation()`, `record_free()`.
- `tracked_allocation.h`: `tracked_allocate(bytes, alignment)`, `tracked_free(pointer, bytes)`,
  `tracked_reallocate(...)`, `DEFAULT_ALIGNMENT`, `MAX_ALLOCATION_BYTES`, `is_power_of_two`, `align_up`,
  `is_aligned`.
- `arena.h`: `arena`, a bump allocator over a chain of blocks; `reset()` keeps the first block,
  `release()` frees all, `construct<T>` for trivially destructible types.
- `pool_allocator.h`: `pool_allocator(blockSize, alignment, blocksPerSlab, maxSlabs = 0)`, fixed-size
  blocks in slabs; `allocate()` returns nullptr on exhaustion, `shrink_to_fit()` frees empty slabs.
- `frame_allocator.h`: `frame_allocator(arenaBytes, threadBlockBytes)`, per-frame scratch memory with
  per-thread blocks and heap overflow; `reset()` once per frame.

Counting is cumulative: `allocationCount`, `freeCount` and `allocatedBytes` are never decremented, so
"live allocations" is `allocationCount - freeCount` (`memory_snapshot::live_count()`) and live bytes are
not tracked (see `docs/design/08_testing.md`). `tracked_reallocate` counts as one free plus one allocation.

`tracked_allocation.cpp` is the only platform backend of the area: `_aligned_malloc` / `_aligned_free` /
`_aligned_realloc` on Windows, `posix_memalign` / `free` elsewhere (no aligned realloc exists on POSIX, so
`tracked_reallocate` always allocates anew and copies there). Alignments below `DEFAULT_ALIGNMENT` are
raised to it; sizes are passed through unrounded. Requests above `MAX_ALLOCATION_BYTES` (`PTRDIFF_MAX`)
are refused before the runtime sees them: they can never be satisfied, and rejecting them keeps the size
arithmetic of the allocators built on top from wrapping (and keeps sanitizers quiet).

`DEFAULT_ALIGNMENT` is `alignof(std::max_align_t)` as the design mandates. On MSVC x64 that is 8 while
`__STDCPP_DEFAULT_NEW_ALIGNMENT__` is 16, so it is *not* what `operator new` guarantees: a global
`operator new` replacement (the test support's `tracked_global_new.cpp`) must call
`tracked_allocate(size, __STDCPP_DEFAULT_NEW_ALIGNMENT__)` and forward the alignment of the aligned
overloads, or 16-byte aligned types end up under-aligned on MSVC only.

## Allocator behaviour

- `arena`: a request that does not fit a regular block gets a dedicated block linked behind the current
  one, so the current block keeps serving small requests. `reset()` frees every block but the first and
  rewinds it; `release()` frees everything and the next `allocate()` starts over. Addresses are stable
  until either call. `bytes_allocated()` counts requested bytes, `bytes_reserved()` block capacities.
  A moved-from arena is empty and usable.
- `pool_allocator`: the requested alignment is raised to at least `alignof(void *)`, because a free
  block stores its free-list link in place; `alignment()` reports that raised value and `allocate_as()`
  checks types against it. The block stride is `blockSize` rounded up to that alignment and to a
  pointer. Each slab allocation is the blocks followed by a free bitmap (one bit per block); the slab
  table is a sorted, `tracked_allocate`-backed array (8 entries, then doubling through
  `tracked_reallocate`) obtained on the first `allocate()`, so the pool is fully counted, allocates
  nothing until used and never throws. `allocate()`, `deallocate()` and `owns()` are O(log slabCount)
  and per-slab live counts stay exact, which is what lets `shrink_to_fit()` release exactly the fully
  free slabs (the table keeps its capacity). `deallocate()` of a pointer the pool does not own, of a
  pointer inside a block, or of a block that is already free (the bitmap catches a double free whatever
  its neighbours are doing) asserts and is ignored. A slab larger than `MAX_ALLOCATION_BYTES` can never
  be obtained: `allocate()` returns nullptr. A moved-from pool is empty and usable.
- `frame_allocator`: one arena of `arenaBytes` (rounded up to whole thread blocks of `threadBlockBytes`,
  itself clamped and rounded up to cache lines). A thread claims a block with one compare-exchange that
  never moves the offset past the capacity (so `used_bytes()` is exact and a failed claim marks the
  arena exhausted in the thread's slot until the next `reset()`), then bumps inside it through a
  `thread_local` slot `{owner, epoch, cursor, end, arenaExhausted}`. A request that does not fit the
  remaining block claims a new block; one larger than a block, or one made after the arena is exhausted,
  falls back to `tracked_allocate` (`overflow_count()` / `overflow_bytes()`). An arena the heap cannot
  provide (size wraps, above `MAX_ALLOCATION_BYTES`, or refused) degrades to heap-only:
  `capacity_bytes()` is 0 and everything overflows. `reset()` frees the overflow blocks, rewinds the
  arena and takes a new epoch from a process-wide counter, so a stale slot (previous frame, other
  allocator, or an allocator that has been destroyed) never matches and is never bumped from.
  `high_water_bytes()` is the largest frame seen (claimed arena bytes plus overflow bytes).

## Threading model

- Counters: lock-free relaxed atomics, callable from any thread at any time (function-local static).
- `tracked_*`: thread-safe (the system allocator is).
- `arena`, `pool_allocator`: **not** thread-safe; the owner serialises access (the ECS wraps its chunk
  pool under the world lock).
- `frame_allocator::allocate()` and the getters (`high_water_bytes()` included: every value they read is
  atomic): thread-safe and lock-free (the overflow path pushes onto a lock-free stack). `reset()` is not
  thread-safe: the caller guarantees no concurrent `allocate()` (the engine resets at FRAME_BEGIN with no
  job in flight).

## Testing

`tests/core/memory/` builds `conjure_core_memory_tests` (one `test_<header>.cpp` per public header):

```
python tools/build.py --build-dir build/core_memory --target conjure_core_memory_tests --test -R core_memory
build/core_memory/bin/conjure_core_memory_tests        # doctest output
```

The frame allocator tests use `std::thread` directly (the jobs area may not exist yet) to prove that
concurrent threads receive disjoint ranges and that `reset()` invalidates the cached block of another
thread. Everything is headless and deterministic (seeded `std::mt19937_64`). Size-limit tests use
`SIZE_MAX`-based requests that are rejected before any system call, so they stay deterministic under
sanitizers. Tests that check exact allocation counts keep every non-tracked heap activity (thread
start-up, `std::vector` growth) outside the measured window, so they will keep holding once the test
support routes global `operator new` through the counters (wave 1 stage 2).

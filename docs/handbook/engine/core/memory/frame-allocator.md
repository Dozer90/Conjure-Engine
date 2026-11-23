---
kind: header
---

[Engine](../../../engine.md) → [Core](../../core.md)

# frame_allocator.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/core/memory/frame_allocator.h` |
| API ↗ | [Memory](../../../../api/core/memory.md) |

<p>Declarations for <code><a href="frame-allocator/memory-frame-allocator.html">frame_allocator</a>.h</code>.</p>



## Classes

| File | Description |
|---|---|
| [`memory::frame_allocator`](frame-allocator/memory-frame-allocator.md) | Per-frame scratch memory shared by every thread. One arena of <code>arenaBytes</code> is carved into thread blocks of <code>threadBlockBytes</code> with a single atomic bump; each thread then allocates inside its own block from a <code>thread_local</code> slot without any synchronisation. When a request does not fit the thread&#x27;s block the thread grabs a new one (the remainder of the old block is abandoned); when the arena is exhausted, or a request is larger than a block, the allocation falls back to <code>tracked_allocate</code> and <code>overflow_count()</code> grows. Overflow blocks are released by <code>reset()</code>. Thread safety: <code>allocate()</code> and the getters may be called from any thread concurrently and are lock-free (overflow uses a lock-free push; every counter the getters read is atomic). <code>reset()</code> must not run concurrently with <code>allocate()</code>: the engine resets at <code>FRAME_BEGIN</code> with no job in flight. <code>reset()</code> bumps the epoch, which invalidates every thread&#x27;s cached block so a stale <code>thread_local</code> cursor is never reused. Implementation note: the <code>thread_local</code> slot is {owner, epoch, cursor, end, arenaExhausted}; a thread that alternates between two frame allocators re-acquires a block on every switch. Epochs are process-unique, so a slot left over from a destroyed allocator never matches a new one that happens to live at the same address. |

## Forward Declarations

### `memory::frame_allocator::overflow_block` {#symbol-memory-frame-allocator-overflow-block}

```cpp
struct overflow_block;
```

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="frame_allocator"></a>
<a id="frame_allocatoracquire_block"></a>
<a id="frame_allocatorallocate"></a>
<a id="frame_allocatorallocate_overflow"></a>
<a id="frame_allocatorallocate_span"></a>
<a id="frame_allocatorcapacity_bytes"></a>
<a id="frame_allocatorconstruct"></a>
<a id="frame_allocatorepoch"></a>
<a id="frame_allocatorfits_in_fresh_block"></a>
<a id="frame_allocatorframe_allocator"></a>
<a id="frame_allocatorframe_allocator_1"></a>
<a id="frame_allocatorfree_overflow_blocks"></a>
<a id="frame_allocatorhigh_water_bytes"></a>
<a id="frame_allocatormarena"></a>
<a id="frame_allocatormcapacitybytes"></a>
<a id="frame_allocatormepoch"></a>
<a id="frame_allocatormhighwaterbytes"></a>
<a id="frame_allocatormoffset"></a>
<a id="frame_allocatormoverflowbytes"></a>
<a id="frame_allocatormoverflowcount"></a>
<a id="frame_allocatormoverflowhead"></a>
<a id="frame_allocatormthreadblockbytes"></a>
<a id="frame_allocatoroperator"></a>
<a id="frame_allocatoroverflow_block"></a>
<a id="frame_allocatoroverflow_bytes"></a>
<a id="frame_allocatoroverflow_count"></a>
<a id="frame_allocatorreset"></a>
<a id="frame_allocatorthread_block_bytes"></a>
<a id="frame_allocatorused_bytes"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="memoryframe_allocator"></a>
<a id="memoryframe_allocatoracquire_block"></a>
<a id="memoryframe_allocatorallocate"></a>
<a id="memoryframe_allocatorallocate_overflow"></a>
<a id="memoryframe_allocatorallocate_span"></a>
<a id="memoryframe_allocatorcapacity_bytes"></a>
<a id="memoryframe_allocatorconstruct"></a>
<a id="memoryframe_allocatorepoch"></a>
<a id="memoryframe_allocatorfits_in_fresh_block"></a>
<a id="memoryframe_allocatorframe_allocator"></a>
<a id="memoryframe_allocatorframe_allocator_1"></a>
<a id="memoryframe_allocatorfree_overflow_blocks"></a>
<a id="memoryframe_allocatorhigh_water_bytes"></a>
<a id="memoryframe_allocatormarena"></a>
<a id="memoryframe_allocatormcapacitybytes"></a>
<a id="memoryframe_allocatormepoch"></a>
<a id="memoryframe_allocatormhighwaterbytes"></a>
<a id="memoryframe_allocatormoffset"></a>
<a id="memoryframe_allocatormoverflowbytes"></a>
<a id="memoryframe_allocatormoverflowcount"></a>
<a id="memoryframe_allocatormoverflowhead"></a>
<a id="memoryframe_allocatormthreadblockbytes"></a>
<a id="memoryframe_allocatoroperator"></a>
<a id="memoryframe_allocatoroverflow_block"></a>
<a id="memoryframe_allocatoroverflow_bytes"></a>
<a id="memoryframe_allocatoroverflow_count"></a>
<a id="memoryframe_allocatorreset"></a>
<a id="memoryframe_allocatorthread_block_bytes"></a>
<a id="memoryframe_allocatorused_bytes"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->

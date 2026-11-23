---
kind: header
---

[Engine](../../../engine.md) → [Core](../../core.md)

# pool_allocator.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/core/memory/pool_allocator.h` |
| API ↗ | [Memory](../../../../api/core/memory.md) |

<p>Declarations for <code><a href="pool-allocator/memory-pool-allocator.html">pool_allocator</a>.h</code>.</p>

## Structs

| File | Description |
|---|---|
| [`memory::pool_allocator::free_block`](pool-allocator/memory-pool-allocator-free-block.md) |  |
| [`memory::pool_allocator::slab`](pool-allocator/memory-pool-allocator-slab.md) |  |

## Classes

| File | Description |
|---|---|
| [`memory::pool_allocator`](pool-allocator/memory-pool-allocator.md) | Fixed-size block allocator. Blocks are carved from slabs of <code>blocksPerSlab</code> blocks; the pool grows by one slab whenever the free list is empty (up to <code>maxSlabs</code> when a cap is given) and <code>shrink_to_fit()</code> returns slabs with no live block. Blocks are pure data: a block is only used as a free-list link while it is free, so a 16 KiB block really holds 16 KiB. <code>allocate()</code> and <code>deallocate()</code> are <code>O(log slabCount)</code>: a sorted slab table resolves the owning slab, which keeps per-slab live counts exact and lets every slab carry a free bitmap, so a double free is detected (asserted and ignored) even while neighbouring blocks are live. Every <code><a href="../types.html#symbol-byte">byte</a></code> the pool holds, the slab table included, comes from <code>tracked_allocate</code>: the pool is fully counted and never throws. Not thread-safe: the ECS wraps a pool under its own lock. |

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
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="memorypool_allocator"></a>
<a id="memorypool_allocatoralignment"></a>
<a id="memorypool_allocatorallocate"></a>
<a id="memorypool_allocatorallocate_as"></a>
<a id="memorypool_allocatorblock_index"></a>
<a id="memorypool_allocatorblock_size"></a>
<a id="memorypool_allocatorblocks_per_slab"></a>
<a id="memorypool_allocatorcapacity"></a>
<a id="memorypool_allocatorconstruct"></a>
<a id="memorypool_allocatordeallocate"></a>
<a id="memorypool_allocatordestroy"></a>
<a id="memorypool_allocatorfind_slab_index"></a>
<a id="memorypool_allocatorfits"></a>
<a id="memorypool_allocatorfree_bits"></a>
<a id="memorypool_allocatorfree_block"></a>
<a id="memorypool_allocatorfree_blocknext"></a>
<a id="memorypool_allocatorgrow"></a>
<a id="memorypool_allocatoris_block_start"></a>
<a id="memorypool_allocatoris_free"></a>
<a id="memorypool_allocatorlive_count"></a>
<a id="memorypool_allocatormalignment"></a>
<a id="memorypool_allocatormark_free"></a>
<a id="memorypool_allocatormax_slabs"></a>
<a id="memorypool_allocatormblockbytes"></a>
<a id="memorypool_allocatormblocksperslab"></a>
<a id="memorypool_allocatormfreelist"></a>
<a id="memorypool_allocatormlivecount"></a>
<a id="memorypool_allocatormmaxslabs"></a>
<a id="memorypool_allocatormslabbytes"></a>
<a id="memorypool_allocatormslabcapacity"></a>
<a id="memorypool_allocatormslabcount"></a>
<a id="memorypool_allocatormslabs"></a>
<a id="memorypool_allocatoroperator"></a>
<a id="memorypool_allocatorowns"></a>
<a id="memorypool_allocatorpool_allocator"></a>
<a id="memorypool_allocatorpool_allocator_1"></a>
<a id="memorypool_allocatorrelease_slabs"></a>
<a id="memorypool_allocatorreserve_slab_table"></a>
<a id="memorypool_allocatorshrink_to_fit"></a>
<a id="memorypool_allocatorslab"></a>
<a id="memorypool_allocatorslab_count"></a>
<a id="memorypool_allocatorslabblocks"></a>
<a id="memorypool_allocatorslablivecount"></a>
<a id="pool_allocator"></a>
<a id="pool_allocatoralignment"></a>
<a id="pool_allocatorallocate"></a>
<a id="pool_allocatorallocate_as"></a>
<a id="pool_allocatorblock_index"></a>
<a id="pool_allocatorblock_size"></a>
<a id="pool_allocatorblocks_per_slab"></a>
<a id="pool_allocatorcapacity"></a>
<a id="pool_allocatorconstruct"></a>
<a id="pool_allocatordeallocate"></a>
<a id="pool_allocatordestroy"></a>
<a id="pool_allocatorfind_slab_index"></a>
<a id="pool_allocatorfits"></a>
<a id="pool_allocatorfree_bits"></a>
<a id="pool_allocatorfree_block"></a>
<a id="pool_allocatorfree_blocknext"></a>
<a id="pool_allocatorgrow"></a>
<a id="pool_allocatoris_block_start"></a>
<a id="pool_allocatoris_free"></a>
<a id="pool_allocatorlive_count"></a>
<a id="pool_allocatormalignment"></a>
<a id="pool_allocatormark_free"></a>
<a id="pool_allocatormax_slabs"></a>
<a id="pool_allocatormblockbytes"></a>
<a id="pool_allocatormblocksperslab"></a>
<a id="pool_allocatormfreelist"></a>
<a id="pool_allocatormlivecount"></a>
<a id="pool_allocatormmaxslabs"></a>
<a id="pool_allocatormslabbytes"></a>
<a id="pool_allocatormslabcapacity"></a>
<a id="pool_allocatormslabcount"></a>
<a id="pool_allocatormslabs"></a>
<a id="pool_allocatoroperator"></a>
<a id="pool_allocatorowns"></a>
<a id="pool_allocatorpool_allocator"></a>
<a id="pool_allocatorpool_allocator_1"></a>
<a id="pool_allocatorrelease_slabs"></a>
<a id="pool_allocatorreserve_slab_table"></a>
<a id="pool_allocatorshrink_to_fit"></a>
<a id="pool_allocatorslab"></a>
<a id="pool_allocatorslab_count"></a>
<a id="pool_allocatorslabblocks"></a>
<a id="pool_allocatorslablivecount"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->

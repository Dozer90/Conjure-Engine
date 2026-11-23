---
kind: header
---

[Engine](../../../engine.md) → [Core](../../core.md)

# arena.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/core/memory/arena.h` |
| API ↗ | [Memory](../../../../api/core/memory.md) |

<p>Declarations for <code>arena.h</code>.</p>



## Classes

| File | Description |
|---|---|
| [`memory::arena`](arena/memory-arena.md) | Linear (bump) allocator over a chain of fixed-size blocks. Individual allocations are never freed; <code>reset()</code> recycles the whole arena at once, which is what command buffers, parsers and per-load scratch memory want. Addresses are stable until <code>reset()</code> or <code>release()</code>. <code>A</code> request that does not fit in a block gets a dedicated block of its own; the current block keeps serving smaller requests. Blocks are obtained with <code>tracked_allocate</code>, so every acquisition shows up in the allocation counters. Not thread-safe: one arena belongs to one thread (or to a lock held by its owner). |

## Forward Declarations

### `memory::arena::block` {#symbol-memory-arena-block}

```cpp
struct block;
```

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-memory::DEFAULT_ARENA_BLOCK_BYTES"></a><code>memory::DEFAULT_ARENA_BLOCK_BYTES</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../types.html#symbol-usize">usize</a></code> | <code class="signature"><span class="mi">64</span> * KIB</code> | Block size an arena uses when the caller does not choose one. |

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="arena"></a>
<a id="arenaacquire_block"></a>
<a id="arenaallocate"></a>
<a id="arenaarena"></a>
<a id="arenaarena_1"></a>
<a id="arenablock"></a>
<a id="arenablock_bytes"></a>
<a id="arenablock_count"></a>
<a id="arenabytes_allocated"></a>
<a id="arenabytes_reserved"></a>
<a id="arenaconstruct"></a>
<a id="arenafree_block"></a>
<a id="arenamblockbytes"></a>
<a id="arenamblockcount"></a>
<a id="arenambytesallocated"></a>
<a id="arenambytesreserved"></a>
<a id="arenamcurrent"></a>
<a id="arenamfirst"></a>
<a id="arenaoperator"></a>
<a id="arenaowns"></a>
<a id="arenarelease"></a>
<a id="arenareset"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="default_arena_block_bytes"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="memoryarena"></a>
<a id="memoryarenaacquire_block"></a>
<a id="memoryarenaallocate"></a>
<a id="memoryarenaarena"></a>
<a id="memoryarenaarena_1"></a>
<a id="memoryarenablock"></a>
<a id="memoryarenablock_bytes"></a>
<a id="memoryarenablock_count"></a>
<a id="memoryarenabytes_allocated"></a>
<a id="memoryarenabytes_reserved"></a>
<a id="memoryarenaconstruct"></a>
<a id="memoryarenafree_block"></a>
<a id="memoryarenamblockbytes"></a>
<a id="memoryarenamblockcount"></a>
<a id="memoryarenambytesallocated"></a>
<a id="memoryarenambytesreserved"></a>
<a id="memoryarenamcurrent"></a>
<a id="memoryarenamfirst"></a>
<a id="memoryarenaoperator"></a>
<a id="memoryarenaowns"></a>
<a id="memoryarenarelease"></a>
<a id="memoryarenareset"></a>
<a id="memorydefault_arena_block_bytes"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->

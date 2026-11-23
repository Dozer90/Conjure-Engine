---
kind: header
---

[Engine](../../engine.md) → [Engine](../engine.md)

# frame_memory.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/engine/frame_memory.h` |
| API ↗ | [The frame, layers and time](../../../api/engine/frame-layers-and-time.md), [Engine: the application and the frame](../../../api/engine/index.md) |

<p>Per-frame scratch memory: one <code><a href="../core/memory/frame-allocator/memory-frame-allocator.html">core::memory::frame_allocator</a></code> with per-thread blocks, reset by the main thread at <code>FRAME_BEGIN</code> with no scheduler job in flight (every job of frame <code>N</code>-1 completed at <code>FRAME_END</code>, so the reset is safe by construction). Anything allocated here is dead at the next <code>FRAME_BEGIN</code>; an arena overflow falls back to the heap, is counted and warned about once per <code>OVERFLOW_WARNING_INTERVAL</code> frames with the high-water mark.</p>

## Structs

| File | Description |
|---|---|
| [`frame_memory_stats`](frame-memory/frame-memory-stats.md) |  |

## Classes

| File | Description |
|---|---|
| [`frame_memory`](frame-memory/frame-memory.md) |  |

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
<a id="frame_memory"></a>
<a id="frame_memory_stats"></a>
<a id="frame_memory_statscapacitybytes"></a>
<a id="frame_memory_statshighwaterbytes"></a>
<a id="frame_memory_statsoperator"></a>
<a id="frame_memory_statsoverflowcount"></a>
<a id="frame_memoryallocate"></a>
<a id="frame_memoryallocate_span"></a>
<a id="frame_memoryallocator"></a>
<a id="frame_memoryconstruct"></a>
<a id="frame_memoryepoch"></a>
<a id="frame_memoryframe_memory"></a>
<a id="frame_memoryframe_memory_1"></a>
<a id="frame_memoryis_phase_in_flight"></a>
<a id="frame_memorymallocator"></a>
<a id="frame_memorymepoch"></a>
<a id="frame_memorymlastwarningreset"></a>
<a id="frame_memorymoverflowcount"></a>
<a id="frame_memorymphaseinflight"></a>
<a id="frame_memorymresetcount"></a>
<a id="frame_memorymwarnedonce"></a>
<a id="frame_memoryoperator"></a>
<a id="frame_memoryoverflow_warning_interval"></a>
<a id="frame_memoryreset"></a>
<a id="frame_memoryreset_count"></a>
<a id="frame_memoryset_phase_in_flight"></a>
<a id="frame_memorystats"></a>
<a id="frame_memoryused_bytes"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->

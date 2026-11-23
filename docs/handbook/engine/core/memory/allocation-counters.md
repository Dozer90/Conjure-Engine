---
kind: header
---

[Engine](../../../engine.md) → [Core](../../core.md)

# allocation_counters.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/core/memory/allocation_counters.h` |
| API ↗ | [Memory](../../../../api/core/memory.md) |

<p>Process-wide allocation counters.</p>

## Structs

| File | Description |
|---|---|
| [`memory::allocation_counters`](allocation-counters/memory-allocation-counters.md) | The three cumulative counters. Relaxed atomics: readers only need eventual, monotonic values. |
| [`memory::memory_snapshot`](allocation-counters/memory-memory-snapshot.md) | <code>A</code> plain copy of the counters taken at one point in time. |

## Functions

### Public

#### `memory::counters` {#function-memory-counters}

<div class="function-declaration" id="function-memory-counters-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="allocation-counters/memory-allocation-counters.html">allocation_counters</a> &amp;counters() <span class="k">noexcept</span>;</code></pre></div>

<p>The process-wide counters. Safe to call from any thread, including during static initialisation.</p>

#### `memory::record_allocation` {#function-memory-record-allocation}

<div class="function-declaration" id="function-memory-record-allocation-1"><pre><code><span class="na">CONJURE_API</span> <span class="k">void</span> record_allocation(<a href="../types.html#symbol-usize">usize</a> bytes) <span class="k">noexcept</span>;</code></pre></div>

<p>Records one allocation of <code>bytes</code> bytes. Called by <code>tracked_allocate</code> and by code that acquires memory through another route (global operator new in tests, physics allocation hooks).</p>

#### `memory::record_free` {#function-memory-record-free}

<div class="function-declaration" id="function-memory-record-free-1"><pre><code><span class="na">CONJURE_API</span> <span class="k">void</span> record_free() <span class="k">noexcept</span>;</code></pre></div>

<p>Records one free. Counterpart of <code>record_allocation</code>.</p>

#### `memory::snapshot` {#function-memory-snapshot}

<div class="function-declaration" id="function-memory-snapshot-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="allocation-counters/memory-memory-snapshot.html">memory_snapshot</a> snapshot() <span class="k">noexcept</span>;</code></pre></div>

<p>Copies the current counter values. The three loads are not one atomic transaction; callers comparing snapshots quiesce other threads first (test fixtures) or accept small skews.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="allocation_counters"></a>
<a id="allocation_countersallocatedbytes"></a>
<a id="allocation_countersallocationcount"></a>
<a id="allocation_countersfreecount"></a>
<a id="classes"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="counters"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="memory_snapshot"></a>
<a id="memory_snapshotallocatedbytes"></a>
<a id="memory_snapshotallocationcount"></a>
<a id="memory_snapshotfreecount"></a>
<a id="memory_snapshotlive_count"></a>
<a id="memory_snapshotoperator"></a>
<a id="memoryallocation_counters"></a>
<a id="memoryallocation_countersallocatedbytes"></a>
<a id="memoryallocation_countersallocationcount"></a>
<a id="memoryallocation_countersfreecount"></a>
<a id="memorycounters"></a>
<a id="memorymemory_snapshot"></a>
<a id="memorymemory_snapshotallocatedbytes"></a>
<a id="memorymemory_snapshotallocationcount"></a>
<a id="memorymemory_snapshotfreecount"></a>
<a id="memorymemory_snapshotlive_count"></a>
<a id="memorymemory_snapshotoperator"></a>
<a id="memoryrecord_allocation"></a>
<a id="memoryrecord_free"></a>
<a id="memorysnapshot"></a>
<a id="record_allocation"></a>
<a id="record_free"></a>
<a id="snapshot"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->

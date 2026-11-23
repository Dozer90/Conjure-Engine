---
kind: header
---

[Engine](../../../engine.md) → [Core](../../core.md)

# tracked_allocation.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/core/memory/tracked_allocation.h` |
| API ↗ | [Memory](../../../../api/core/memory.md) |

<p>Counted, aligned heap allocation.</p>

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-memory::DEFAULT_ALIGNMENT"></a><code>memory::DEFAULT_ALIGNMENT</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../types.html#symbol-usize">usize</a></code> | <code class="signature">alignof(std::<wbr>max_<wbr>align_<wbr>t)</code> | Alignment used when a caller does not ask for a specific one. Every allocation is at least this aligned, whatever smaller value was requested. Trap: <code>alignof(std::max_align_t)</code> is 8 on MSVC x64 but <code>__STDCPP_DEFAULT_NEW_ALIGNMENT__</code> is 16 there, so this is *not* the alignment operator new guarantees. <code>A</code> replacement of global operator new (the test support&#x27;s <code>tracked_global_new.cpp</code>) must pass <code>__STDCPP_DEFAULT_NEW_ALIGNMENT__</code> to <code>tracked_allocate</code> explicitly, and its aligned overloads must forward their alignment; relying on this default would hand out under-aligned storage for 16-<code><a href="../types.html#symbol-byte">byte</a></code> aligned types (SSE and glm aligned types) on MSVC only. |
| <a id="variable-memory::MAX_ALLOCATION_BYTES"></a><code>memory::MAX_ALLOCATION_BYTES</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../types.html#symbol-usize">usize</a></code> | <code class="signature">static_<wbr>cast&lt;<wbr><a href="../types.html#symbol-usize">usize</a>&gt;(std::<wbr>numeric_<wbr>limits&lt;<wbr><a href="../types.html#symbol-isize">isize</a>&gt;::<wbr>max())</code> | Largest request <code>tracked_allocate</code> and <code>tracked_reallocate</code> accept: <code>PTRDIFF_MAX</code>. No object may be larger (pointer differences inside it would not be representable), every <code>C</code> runtime refuses such requests, and rejecting them here keeps the size arithmetic of the allocators built on top from wrapping. Larger requests fail like any other out-of-memory condition: <code>nullptr</code>, no assert, counters untouched. |

## Functions

### Public

#### `memory::align_up` {#function-memory-align-up}

<div class="function-declaration" id="function-memory-align-up-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="../types.html#symbol-usize">usize</a> align_up(<a href="../types.html#symbol-usize">usize</a> value, <a href="../types.html#symbol-usize">usize</a> alignment) <span class="k">noexcept</span>;</code></pre></div>

<p>Rounds <code>value</code> up to the next multiple of <code>alignment</code> (a power of two). Returns <code>value</code> when it is already aligned. Wraps for values within <code>alignment</code> of the <code><a href="../types.html#symbol-usize">usize</a></code> maximum; callers that cannot rule that out check first.</p>

#### `memory::is_aligned` {#function-memory-is-aligned}

<div class="function-declaration" id="function-memory-is-aligned-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <span class="k">bool</span> is_aligned(<span class="k">const</span> <span class="k">void</span> *pointer, <a href="../types.html#symbol-usize">usize</a> alignment) <span class="k">noexcept</span>;</code></pre></div>

<p>True when the address of <code>pointer</code> is a multiple of <code>alignment</code> (a power of two). <code>nullptr</code> is aligned to everything.</p>

#### `memory::is_power_of_two` {#function-memory-is-power-of-two}

<div class="function-declaration" id="function-memory-is-power-of-two-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> is_power_of_two(<a href="../types.html#symbol-usize">usize</a> value) <span class="k">noexcept</span>;</code></pre></div>

<p>True for 1, 2, 4, 8, ... and <code>false</code> for 0.</p>

#### `memory::tracked_allocate` {#function-memory-tracked-allocate}

<div class="function-declaration" id="function-memory-tracked-allocate-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <span class="k">void</span> *tracked_allocate(<a href="../types.html#symbol-usize">usize</a> bytes, <a href="../types.html#symbol-usize">usize</a> alignment = DEFAULT_ALIGNMENT) <span class="k">noexcept</span>;</code></pre></div>

<p>Allocates <code>bytes</code> bytes whose address is a multiple of <code>alignment</code> (a power of two; values below <code>DEFAULT_ALIGNMENT</code> are raised to it). <code>A</code> zero-<code><a href="../types.html#symbol-byte">byte</a></code> request returns a unique, non-<code>null</code> pointer like operator new does. Returns <code>nullptr</code> when the system is out of memory, when <code>bytes</code> exceeds <code>MAX_ALLOCATION_BYTES</code>, or when the alignment is not a power of two (the latter also asserts). Nothing is counted on failure.</p>

#### `memory::tracked_free` {#function-memory-tracked-free}

<div class="function-declaration" id="function-memory-tracked-free-1"><pre><code><span class="na">CONJURE_API</span> <span class="k">void</span> tracked_free(<span class="k">void</span> *pointer, <a href="../types.html#symbol-usize">usize</a> bytes) <span class="k">noexcept</span>;</code></pre></div>

<p>Releases memory obtained from <code>tracked_allocate</code> or <code>tracked_reallocate</code>. <code>bytes</code> is the size that was requested; it is kept for symmetry with the allocation call and for future per-size statistics, the counters themselves only count calls. <code>nullptr</code> is ignored and not counted.</p>

#### `memory::tracked_reallocate` {#function-memory-tracked-reallocate}

<div class="function-declaration" id="function-memory-tracked-reallocate-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <span class="k">void</span> *tracked_reallocate( <span class="k">void</span> *pointer, <a href="../types.html#symbol-usize">usize</a> oldBytes, <a href="../types.html#symbol-usize">usize</a> newBytes, <a href="../types.html#symbol-usize">usize</a> alignment = DEFAULT_ALIGNMENT ) <span class="k">noexcept</span>;</code></pre></div>

<p>Resizes an allocation, preserving the first <code>min(oldBytes, newBytes)</code> bytes. <code>alignment</code> must be the alignment the block was allocated with. <code>A</code> <code>nullptr</code> <code>pointer</code> behaves like <code>tracked_allocate</code>, a zero <code>newBytes</code> frees the block and returns <code>nullptr</code>. Counted as one free plus one allocation so live counts stay balanced. On failure (out of memory, <code>newBytes</code> above <code>MAX_ALLOCATION_BYTES</code>, invalid alignment) the original block is untouched and <code>nullptr</code> returned. Platform note: Windows resizes in place when the <code>C</code> runtime can (<code>_aligned_realloc</code>); POSIX has no aligned realloc, so the block is always allocated anew and the preserved bytes copied. Callers that resize often should grow geometrically so the copy stays amortised <code>O(1)</code>.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="align_up"></a>
<a id="classes"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="default_alignment"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="is_aligned"></a>
<a id="is_power_of_two"></a>
<a id="max_allocation_bytes"></a>
<a id="memoryalign_up"></a>
<a id="memorydefault_alignment"></a>
<a id="memoryis_aligned"></a>
<a id="memoryis_power_of_two"></a>
<a id="memorymax_allocation_bytes"></a>
<a id="memorytracked_allocate"></a>
<a id="memorytracked_free"></a>
<a id="memorytracked_reallocate"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="tracked_allocate"></a>
<a id="tracked_free"></a>
<a id="tracked_reallocate"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->

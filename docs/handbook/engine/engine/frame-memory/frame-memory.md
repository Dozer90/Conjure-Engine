---
kind: class
---

[Engine](../../../engine.md) → [Engine](../../engine.md) → [frame_memory.h](../frame-memory.md)

# frame_memory
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::engine` |
| Include File Path | `engine/include/conjure/engine/frame_memory.h` |
| Source File Path | `engine/src/engine/frame_memory.cpp` |
| API ↗ | [The frame, layers and time](../../../../api/engine/frame-layers-and-time.md), [Engine: the application and the frame](../../../../api/engine/index.md) |

## Syntax

```cpp
class CONJURE_API frame_memory
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">explicit</span> frame_memory(
    <span class="k">const</span> <a href="../engine-config/memory-settings.html">memory_settings</a> &amp;settings);</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~frame_memory();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mAllocator</code></td><td><code class="signature"><a href="../../core/memory/frame-allocator/memory-frame-allocator.html">core::<wbr>memory::<wbr>frame_<wbr>allocator</a></code></td><td>—</td><td></td></tr>
<tr><td>private</td><td><code class="signature">mEpoch</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Last<wbr>Warning<wbr>Reset</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Overflow<wbr>Count</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Phase<wbr>In<wbr>Flight</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mResetCount</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mWarnedOnce</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">OVERFLOW_<wbr>WARNING_<wbr>INTERVAL</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">60</span></code></td><td>Resets between two overflow warnings (one second at 60 Hz).</td></tr>
</tbody>
</table>

## Functions

### Public

#### `allocate` {#function-allocate}

<div class="function-declaration" id="function-allocate-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">void</span> *allocate(
    <a href="../../core/types.html#symbol-usize">usize</a> bytes,
    <a href="../../core/types.html#symbol-usize">usize</a> alignment = core::memory::DEFAULT_ALIGNMENT) <span class="k">noexcept</span>;</code></pre></div>

<p>Thread-safe bump from the calling thread&#x27;s block; the arena overflow falls back to the heap. Returns <code>nullptr</code> for a zero size or an alignment that is not a power of two (asserted).</p>

#### `allocate_span` {#function-allocate-span}

<div class="function-declaration" id="function-allocate-span-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> ElementType&gt; [[<span class="na">nodiscard</span>]] std::span&lt;ElementType&gt; allocate_span(
    <a href="../../core/types.html#symbol-usize">usize</a> count) <span class="k">noexcept</span>;</code></pre></div>

<p>Uninitialised storage for <code>count</code> trivially destructible elements; empty for 0 or on failure.</p>

#### `allocator` {#function-allocator}

<div class="function-declaration" id="function-allocator-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../../core/memory/frame-allocator/memory-frame-allocator.html">core::memory::frame_allocator</a> &amp;allocator() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The allocator underneath (for diagnostics and tests).</p>

#### `construct` {#function-construct}

<div class="function-declaration" id="function-construct-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> ElementType, <span class="k">typename</span>... Args&gt; [[<span class="na">nodiscard</span>]] ElementType *construct(
    Args &amp;&amp;...args);</code></pre></div>

<p>Constructs a trivially destructible object that lives until the next <code><a href="#function-reset-1">reset</a>()</code>; <code>nullptr</code> on failure.</p>

#### `epoch` {#function-epoch}

<div class="function-declaration" id="function-epoch-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u64">u64</a> epoch() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Identifies the current frame; changes on every <code><a href="#function-reset-1">reset</a>()</code>.</p>

#### `is_phase_in_flight` {#function-is-phase-in-flight}

<div class="function-declaration" id="function-is-phase-in-flight-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_phase_in_flight() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `reset` {#function-reset}

<div class="function-declaration" id="function-reset-1"><pre><code><span class="k">void</span> reset();</code></pre></div>

<p>Main thread, at <code>FRAME_BEGIN</code>: returns every block, frees the overflow blocks, bumps the epoch and warns about overflows at most once per <code>OVERFLOW_WARNING_INTERVAL</code> resets. Must not run concurrently with <code><a href="#function-allocate-1">allocate</a>()</code>: a call while a scheduler phase is in flight asserts and is refused.</p>

#### `reset_count` {#function-reset-count}

<div class="function-declaration" id="function-reset-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u64">u64</a> reset_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Number of <code><a href="#function-reset-1">reset</a>()</code> calls so far.</p>

#### `set_phase_in_flight` {#function-set-phase-in-flight}

<div class="function-declaration" id="function-set-phase-in-flight-1"><pre><code><span class="k">void</span> set_phase_in_flight(
    <span class="k">bool</span> inFlight) <span class="k">noexcept</span>;</code></pre></div>

<p>The frame loop marks the scheduler phases it runs; <code><a href="#function-reset-1">reset</a>()</code> asserts that none is in flight.</p>

#### `stats` {#function-stats}

<div class="function-declaration" id="function-stats-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="frame-memory-stats.html">frame_memory_stats</a> stats() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>High-water mark (including the current frame), arena capacity and the overflow count of the current frame.</p>

#### `used_bytes` {#function-used-bytes}

<div class="function-declaration" id="function-used-bytes-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-usize">usize</a> used_bytes() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Arena bytes claimed by thread blocks in the current frame.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

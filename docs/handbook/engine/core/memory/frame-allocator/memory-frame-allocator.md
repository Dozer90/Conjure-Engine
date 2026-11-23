---
kind: class
---

[Engine](../../../../engine.md) → [Core](../../../core.md) → [frame_allocator.h](../frame-allocator.md)

# memory::frame_allocator
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::core::memory` |
| Include File Path | `engine/include/conjure/core/memory/frame_allocator.h` |
| Source File Path | `engine/src/core/memory/frame_allocator.cpp` |
| API ↗ | [Memory](../../../../../api/core/memory.md) |

## Syntax

```cpp
class CONJURE_API frame_allocator
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">frame_allocator(
    <a href="../../types.html#symbol-usize">usize</a> arenaBytes,
    <a href="../../types.html#symbol-usize">usize</a> threadBlockBytes);</code></td><td><code>threadBlockBytes</code> is rounded up to a multiple of <code>CACHE_LINE_SIZE</code> (at least one line) so blocks of different threads never share a cache line; <code>arenaBytes</code> is rounded up to a whole number of blocks. An arena of 0 bytes is allowed: every allocation then overflows, which keeps the allocator usable when no frame budget is configured. An arena the heap cannot provide (too large for the address space or refused by the system allocator) degrades the same way: <code>capacity_bytes()</code> reports 0 and every allocation overflows.</td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Additional constructor overloads have been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~frame_allocator();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mArena</code></td><td><code class="signature"><a href="../../types.html#symbol-byte">byte</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Capacity<wbr>Bytes</code></td><td><code class="signature"><a href="../../types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mEpoch</code></td><td><code class="signature">std::atomic&lt;<a href="../../types.html#symbol-u64">u64</a>&gt;</code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>High<wbr>Water<wbr>Bytes</code></td><td><code class="signature">std::atomic&lt;<a href="../../types.html#symbol-usize">usize</a>&gt;</code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mOffset</code></td><td><code class="signature">std::atomic&lt;<a href="../../types.html#symbol-usize">usize</a>&gt;</code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Overflow<wbr>Bytes</code></td><td><code class="signature">std::atomic&lt;<a href="../../types.html#symbol-usize">usize</a>&gt;</code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Overflow<wbr>Count</code></td><td><code class="signature">std::atomic&lt;<a href="../../types.html#symbol-u32">u32</a>&gt;</code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Overflow<wbr>Head</code></td><td><code class="signature">std::<wbr>atomic&lt;<wbr><a href="../frame-allocator.html">overflow_<wbr>block</a> *&gt;</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Thread<wbr>Block<wbr>Bytes</code></td><td><code class="signature"><a href="../../types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `allocate` {#function-allocate}

<div class="function-declaration" id="function-allocate-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">void</span> *allocate(
    <a href="../../types.html#symbol-usize">usize</a> bytes,
    <a href="../../types.html#symbol-usize">usize</a> alignment = DEFAULT_ALIGNMENT) <span class="k">noexcept</span>;</code></pre></div>

<p>Returns <code>bytes</code> bytes aligned to <code>alignment</code> (a power of two) that live until the next <code><a href="#function-reset-1">reset</a>()</code>. Thread-safe and lock-free. Returns <code>nullptr</code> for a zero-<code><a href="../../types.html#symbol-byte">byte</a></code> request, for an alignment that is not a power of two (asserts) and when the heap fallback fails.</p>

#### `allocate_span` {#function-allocate-span}

<div class="function-declaration" id="function-allocate-span-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> T&gt; [[<span class="na">nodiscard</span>]] std::span&lt;T&gt; allocate_span(
    <a href="../../types.html#symbol-usize">usize</a> count) <span class="k">noexcept</span>;</code></pre></div>

<p>Allocates uninitialised storage for <code>count</code> elements of a trivially destructible <code>T</code>: nothing is constructed or destroyed, the caller initialises every element before reading it. Returns an empty span for a count of 0 or when allocation fails.</p>

#### `capacity_bytes` {#function-capacity-bytes}

<div class="function-declaration" id="function-capacity-bytes-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-usize">usize</a> capacity_bytes() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Usable arena bytes (after rounding to whole blocks); 0 when the allocator is heap-only.</p>

#### `construct` {#function-construct}

<div class="function-declaration" id="function-construct-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> T, <span class="k">typename</span>... Args&gt; [[<span class="na">nodiscard</span>]] T *construct(
    Args &amp;&amp;...args);</code></pre></div>

<p>Allocates and constructs a <code>T</code> that lives until the next <code><a href="#function-reset-1">reset</a>()</code>. <code>T</code> must be trivially destructible because <code><a href="#function-reset-1">reset</a>()</code> never runs destructors. Returns <code>nullptr</code> when allocation fails.</p>

#### `epoch` {#function-epoch}

<div class="function-declaration" id="function-epoch-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-u64">u64</a> epoch() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Identifies the current frame. Changes on every <code><a href="#function-reset-1">reset</a>()</code> and is unique across every <code><a href="memory-frame-allocator.html">frame_allocator</a></code> in the process.</p>

#### `high_water_bytes` {#function-high-water-bytes}

<div class="function-declaration" id="function-high-water-bytes-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-usize">usize</a> high_water_bytes() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The largest frame seen so far, including the current one: arena bytes claimed by thread blocks plus bytes served by the heap fallback. Size the arena from this.</p>

#### `overflow_bytes` {#function-overflow-bytes}

<div class="function-declaration" id="function-overflow-bytes-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-usize">usize</a> overflow_bytes() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Bytes served by the heap fallback in the current frame.</p>

#### `overflow_count` {#function-overflow-count}

<div class="function-declaration" id="function-overflow-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-u32">u32</a> overflow_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Allocations served by the heap fallback in the current frame.</p>

#### `reset` {#function-reset}

<div class="function-declaration" id="function-reset-1"><pre><code><span class="k">void</span> reset() <span class="k">noexcept</span>;</code></pre></div>

<p>Starts a new frame: frees the overflow blocks, returns every thread block to the arena and bumps the epoch so cached thread blocks are abandoned. Not thread-safe: no <code><a href="#function-allocate-1">allocate</a>()</code> may run concurrently.</p>

#### `thread_block_bytes` {#function-thread-block-bytes}

<div class="function-declaration" id="function-thread-block-bytes-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-usize">usize</a> thread_block_bytes() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Size of one thread block (after rounding).</p>

#### `used_bytes` {#function-used-bytes}

<div class="function-declaration" id="function-used-bytes-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-usize">usize</a> used_bytes() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Arena bytes claimed by thread blocks in the current frame.</p>

### Private

#### `acquire_block` {#function-acquire-block}

<div class="function-declaration" id="function-acquire-block-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-byte">byte</a> *acquire_block() <span class="k">noexcept</span>;</code></pre></div>

#### `allocate_overflow` {#function-allocate-overflow}

<div class="function-declaration" id="function-allocate-overflow-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">void</span> *allocate_overflow(
    <a href="../../types.html#symbol-usize">usize</a> bytes,
    <a href="../../types.html#symbol-usize">usize</a> alignment) <span class="k">noexcept</span>;</code></pre></div>

#### `fits_in_fresh_block` {#function-fits-in-fresh-block}

<div class="function-declaration" id="function-fits-in-fresh-block-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> fits_in_fresh_block(
    <a href="../../types.html#symbol-usize">usize</a> bytes,
    <a href="../../types.html#symbol-usize">usize</a> alignment) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `free_overflow_blocks` {#function-free-overflow-blocks}

<div class="function-declaration" id="function-free-overflow-blocks-1"><pre><code><span class="k">void</span> free_overflow_blocks() <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

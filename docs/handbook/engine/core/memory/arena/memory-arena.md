---
kind: class
---

[Engine](../../../../engine.md) → [Core](../../../core.md) → [arena.h](../arena.md)

# memory::arena
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::core::memory` |
| Include File Path | `engine/include/conjure/core/memory/arena.h` |
| Source File Path | `engine/src/core/memory/arena.cpp` |
| API ↗ | [Memory](../../../../../api/core/memory.md) |

## Syntax

```cpp
class CONJURE_API arena
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">explicit</span> arena(
    <a href="../../types.html#symbol-usize">usize</a> blockBytes = DEFAULT_ARENA_BLOCK_BYTES);</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">arena(
    <a href="memory-arena.html">arena</a> &amp;&amp;other) <span class="k">noexcept</span>;</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Additional constructor overloads have been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~arena();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mBlockBytes</code></td><td><code class="signature"><a href="../../types.html#symbol-usize">usize</a></code></td><td><code class="signature">DEFAULT_<wbr>ARENA_<wbr>BLOCK_<wbr>BYTES</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mBlockCount</code></td><td><code class="signature"><a href="../../types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Bytes<wbr>Allocated</code></td><td><code class="signature"><a href="../../types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Bytes<wbr>Reserved</code></td><td><code class="signature"><a href="../../types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mCurrent</code></td><td><code class="signature"><a href="../arena.html">block</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mFirst</code></td><td><code class="signature"><a href="../arena.html">block</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `allocate` {#function-allocate}

<div class="function-declaration" id="function-allocate-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">void</span> *allocate(
    <a href="../../types.html#symbol-usize">usize</a> bytes,
    <a href="../../types.html#symbol-usize">usize</a> alignment = DEFAULT_ALIGNMENT) <span class="k">noexcept</span>;</code></pre></div>

<p>Returns <code>bytes</code> bytes aligned to <code>alignment</code> (a power of two, up to and beyond 256). Bumps within the current block; a request that does not fit in a block of the configured size gets its own block. Returns <code>nullptr</code> for a zero-<code><a href="../../types.html#symbol-byte">byte</a></code> request, for an alignment that is not a power of two (asserts) and when the heap is exhausted.</p>

#### `block_bytes` {#function-block-bytes}

<div class="function-declaration" id="function-block-bytes-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-usize">usize</a> block_bytes() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Capacity of a regular block, as passed to the constructor.</p>

#### `block_count` {#function-block-count}

<div class="function-declaration" id="function-block-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-usize">usize</a> block_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Number of blocks currently held.</p>

#### `bytes_allocated` {#function-bytes-allocated}

<div class="function-declaration" id="function-bytes-allocated-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-usize">usize</a> bytes_allocated() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Sum of the bytes handed out since the last <code><a href="#function-reset-1">reset</a>()</code>, excluding alignment padding.</p>

#### `bytes_reserved` {#function-bytes-reserved}

<div class="function-declaration" id="function-bytes-reserved-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-usize">usize</a> bytes_reserved() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Sum of the capacities of the blocks currently held (the usable bytes, excluding the small per-block header).</p>

#### `construct` {#function-construct}

<div class="function-declaration" id="function-construct-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> T, <span class="k">typename</span>... Args&gt; [[<span class="na">nodiscard</span>]] T *construct(
    Args &amp;&amp;...args);</code></pre></div>

<p>Allocates and constructs a <code>T</code> in the arena. <code>T</code> must be trivially destructible because <code><a href="#function-reset-1">reset</a>()</code> and <code><a href="#function-release-1">release</a>()</code> never run destructors. Returns <code>nullptr</code> when allocation fails.</p>

#### `owns` {#function-owns}

<div class="function-declaration" id="function-owns-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> owns(
    <span class="k">const</span> <span class="k">void</span> *pointer) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when <code>pointer</code> lies inside the used part of one of the arena&#x27;s blocks. Linear in the block count; meant for assertions and tests.</p>

#### `release` {#function-release}

<div class="function-declaration" id="function-release-1"><pre><code><span class="k">void</span> release() <span class="k">noexcept</span>;</code></pre></div>

<p>Invalidates every allocation and returns every block to the heap. The arena stays usable; the next <code><a href="#function-allocate-1">allocate</a>()</code> obtains a fresh block.</p>

#### `reset` {#function-reset}

<div class="function-declaration" id="function-reset-1"><pre><code><span class="k">void</span> reset() <span class="k">noexcept</span>;</code></pre></div>

<p>Invalidates every allocation. The first block whose capacity is the regular block size is kept for reuse, if there is one; every other block is returned to the heap. <code>A</code> dedicated oversized block therefore never survives a reset, wherever it sits in the chain: an arena whose first request of every cycle is oversized re-allocates that block every cycle, which a larger block size avoids.</p>

### Private

#### `acquire_block` {#function-acquire-block}

<div class="function-declaration" id="function-acquire-block-1"><pre><code><a href="../arena.html">block</a> *acquire_block(
    <a href="../../types.html#symbol-usize">usize</a> capacity) <span class="k">noexcept</span>;</code></pre></div>

#### `free_block` {#function-free-block}

<div class="function-declaration" id="function-free-block-1"><pre><code><span class="k">void</span> free_block(
    <a href="../arena.html">block</a> *node) <span class="k">noexcept</span>;</code></pre></div>

## Operator Overloads

### Public

#### `operator=` {#operator-3d}

<div class="function-declaration" id="operator-3d-1"><pre><code><a href="memory-arena.html">arena</a> &amp;<span class="k">operator</span>=(
    <a href="memory-arena.html">arena</a> &amp;&amp;other) <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

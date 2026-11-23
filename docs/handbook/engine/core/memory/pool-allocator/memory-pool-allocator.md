---
kind: class
---

[Engine](../../../../engine.md) → [Core](../../../core.md) → [pool_allocator.h](../pool-allocator.md)

# memory::pool_allocator
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::core::memory` |
| Include File Path | `engine/include/conjure/core/memory/pool_allocator.h` |
| Source File Path | `engine/src/core/memory/pool_allocator.cpp` |
| API ↗ | [Memory](../../../../../api/core/memory.md) |

## Syntax

```cpp
class CONJURE_API pool_allocator
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">pool_allocator(
    <a href="../../types.html#symbol-usize">usize</a> blockSize,
    <a href="../../types.html#symbol-usize">usize</a> alignment,
    <a href="../../types.html#symbol-u32">u32</a> blocksPerSlab,
    <a href="../../types.html#symbol-u32">u32</a> maxSlabs = <span class="mi">0</span>);</code></td><td><code>blockSize</code> is rounded up to a multiple of <code>alignment</code> and to at least one pointer so a free block can hold its link. <code>alignment</code> must be a power of two (asserts and falls back to <code>DEFAULT_ALIGNMENT</code> otherwise) and is raised to at least <code>alignof(void *)</code> because a free block stores its link in place. <code>blocksPerSlab</code> must be at least 1 (asserts and uses 1 otherwise). <code>maxSlabs</code> caps growth; 0 means unlimited. Nothing is allocated until the first <code>allocate()</code>.</td></tr>
<tr><td>public</td><td><code class="signature">pool_allocator(
    <a href="memory-pool-allocator.html">pool_allocator</a> &amp;&amp;other) <span class="k">noexcept</span>;</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Additional constructor overloads have been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~pool_allocator();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mAlignment</code></td><td><code class="signature"><a href="../../types.html#symbol-usize">usize</a></code></td><td><code class="signature">DEFAULT_<wbr>ALIGNMENT</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mBlockBytes</code></td><td><code class="signature"><a href="../../types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Blocks<wbr>Per<wbr>Slab</code></td><td><code class="signature"><a href="../../types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">1</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mFreeList</code></td><td><code class="signature"><a href="memory-pool-allocator-free-block.html">free_block</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mLiveCount</code></td><td><code class="signature"><a href="../../types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mMaxSlabs</code></td><td><code class="signature"><a href="../../types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mSlabBytes</code></td><td><code class="signature"><a href="../../types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Slab<wbr>Capacity</code></td><td><code class="signature"><a href="../../types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mSlabCount</code></td><td><code class="signature"><a href="../../types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mSlabs</code></td><td><code class="signature"><a href="memory-pool-allocator-slab.html">slab</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `alignment` {#function-alignment}

<div class="function-declaration" id="function-alignment-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-usize">usize</a> alignment() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Alignment every block is guaranteed: the requested alignment raised to at least <code>alignof(void *)</code>.</p>

#### `allocate` {#function-allocate}

<div class="function-declaration" id="function-allocate-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">void</span> *allocate() <span class="k">noexcept</span>;</code></pre></div>

<p>Returns one block, growing by a slab when none is free. Returns <code>nullptr</code> when the slab cap is reached, a slab would be larger than <code>MAX_ALLOCATION_BYTES</code>, or a new slab (or the room to record it) cannot be obtained from the heap.</p>

#### `allocate_as` {#function-allocate-as}

<div class="function-declaration" id="function-allocate-as-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> T&gt; [[<span class="na">nodiscard</span>]] T *allocate_as() <span class="k">noexcept</span>;</code></pre></div>

<p><code><a href="#function-allocate-1">allocate</a>()</code> typed as a pointer to uninitialised storage for a <code>T</code>. Asserts (and returns <code>nullptr</code>) when a <code>T</code> does not fit a block or needs more alignment than the pool gives.</p>

#### `block_size` {#function-block-size}

<div class="function-declaration" id="function-block-size-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-usize">usize</a> block_size() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Effective block size: the requested size rounded up to the alignment and to a pointer.</p>

#### `blocks_per_slab` {#function-blocks-per-slab}

<div class="function-declaration" id="function-blocks-per-slab-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-u32">u32</a> blocks_per_slab() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Blocks each slab holds, as passed to the constructor (at least 1).</p>

#### `capacity` {#function-capacity}

<div class="function-declaration" id="function-capacity-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-usize">usize</a> capacity() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Blocks the held slabs can hold in total (live and free).</p>

#### `construct` {#function-construct}

<div class="function-declaration" id="function-construct-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> T, <span class="k">typename</span>... Args&gt; [[<span class="na">nodiscard</span>]] T *construct(
    Args &amp;&amp;...args);</code></pre></div>

<p>Allocates a block and constructs a <code>T</code> in it. Returns <code>nullptr</code> when allocation fails or a <code>T</code> does not fit a block. Pair with <code><a href="#function-destroy-1">destroy</a>()</code>.</p>

#### `deallocate` {#function-deallocate}

<div class="function-declaration" id="function-deallocate-1"><pre><code><span class="k">void</span> deallocate(
    <span class="k">void</span> *block) <span class="k">noexcept</span>;</code></pre></div>

<p>Returns a block to the pool. <code>nullptr</code> is ignored. <code>A</code> pointer the pool does not own, one that is not the start of a block, or a block that is already free asserts and is ignored.</p>

#### `destroy` {#function-destroy}

<div class="function-declaration" id="function-destroy-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> T&gt; <span class="k">void</span> destroy(
    T *object) <span class="k">noexcept</span>;</code></pre></div>

<p>Destroys an object made by <code><a href="#function-construct-1">construct</a>()</code> and returns its block. <code>nullptr</code> is ignored.</p>

#### `live_count` {#function-live-count}

<div class="function-declaration" id="function-live-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-usize">usize</a> live_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Blocks currently handed out.</p>

#### `max_slabs` {#function-max-slabs}

<div class="function-declaration" id="function-max-slabs-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-u32">u32</a> max_slabs() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Slab cap given to the constructor; 0 means unlimited.</p>

#### `owns` {#function-owns}

<div class="function-declaration" id="function-owns-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> owns(
    <span class="k">const</span> <span class="k">void</span> *pointer) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when <code>pointer</code> is the start of a block in one of the pool&#x27;s slabs (live or free).</p>

#### `shrink_to_fit` {#function-shrink-to-fit}

<div class="function-declaration" id="function-shrink-to-fit-1"><pre><code><span class="k">void</span> shrink_to_fit() <span class="k">noexcept</span>;</code></pre></div>

<p>Returns every slab that has no live block to the heap. Blocks in the remaining slabs are untouched; the slab table keeps its capacity.</p>

#### `slab_count` {#function-slab-count}

<div class="function-declaration" id="function-slab-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-usize">usize</a> slab_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Slabs currently held.</p>

### Private

#### `block_index` {#function-block-index}

<div class="function-declaration" id="function-block-index-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-u32">u32</a> block_index(
    <span class="k">const</span> <a href="memory-pool-allocator-slab.html">slab</a> &amp;owner,
    <span class="k">const</span> <span class="k">void</span> *pointer) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `find_slab_index` {#function-find-slab-index}

<div class="function-declaration" id="function-find-slab-index-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-usize">usize</a> find_slab_index(
    <span class="k">const</span> <span class="k">void</span> *pointer) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `fits` {#function-fits}

<div class="function-declaration" id="function-fits-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> fits(
    <a href="../../types.html#symbol-usize">usize</a> size,
    <a href="../../types.html#symbol-usize">usize</a> alignment) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `free_bits` {#function-free-bits}

<div class="function-declaration" id="function-free-bits-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-u64">u64</a> *free_bits(
    <span class="k">const</span> <a href="memory-pool-allocator-slab.html">slab</a> &amp;owner) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `grow` {#function-grow}

<div class="function-declaration" id="function-grow-1"><pre><code><span class="k">bool</span> grow() <span class="k">noexcept</span>;</code></pre></div>

#### `is_block_start` {#function-is-block-start}

<div class="function-declaration" id="function-is-block-start-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_block_start(
    <span class="k">const</span> <a href="memory-pool-allocator-slab.html">slab</a> &amp;owner,
    <span class="k">const</span> <span class="k">void</span> *pointer) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `is_free` {#function-is-free}

<div class="function-declaration" id="function-is-free-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_free(
    <span class="k">const</span> <a href="memory-pool-allocator-slab.html">slab</a> &amp;owner,
    <a href="../../types.html#symbol-u32">u32</a> index) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `mark_free` {#function-mark-free}

<div class="function-declaration" id="function-mark-free-1"><pre><code><span class="k">void</span> mark_free(
    <span class="k">const</span> <a href="memory-pool-allocator-slab.html">slab</a> &amp;owner,
    <a href="../../types.html#symbol-u32">u32</a> index,
    <span class="k">bool</span> isFree) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `release_slabs` {#function-release-slabs}

<div class="function-declaration" id="function-release-slabs-1"><pre><code><span class="k">void</span> release_slabs() <span class="k">noexcept</span>;</code></pre></div>

#### `reserve_slab_table` {#function-reserve-slab-table}

<div class="function-declaration" id="function-reserve-slab-table-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> reserve_slab_table() <span class="k">noexcept</span>;</code></pre></div>

## Operator Overloads

### Public

#### `operator=` {#operator-3d}

<div class="function-declaration" id="operator-3d-1"><pre><code><a href="memory-pool-allocator.html">pool_allocator</a> &amp;<span class="k">operator</span>=(
    <a href="memory-pool-allocator.html">pool_allocator</a> &amp;&amp;other) <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

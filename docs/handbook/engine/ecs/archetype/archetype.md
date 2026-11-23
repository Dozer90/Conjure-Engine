---
kind: class
---

[Engine](../../../engine.md) → [ECS](../../ecs.md) → [archetype.h](../archetype.md)

# archetype
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::ecs` |
| Include File Path | `engine/include/conjure/ecs/archetype.h` |
| Source File Path | `engine/src/ecs/archetype.cpp` |
| API ↗ | [Archives and serialisation](../../../../api/ecs/archives-and-serialization.md) |

## Syntax

```cpp
class CONJURE_API archetype
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">archetype(
    <span class="k">const</span> <a href="../component/component-mask.html">component_mask</a> &amp;mask,
    <a href="chunk-layout.html">chunk_layout</a> layout,
    <a href="../../core/types.html#symbol-u32">u32</a> index);</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted. Move constructors have been deleted. Move assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~archetype();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mChunks</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr>std::<wbr>unique_<wbr>ptr&lt;<wbr><a href="../archetype.html">chunk</a>&gt;&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mEdges</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="archetype-archetype-edge.html">archetype_<wbr>edge</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mEntityCount</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mIndex</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mLayout</code></td><td><code class="signature"><a href="chunk-layout.html">chunk_layout</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mMask</code></td><td><code class="signature"><a href="../component/component-mask.html">component_mask</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mSpareChunks</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr>std::<wbr>unique_<wbr>ptr&lt;<wbr><a href="../archetype.html">chunk</a>&gt;&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `chunk_column` {#function-chunk-column}

<div class="function-declaration" id="function-chunk-column-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <span class="k">void</span> *chunk_column(
    <a href="../../core/types.html#symbol-u32">u32</a> chunkIndex,
    <a href="../component/component-id.html">component_id</a> id) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Read-only start of a component column in one chunk; <code>nullptr</code> for a tag, an absent component or an out-of-range chunk.</p>

#### `chunk_column_version` {#function-chunk-column-version}

<div class="function-declaration" id="function-chunk-column-version-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u64">u64</a> chunk_column_version(
    <a href="../../core/types.html#symbol-u32">u32</a> chunkIndex,
    <a href="../component/component-id.html">component_id</a> id) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>World version last stamped on a column of one chunk; 0 when absent.</p>

#### `chunk_count` {#function-chunk-count}

<div class="function-declaration" id="function-chunk-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> chunk_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Chunks held, including a drained spare kept for reuse.</p>

#### `chunk_entities` {#function-chunk-entities}

<div class="function-declaration" id="function-chunk-entities-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> <a href="../entity/entity.html">entity</a>&gt; chunk_entities(
    <a href="../../core/types.html#symbol-u32">u32</a> chunkIndex) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The entity column of one chunk (<code>count()</code> entries); empty for an out-of-range index.</p>

#### `chunk_entity_count` {#function-chunk-entity-count}

<div class="function-declaration" id="function-chunk-entity-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> chunk_entity_count(
    <a href="../../core/types.html#symbol-u32">u32</a> chunkIndex) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Rows used in one chunk; 0 for an out-of-range index.</p>

#### `entity_count` {#function-entity-count}

<div class="function-declaration" id="function-entity-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> entity_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Entities stored here.</p>

#### `has` {#function-has}

<div class="function-declaration" id="function-has-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> has(
    <a href="../component/component-id.html">component_id</a> id) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when the set contains <code>id</code>.</p>

#### `index` {#function-index}

<div class="function-declaration" id="function-index-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> index() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Position in <code>world::archetypes()</code>.</p>

#### `layout` {#function-layout}

<div class="function-declaration" id="function-layout-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="chunk-layout.html">chunk_layout</a> &amp;layout() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The chunk layout shared by every chunk.</p>

#### `mask` {#function-mask}

<div class="function-declaration" id="function-mask-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../component/component-mask.html">component_mask</a> &amp;mask() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The component set of this <code><a href="archetype.html">archetype</a></code>.</p>

#### `spare_chunk_count` {#function-spare-chunk-count}

<div class="function-declaration" id="function-spare-chunk-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> spare_chunk_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Drained chunk objects parked without a block (their blocks are back in the pool), reused by the next growth so create/destroy churn allocates no chunk metadata.</p>

### Private

#### `chunk_at` {#function-chunk-at}

<div class="function-declaration" id="function-chunk-at-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../archetype.html">chunk</a> *chunk_at(
    <a href="../../core/types.html#symbol-u32">u32</a> chunkIndex) <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-chunk-at-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../archetype.html">chunk</a> *chunk_at(
    <a href="../../core/types.html#symbol-u32">u32</a> chunkIndex) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `edge` {#function-edge}

<div class="function-declaration" id="function-edge-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="archetype-archetype-edge.html">archetype_edge</a> &amp;edge(
    <a href="../component/component-id.html">component_id</a> id);</code></pre></div>

#### `find_edge` {#function-find-edge}

<div class="function-declaration" id="function-find-edge-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="archetype-archetype-edge.html">archetype_edge</a> *find_edge(
    <a href="../component/component-id.html">component_id</a> id) <span class="k">noexcept</span>;</code></pre></div>

#### `open_chunk` {#function-open-chunk}

<div class="function-declaration" id="function-open-chunk-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../archetype.html">chunk</a> *open_chunk(
    <a href="../../core/types.html#symbol-u32">u32</a> &amp;chunkIndex) <span class="k">noexcept</span>;</code></pre></div>

<p>The chunk the next row goes into (a partial chunk ahead of a drained spare fills before the spare so only the last used chunk is partial), or <code>nullptr</code> when every chunk is full.</p>

#### `push_row` {#function-push-row}

<div class="function-declaration" id="function-push-row-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="archetype-row-location.html">row_location</a>&gt; push_row(
    <a href="../archetype.html">chunk_pool</a> &amp;pool);</code></pre></div>

#### `reserve_row` {#function-reserve-row}

<div class="function-declaration" id="function-reserve-row-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; reserve_row(
    <a href="../archetype.html">chunk_pool</a> &amp;pool);</code></pre></div>

<p>Makes sure a chunk with a free row exists, allocating one when needed, without taking the row: the part of <code>push_row</code> that can fail (<code>ARCHETYPE_TOO_LARGE</code>, <code>OUT_OF_MEMORY</code>), split off so the world can run observers between the check and the move.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

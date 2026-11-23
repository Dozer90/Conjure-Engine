---
kind: class
---

[Engine](../../../engine.md) → [ECS](../../ecs.md) → [chunk_view.h](../chunk-view.md)

# chunk_view
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::ecs` |
| Include File Path | `engine/include/conjure/ecs/chunk_view.h` |
| Source File Path | `engine/src/ecs/chunk_view.cpp` |
| API ↗ | [Queries, views and systems](../../../../api/ecs/queries-views-and-systems.md) |

## Syntax

```cpp
class CONJURE_API chunk_view
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">chunk_view() <span class="k">noexcept</span> = <span class="k">default</span>;</code></td><td>An invalid view: <code>count()</code> is 0 and every accessor returns empty.</td></tr>
<tr><td>public</td><td><code class="signature">chunk_view(
    <a href="../archetype/archetype.html">archetype</a> &amp;owner,
    <a href="../chunk-view.html">chunk</a> &amp;target,
    <a href="../../core/types.html#symbol-u32">u32</a> chunkIndex,
    <span class="k">const</span> <a href="../component-registry/component-registry.html">component_registry</a> &amp;registry,
    <a href="../../core/types.html#symbol-u64">u64</a> stampVersion,
    <span class="k">const</span> <a href="../system/system-context.html">system_context</a> *context) <span class="k">noexcept</span>;</code></td><td><code>A</code> view of one chunk. Built by queries and the world; <code>context</code> is the running system when the view was created through one (Debug access validation).</td></tr>
<tr><td>public</td><td><code class="signature">chunk_view(
    <span class="k">const</span> <a href="chunk-view.html">chunk_view</a> &amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~chunk_view() = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mArchetype</code></td><td><code class="signature"><a href="../archetype/archetype.html">archetype</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mChunk</code></td><td><code class="signature"><a href="../chunk-view.html">chunk</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mChunkIndex</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mContext</code></td><td><code class="signature">[[<span class="na">maybe_<wbr>unused</span>]] <span class="k">const</span> <a href="../system/system-context.html">system_<wbr>context</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mRegistry</code></td><td><code class="signature"><span class="k">const</span> <a href="../component-registry/component-registry.html">component_<wbr>registry</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Stamp<wbr>Version</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `chunk_index` {#function-chunk-index}

<div class="function-declaration" id="function-chunk-index-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> chunk_index() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Index of this chunk in its archetype.</p>

#### `column_data` {#function-column-data}

<div class="function-declaration" id="function-column-data-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">void</span> *column_data(
    <a href="../component/component-id.html">component_id</a> id,
    <span class="k">bool</span> forWrite) <span class="k">noexcept</span>;</code></pre></div>

<p>Start of a component column, or <code>nullptr</code> for a tag or an absent component. <code>forWrite</code> stamps the column and, inside a system, checks the declared writes.</p>

<div class="function-declaration" id="function-column-data-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <span class="k">void</span> *column_data(
    <a href="../component/component-id.html">component_id</a> id) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Read-only start of a component column, or <code>nullptr</code>. Never stamps.</p>

#### `column_stride` {#function-column-stride}

<div class="function-declaration" id="function-column-stride-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> column_stride(
    <a href="../component/component-id.html">component_id</a> id) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Bytes between two rows of a column (the component size); 0 when absent.</p>

#### `column_version` {#function-column-version}

<div class="function-declaration" id="function-column-version-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u64">u64</a> column_version(
    <a href="../component/component-id.html">component_id</a> id) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Version last stamped on a column; 0 when absent.</p>

#### `count` {#function-count}

<div class="function-declaration" id="function-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Rows in the chunk.</p>

#### `entities` {#function-entities}

<div class="function-declaration" id="function-entities-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> <a href="../entity/entity.html">entity</a>&gt; entities() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The entity of every row.</p>

#### `get` {#function-get}

<div class="function-declaration" id="function-get-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> ComponentType&gt; <span class="k">requires</span> (!std::is_empty_v&lt;std::remove_cv_t&lt;ComponentType&gt;&gt;) [[<span class="na">nodiscard</span>]] std::span&lt;ComponentType&gt; get() <span class="k">noexcept</span>;</code></pre></div>

<p>The column of a required component: a non-const <code>T</code> stamps the column version once, a const <code>T</code> never does. <code>A</code> component the archetype lacks asserts and yields an empty span; use <code><a href="#function-has-1">has</a>()</code> or <code><a href="#function-get-optional-1">get_optional</a>()</code> for optional components. <code>A</code> tag has no column and does not compile; test it with <code><a href="#function-has-1">has</a>()</code>.</p>

#### `get_optional` {#function-get-optional}

<div class="function-declaration" id="function-get-optional-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> ComponentType&gt; <span class="k">requires</span> (!std::is_empty_v&lt;std::remove_cv_t&lt;ComponentType&gt;&gt;) [[<span class="na">nodiscard</span>]] std::span&lt;ComponentType&gt; get_optional() <span class="k">noexcept</span>;</code></pre></div>

<p>Like <code><a href="#function-get-1">get</a>()</code>, but an absent component yields an empty span without asserting. <code>A</code> tag does not compile here either.</p>

#### `has` {#function-has}

<div class="function-declaration" id="function-has-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> has(
    <a href="../component/component-id.html">component_id</a> id) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when the archetype holds <code>id</code> (tags included).</p>

#### `is_valid` {#function-is-valid}

<div class="function-declaration" id="function-is-valid-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_valid() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>False for a default-constructed view.</p>

#### `owner` {#function-owner}

<div class="function-declaration" id="function-owner-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../archetype/archetype.html">archetype</a> *owner() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The archetype this chunk belongs to (<code>nullptr</code> for an invalid view).</p>

#### `stamp_version` {#function-stamp-version}

<div class="function-declaration" id="function-stamp-version-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u64">u64</a> stamp_version() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The version non-const access stamps.</p>

## Operator Overloads

### Public

#### `operator=` {#operator-3d}

<div class="function-declaration" id="operator-3d-1"><pre><code><a href="chunk-view.html">chunk_view</a> &amp;<span class="k">operator</span>=(
    <span class="k">const</span> <a href="chunk-view.html">chunk_view</a> &amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

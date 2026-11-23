---
kind: class
---

[Engine](../../../engine.md) → [ECS](../../ecs.md) → [query.h](../query.md)

# query
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::ecs` |
| Include File Path | `engine/include/conjure/ecs/query.h` |
| Source File Path | `engine/src/ecs/query.cpp` |
| API ↗ | [Queries, views and systems](../../../../api/ecs/queries-views-and-systems.md) |

## Syntax

```cpp
class CONJURE_API query
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">query(
    <a href="../world/world.html">world</a> &amp;target,
    <a href="query-desc.html">query_desc</a> desc);</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">query(
    <span class="k">const</span> <a href="query.html">query</a> &amp;) = <span class="k">default</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">query(
    <a href="query.html">query</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~query();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mContext</code></td><td><code class="signature"><span class="k">const</span> <a href="../system/system-context.html">system_context</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mDesc</code></td><td><code class="signature"><a href="query-desc.html">query_desc</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mMatches</code></td><td><code class="signature"><a href="../../core/containers/small-vector/small-vector.html">core::<wbr>small_<wbr>vector</a>&lt;<wbr><a href="../archetype/archetype.html">archetype</a> *,<wbr> QUERY_<wbr>INLINE_<wbr>MATCHES&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mRegistry</code></td><td><code class="signature"><span class="k">const</span> <a href="../component-registry/component-registry.html">component_<wbr>registry</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Seen<wbr>Archetype<wbr>Count</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mWorld</code></td><td><code class="signature"><a href="../world/world.html">world</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `bind_context` {#function-bind-context}

<div class="function-declaration" id="function-bind-context-1"><pre><code><span class="k">void</span> bind_context(
    <span class="k">const</span> <a href="../system/system-context.html">system_context</a> *context) <span class="k">noexcept</span>;</code></pre></div>

<p>Attaches the running system: every <code><a href="../chunk-view/chunk-view.html">chunk_view</a></code> this <code><a href="query.html">query</a></code> builds carries it, so a non-const span of a component the system did not declare as written asserts (Debug validation), and non-const spans stamp the system&#x27;s run version instead of the current world version, so the system never sees its own writes through its next <code>since_version()</code>. Set by <code>system_context::view()</code>; <code>nullptr</code> for plain world queries. The context must outlive the <code><a href="query.html">query</a></code>&#x27;s iterations.</p>

#### `chunk` {#function-chunk}

<div class="function-declaration" id="function-chunk-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../chunk-view/chunk-view.html">chunk_view</a> chunk(
    <a href="../../core/types.html#symbol-u32">u32</a> index);</code></pre></div>

<p>The chunk at a flat index over the visited chunks (bindings). An out-of-range index yields an invalid view. The caller holds <code>world::lock_iteration()</code> while using the result. The view stamps like <code>for_each_chunk</code>: the bound system&#x27;s run version, else the current world version.</p>

#### `chunk_count` {#function-chunk-count}

<div class="function-declaration" id="function-chunk-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> chunk_count();</code></pre></div>

<p>Number of chunks the next iteration would visit. Refreshes lazily first (archetypes created since the last refresh join the cached matches), so it is non-const by design.</p>

#### `chunk_passes` {#function-chunk-passes}

<div class="function-declaration" id="function-chunk-passes-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> chunk_passes(
    <span class="k">const</span> <a href="../archetype/archetype.html">archetype</a> &amp;owner,
    <a href="../../core/types.html#symbol-u32">u32</a> chunkIndex) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when a chunk of a matching archetype passes the changed filter and is not empty.</p>

#### `context` {#function-context}

<div class="function-declaration" id="function-context-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../system/system-context.html">system_context</a> *context() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The system bound with <code><a href="#function-bind-context-1">bind_context</a>()</code>, or <code>nullptr</code>.</p>

#### `desc` {#function-desc}

<div class="function-declaration" id="function-desc-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="query-desc.html">query_desc</a> &amp;desc() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The description this <code><a href="query.html">query</a></code> was built from.</p>

#### `entity_count` {#function-entity-count}

<div class="function-declaration" id="function-entity-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> entity_count();</code></pre></div>

<p>Number of entities in the chunks the next iteration would visit. Refreshes lazily first, so it is non-const by design.</p>

#### `for_each_chunk` {#function-for-each-chunk}

<div class="function-declaration" id="function-for-each-chunk-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> Callable&gt; <span class="k">void</span> for_each_chunk(
    Callable &amp;&amp;callable);</code></pre></div>

<p>Invokes <code>callable(<a href="../chunk-view/chunk-view.html">chunk_view</a> &amp;)</code> for every non-empty chunk of every matching archetype that passes the changed filter, under an iteration lock. Non-const spans acquired from the view stamp the run version of the system bound with <code><a href="#function-bind-context-1">bind_context</a>()</code>, otherwise the world version current when the iteration started.</p>

#### `matches` {#function-matches}

<div class="function-declaration" id="function-matches-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<a href="../archetype/archetype.html">archetype</a> *<span class="k">const</span>&gt; matches();</code></pre></div>

<p>The matching archetypes. Refreshes lazily first, so it is non-const by design.</p>

#### `owner` {#function-owner}

<div class="function-declaration" id="function-owner-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../world/world.html">world</a> &amp;owner() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The world this <code><a href="query.html">query</a></code> iterates.</p>

#### `refresh` {#function-refresh}

<div class="function-declaration" id="function-refresh-1"><pre><code><span class="k">void</span> refresh();</code></pre></div>

<p>Scans the archetypes created since the last refresh and caches the matching ones.</p>

#### `registry` {#function-registry}

<div class="function-declaration" id="function-registry-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../component-registry/component-registry.html">component_registry</a> &amp;registry() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The registry of the world.</p>

#### `reset` {#function-reset}

<div class="function-declaration" id="function-reset-1"><pre><code><span class="k">void</span> reset(
    <a href="query-desc.html">query_desc</a> desc);</code></pre></div>

<p>Replaces the description and forgets the cached matches.</p>

### Private

#### `visit_chunks` {#function-visit-chunks}

<div class="function-declaration" id="function-visit-chunks-1"><pre><code><span class="k">void</span> visit_chunks(
    <span class="k">void</span> (*visit)(<span class="k">void</span> *context, <a href="../chunk-view/chunk-view.html">chunk_view</a> &amp;view),
    <span class="k">void</span> *context);</code></pre></div>

## Operator Overloads

### Public

#### `operator=` {#operator-3d}

<div class="function-declaration" id="operator-3d-1"><pre><code><a href="query.html">query</a> &amp;<span class="k">operator</span>=(
    <span class="k">const</span> <a href="query.html">query</a> &amp;) = <span class="k">default</span>;</code></pre></div>

<div class="function-declaration" id="operator-3d-2"><pre><code><a href="query.html">query</a> &amp;<span class="k">operator</span>=(
    <a href="query.html">query</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

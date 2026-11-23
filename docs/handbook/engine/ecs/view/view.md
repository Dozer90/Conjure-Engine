---
kind: class
---

[Engine](../../../engine.md) → [ECS](../../ecs.md) → [view.h](../view.md)

# view
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::ecs` |
| Include File Path | `engine/include/conjure/ecs/view.h` |
| Source File Path | N/A |
| API ↗ | [Queries, views and systems](../../../../api/ecs/queries-views-and-systems.md) |

## Syntax

```cpp
template <typename... ComponentTypes> class view
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">explicit</span> view(
    <a href="../world/world.html">world</a> &amp;target);</code></td><td><code>A</code> <code><a href="view.html">view</a></code> over every entity of <code>target</code> holding the required components of the pack.</td></tr>
<tr><td>public</td><td><code class="signature">view(
    <span class="k">const</span> <a href="view.html">view</a> &amp;) = <span class="k">default</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">view(
    <a href="view.html">view</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~view() = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">MATCH_<wbr>NOTHING</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../component/component-mask.html">component_<wbr>mask</a></code></td><td><code class="signature">~<a href="../component/component-mask.html">component_<wbr>mask</a>{}</code></td><td><code>A</code> <code>with</code> mask no archetype satisfies: the match set of a <code><a href="view.html">view</a></code> over an unregistered type.</td></tr>
<tr><td>private</td><td><code class="signature">mIds</code></td><td><code class="signature">std::<wbr>array&lt;<wbr>component_<wbr>id,<wbr> PACK_<wbr>SIZE&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mQuery</code></td><td><code class="signature"><a href="../query/query.html">query</a></code></td><td>—</td><td></td></tr>
<tr><td>private</td><td><code class="signature">mValid</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">true</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">PACK_SIZE</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../core/types.html#symbol-usize">usize</a></code></td><td><code class="signature">sizeof...(Component<wbr>Types)</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `access` {#function-access}

<div class="function-declaration" id="function-access-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="view-access.html">view_access</a> access() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The read and write sets derived from the pack.</p>

#### `changed` {#function-changed}

<div class="function-declaration" id="function-changed-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> OtherType&gt; [[<span class="na">nodiscard</span>]] <a href="view.html">view</a> &amp;changed(
    <a href="../../core/types.html#symbol-u64">u64</a> sinceVersion);</code></pre></div>

<p>Visits only chunks whose column of OtherType was stamped after <code>sinceVersion</code>.</p>

#### `entity_count` {#function-entity-count}

<div class="function-declaration" id="function-entity-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> entity_count();</code></pre></div>

<p>Number of matching entities.</p>

#### `for_each` {#function-for-each}

<div class="function-declaration" id="function-for-each-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> Callable&gt; <span class="k">void</span> for_each(
    Callable &amp;&amp;callable);</code></pre></div>

<p>Invokes <code>callable(entity, ComponentTypes&amp;...)</code> for every matching entity (<code><a href="maybe.html">maybe</a>&lt;T&gt;</code> as <code>T</code> *).</p>

#### `for_each_chunk` {#function-for-each-chunk}

<div class="function-declaration" id="function-for-each-chunk-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> Callable&gt; <span class="k">void</span> for_each_chunk(
    Callable &amp;&amp;callable);</code></pre></div>

<p>Invokes <code>callable(<a href="../chunk-view/chunk-view.html">chunk_view</a> &amp;)</code> for every matching chunk.</p>

#### `is_valid` {#function-is-valid}

<div class="function-declaration" id="function-is-valid-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_valid() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>False once the pack or a builder call required a type the registry does not know; such a <code><a href="view.html">view</a></code> matches nothing.</p>

#### `parallel_for_each_chunk` {#function-parallel-for-each-chunk}

<div class="function-declaration" id="function-parallel-for-each-chunk-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> Callable&gt; <span class="k">void</span> parallel_for_each_chunk(
    <a href="../system/system-context.html">system_context</a> &amp;context,
    Callable &amp;&amp;callable);</code></pre></div>

<p>Runs <code>callable(<a href="../chunk-view/chunk-view.html">chunk_view</a> &amp;, <a href="../command-buffer/command-buffer.html">command_buffer</a> &amp;)</code> over the matching chunks through the system&#x27;s job system, one command buffer per grain (<code>system_context::parallel_for_chunks</code>). Defined in <code>system.h</code>, which callers using it must include.</p>

#### `underlying` {#function-underlying}

<div class="function-declaration" id="function-underlying-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../query/query.html">query</a> &amp;underlying() <span class="k">noexcept</span>;</code></pre></div>

<p>The query behind this <code><a href="view.html">view</a></code>.</p>

#### `with` {#function-with}

<div class="function-declaration" id="function-with-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> TagType&gt; [[<span class="na">nodiscard</span>]] <a href="view.html">view</a> &amp;with();</code></pre></div>

<p>Requires a component (typically a tag) without reading it.</p>

#### `without` {#function-without}

<div class="function-declaration" id="function-without-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> OtherType&gt; [[<span class="na">nodiscard</span>]] <a href="view.html">view</a> &amp;without();</code></pre></div>

<p>Excludes entities holding a component.</p>

### Private

#### `collect_access` {#function-collect-access}

<div class="function-declaration" id="function-collect-access-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> PackType&gt; <span class="k">void</span> collect_access(
    <a href="view-access.html">view_access</a> &amp;result,
    <a href="../../core/types.html#symbol-usize">usize</a> &amp;slot) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `fill_slot` {#function-fill-slot}

<div class="function-declaration" id="function-fill-slot-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> PackType&gt; <span class="k">void</span> fill_slot(
    <span class="k">const</span> <a href="../component-registry/component-registry.html">component_registry</a> &amp;registry,
    <a href="../query/query-desc.html">query_desc</a> &amp;desc,
    <a href="../../core/types.html#symbol-usize">usize</a> &amp;slot);</code></pre></div>

#### `invoke_row` {#function-invoke-row}

<div class="function-declaration" id="function-invoke-row-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> Callable, <span class="k">typename</span> Columns, <a href="../../core/types.html#symbol-usize">usize</a>... Indices&gt; <span class="k">static</span> <span class="k">void</span> invoke_row(
    Callable &amp;callable,
    <a href="../entity/entity.html">entity</a> target,
    Columns &amp;columns,
    <a href="../../core/types.html#symbol-u32">u32</a> row,
    std::index_sequence&lt;Indices...&gt;);</code></pre></div>

#### `require_unregistered` {#function-require-unregistered}

<div class="function-declaration" id="function-require-unregistered-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> OtherType&gt; <span class="k">void</span> require_unregistered();</code></pre></div>

## Operator Overloads

### Public

#### `operator=` {#operator-3d}

<div class="function-declaration" id="operator-3d-1"><pre><code><a href="view.html">view</a> &amp;<span class="k">operator</span>=(
    <span class="k">const</span> <a href="view.html">view</a> &amp;) = <span class="k">default</span>;</code></pre></div>

<div class="function-declaration" id="operator-3d-2"><pre><code><a href="view.html">view</a> &amp;<span class="k">operator</span>=(
    <a href="view.html">view</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

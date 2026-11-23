---
kind: header
---

[Engine](../../engine.md) → [ECS](../ecs.md)

# query.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/ecs/query.h` |
| API ↗ | [Queries, views and systems](../../../api/ecs/queries-views-and-systems.md) |

<p>Queries. <code>A</code> <code><a href="query/query.html">query</a></code> is a value type that caches the archetypes matching its description and refreshes incrementally (new archetypes only). Iteration locks the world for its duration, so structural calls made from a callback fail with <code>STRUCTURE_LOCKED</code> and the callback must use a <code><a href="command-buffer/command-buffer.html">command_buffer</a></code> instead.</p>

## Structs

| File | Description |
|---|---|
| [`query_desc`](query/query-desc.md) | What a <code><a href="query/query.html">query</a></code> matches: archetypes containing every bit of <code>with</code> and none of <code>without</code>; with a non-empty <code>changed</code> mask a chunk is visited only when one of the listed columns changed after <code>sinceVersion</code>. <code>optional</code> and <code>writes</code> record what a view&#x27;s pack reads when present and what it writes; the <code><a href="query/query.html">query</a></code> itself does not consult them. Stamping follows the constness of the span a <code><a href="chunk-view/chunk-view.html">chunk_view</a></code> hands out, and Debug write validation uses the bound system&#x27;s <code><a href="system/system-desc.html">system_desc</a></code>. |

## Classes

| File | Description |
|---|---|
| [`query`](query/query.md) |  |

## Forward Declarations

### `component_registry` {#symbol-component-registry}

```cpp
class component_registry;
```

### `system_context` {#symbol-system-context}

```cpp
class system_context;
```

### `world` {#symbol-world}

```cpp
class world;
```

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-QUERY_INLINE_MATCHES"></a><code>QUERY_INLINE_MATCHES</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-usize">usize</a></code> | <code class="signature"><span class="mi">16</span></code> | Matching archetypes a <code><a href="query/query.html">query</a></code> stores inline before it touches the heap. <code>A</code> view built per system run (the documented <code>context.view&lt;...&gt;()</code>.<code>for_each</code> idiom, <code>system.h</code>) therefore allocates nothing while it matches at most this many archetypes; beyond that the first refresh of each new <code><a href="query/query.html">query</a></code> allocates once. |

## Functions

### Public

#### `detail::registry_of` {#function-detail-registry-of}

<div class="function-declaration" id="function-detail-registry-of-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <span class="k">const</span> <a href="component-registry/component-registry.html">component_registry</a> &amp;registry_of(<span class="k">const</span> <a href="world/world.html">world</a> &amp;target) <span class="k">noexcept</span>;</code></pre></div>

<p>The registry a world was created on; lets headers that cannot include <code>world.h</code> build masks.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="component_registry"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="detailregistry_of"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="query"></a>
<a id="query_desc"></a>
<a id="query_descchanged"></a>
<a id="query_descoptional"></a>
<a id="query_descsinceversion"></a>
<a id="query_descwith"></a>
<a id="query_descwithout"></a>
<a id="query_descwrites"></a>
<a id="query_inline_matches"></a>
<a id="querybind_context"></a>
<a id="querychunk"></a>
<a id="querychunk_count"></a>
<a id="querychunk_passes"></a>
<a id="querycontext"></a>
<a id="querydesc"></a>
<a id="queryentity_count"></a>
<a id="queryfor_each_chunk"></a>
<a id="querymatches"></a>
<a id="querymcontext"></a>
<a id="querymdesc"></a>
<a id="querymmatches"></a>
<a id="querymregistry"></a>
<a id="querymseenarchetypecount"></a>
<a id="querymworld"></a>
<a id="queryoperator"></a>
<a id="queryowner"></a>
<a id="queryquery"></a>
<a id="queryquery_1"></a>
<a id="queryrefresh"></a>
<a id="queryregistry"></a>
<a id="queryreset"></a>
<a id="queryvisit_chunks"></a>
<a id="registry_of"></a>
<a id="structs-and-types"></a>
<a id="system_context"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<a id="world"></a>
<!-- doc-legacy-anchors:end -->

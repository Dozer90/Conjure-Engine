---
kind: header
---

[Engine](../../engine.md) → [ECS](../ecs.md)

# chunk_view.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/ecs/chunk_view.h` |
| API ↗ | [Queries, views and systems](../../../api/ecs/queries-views-and-systems.md) |

<p>Untyped per-chunk access. <code>A</code> <code><a href="chunk-view/chunk-view.html">chunk_view</a></code> is what queries hand to callbacks and what the <code>C</code># <code>Span&lt;T&gt;</code> is built on. Acquiring a non-const span stamps the column with the world version the view was created with (chunk-granular change detection, 3.3); const spans never stamp. Inside a system (a view created through <code><a href="system/system-context.html">system_context</a></code>) a non-const span of a component the system did not declare as written asserts in Debug and Development.</p>



## Classes

| File | Description |
|---|---|
| [`chunk_view`](chunk-view/chunk-view.md) |  |

## Forward Declarations

### `chunk` {#symbol-chunk}

```cpp
class chunk;
```

### `system_context` {#symbol-system-context}

```cpp
class system_context;
```

## Functions

### Public

#### `detail::context_declares_write` {#function-detail-context-declares-write}

<div class="function-declaration" id="function-detail-context-declares-write-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <span class="k">bool</span> context_declares_write(<span class="k">const</span> <a href="system/system-context.html">system_context</a> &amp;context, <a href="component/component-id.html">component_id</a> id) <span class="k">noexcept</span>;</code></pre></div>

<p>True when the system behind <code>context</code> declared <code>id</code> as written.</p>

#### `detail::report_undeclared_write` {#function-detail-report-undeclared-write}

<div class="function-declaration" id="function-detail-report-undeclared-write-1"><pre><code><span class="na">CONJURE_API</span> <span class="k">void</span> report_undeclared_write(<span class="k">const</span> <a href="system/system-context.html">system_context</a> &amp;context, <a href="component/component-id.html">component_id</a> id);</code></pre></div>

<p>Asserts (naming the system and component) about an undeclared write.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="chunk"></a>
<a id="chunk_view"></a>
<a id="chunk_viewchunk_index"></a>
<a id="chunk_viewchunk_view"></a>
<a id="chunk_viewchunk_view_1"></a>
<a id="chunk_viewcolumn_data"></a>
<a id="chunk_viewcolumn_stride"></a>
<a id="chunk_viewcolumn_version"></a>
<a id="chunk_viewcount"></a>
<a id="chunk_viewentities"></a>
<a id="chunk_viewget"></a>
<a id="chunk_viewget_optional"></a>
<a id="chunk_viewhas"></a>
<a id="chunk_viewis_valid"></a>
<a id="chunk_viewmarchetype"></a>
<a id="chunk_viewmchunk"></a>
<a id="chunk_viewmchunkindex"></a>
<a id="chunk_viewmcontext"></a>
<a id="chunk_viewmregistry"></a>
<a id="chunk_viewmstampversion"></a>
<a id="chunk_viewoperator"></a>
<a id="chunk_viewowner"></a>
<a id="chunk_viewstamp_version"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="context_declares_write"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="detailcontext_declares_write"></a>
<a id="detailreport_undeclared_write"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="report_undeclared_write"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="system_context"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->

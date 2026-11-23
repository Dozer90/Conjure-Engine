---
kind: header
---

[Engine](../../engine.md) → [ECS](../ecs.md)

# archetype.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/ecs/archetype.h` |
| API ↗ | [Archives and serialisation](../../../api/ecs/archives-and-serialization.md) |

<p>One component set and the chunks holding its entities. Read-only outside the world; pointers to archetypes stay valid for the world&#x27;s lifetime (the world&#x27;s list is append-only).</p>

## Structs

| File | Description |
|---|---|
| [`archetype::archetype_edge`](archetype/archetype-archetype-edge.md) | Graph edge to the archetypes that differ by one component. |
| [`archetype::row_location`](archetype/archetype-row-location.md) | Position of one row. |
| [`chunk_layout`](archetype/chunk-layout.md) | Where each column of an <code><a href="archetype/archetype.html">archetype</a></code> lives inside a chunk block. The entity column sits at offset 0; columns hold the non-tag components only. |

## Classes

| File | Description |
|---|---|
| [`archetype`](archetype/archetype.md) | One component set and the chunks holding its entities. Read-only outside the world; pointers to archetypes stay valid for the world&#x27;s lifetime (the world&#x27;s list is append-only). |

## Forward Declarations

### `chunk` {#symbol-chunk}

```cpp
class chunk;
```

### `chunk_pool` {#symbol-chunk-pool}

```cpp
class chunk_pool;
```

### `chunk_view` {#symbol-chunk-view}

```cpp
class chunk_view;
```

### `component_registry` {#symbol-component-registry}

```cpp
class component_registry;
```

### `query` {#symbol-query}

```cpp
class query;
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
| <a id="variable-CHUNK_ALIGNMENT"></a><code>CHUNK_ALIGNMENT</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">64</span></code> | Alignment of every chunk block. |
| <a id="variable-CHUNK_SIZE"></a><code>CHUNK_SIZE</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">16</span> * <span class="mi">1024</span></code> | Bytes of one chunk block. |
| <a id="variable-COLUMN_ALIGNMENT"></a><code>COLUMN_ALIGNMENT</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">16</span></code> | Minimum alignment of a column start inside a chunk (SIMD-friendly loads). |
| <a id="variable-SMALL_CAPACITY_WARNING"></a><code>SMALL_CAPACITY_WARNING</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">8</span></code> | Fewest rows per chunk before <code>compute()</code> logs a warning about a wasteful <code><a href="archetype/archetype.html">archetype</a></code>. |

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="archetype"></a>
<a id="archetypearchetype"></a>
<a id="archetypearchetype_1"></a>
<a id="archetypearchetype_edge"></a>
<a id="archetypearchetype_edgeid"></a>
<a id="archetypearchetype_edgewithadded"></a>
<a id="archetypearchetype_edgewithremoved"></a>
<a id="archetypechunk_at"></a>
<a id="archetypechunk_column"></a>
<a id="archetypechunk_column_version"></a>
<a id="archetypechunk_count"></a>
<a id="archetypechunk_entities"></a>
<a id="archetypechunk_entity_count"></a>
<a id="archetypechunk_view"></a>
<a id="archetypeedge"></a>
<a id="archetypeentity_count"></a>
<a id="archetypefind_edge"></a>
<a id="archetypehas"></a>
<a id="archetypeindex"></a>
<a id="archetypelayout"></a>
<a id="archetypemask"></a>
<a id="archetypemchunks"></a>
<a id="archetypemedges"></a>
<a id="archetypementitycount"></a>
<a id="archetypemindex"></a>
<a id="archetypemlayout"></a>
<a id="archetypemmask"></a>
<a id="archetypemsparechunks"></a>
<a id="archetypeopen_chunk"></a>
<a id="archetypeoperator"></a>
<a id="archetypepush_row"></a>
<a id="archetypequery"></a>
<a id="archetypereserve_row"></a>
<a id="archetyperow_location"></a>
<a id="archetyperow_locationchunkindex"></a>
<a id="archetyperow_locationrow"></a>
<a id="archetypespare_chunk_count"></a>
<a id="archetypesystem_context"></a>
<a id="archetypeworld"></a>
<a id="chunk"></a>
<a id="chunk_alignment"></a>
<a id="chunk_layout"></a>
<a id="chunk_layoutblock_end"></a>
<a id="chunk_layoutcapacity"></a>
<a id="chunk_layoutcolumn_count"></a>
<a id="chunk_layoutcolumn_of"></a>
<a id="chunk_layoutcolumnindex"></a>
<a id="chunk_layoutcolumnoffsets"></a>
<a id="chunk_layoutcolumnsizes"></a>
<a id="chunk_layoutcomponents"></a>
<a id="chunk_layoutcompute"></a>
<a id="chunk_layoutmake_empty_column_index"></a>
<a id="chunk_layoutno_column"></a>
<a id="chunk_layoutrow_bytes"></a>
<a id="chunk_pool"></a>
<a id="chunk_size"></a>
<a id="chunk_view"></a>
<a id="column_alignment"></a>
<a id="component_registry"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="query"></a>
<a id="small_capacity_warning"></a>
<a id="structs-and-types"></a>
<a id="system_context"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<a id="world"></a>
<!-- doc-legacy-anchors:end -->

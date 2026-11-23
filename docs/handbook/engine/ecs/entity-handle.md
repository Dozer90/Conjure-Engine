---
kind: header
---

[Engine](../../engine.md) → [ECS](../ecs.md)

# entity_handle.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/ecs/entity_handle.h` |
| API ↗ | [Entities and components](../../../api/ecs/entities-and-components.md) |

<p>An entity plus its world. Every call re-validates the entity.</p>



## Classes

| File | Description |
|---|---|
| [`component_ref`](entity-handle/component-ref.md) | <code>A</code> reference to one component of one entity that resolves the storage on every access. |
| [`entity_handle`](entity-handle/entity-handle.md) | An entity plus its world. Every call re-validates the entity. |

## Functions

### Public

#### `handle` {#function-handle}

<div class="function-declaration" id="function-handle-1"><pre><code><span class="k">inline</span> <a href="entity-handle/entity-handle.html">entity_handle</a> world::handle(
    <a href="entity/entity.html">entity</a> target) <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="component_ref"></a>
<a id="component_refcomponent_ref"></a>
<a id="component_refcomponent_ref_1"></a>
<a id="component_refmentity"></a>
<a id="component_refmworld"></a>
<a id="component_refoperator"></a>
<a id="component_refowner"></a>
<a id="component_refread"></a>
<a id="component_refset"></a>
<a id="component_refvalid"></a>
<a id="component_refwrite"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="entity_handle"></a>
<a id="entity_handleadd"></a>
<a id="entity_handlecomponent"></a>
<a id="entity_handledestroy"></a>
<a id="entity_handleentity_handle"></a>
<a id="entity_handleentity_handle_1"></a>
<a id="entity_handlehas"></a>
<a id="entity_handleid"></a>
<a id="entity_handleis_alive"></a>
<a id="entity_handlementity"></a>
<a id="entity_handlemworld"></a>
<a id="entity_handleoperator"></a>
<a id="entity_handleoperator_1"></a>
<a id="entity_handleowner"></a>
<a id="entity_handleremove"></a>
<a id="extension-points"></a>
<a id="handle"></a>
<a id="internals-and-algorithms"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->

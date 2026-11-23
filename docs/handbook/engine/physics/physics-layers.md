---
kind: header
---

[Engine](../../engine.md) → [Physics](../physics.md)

# physics_layers.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/physics/physics_layers.h` |
| API ↗ | [Bodies, colliders and characters](../../../api/physics/bodies-and-colliders.md), [Physics](../../../api/physics/index.md) |

<p>Physics layers: up to 32 named layers with a symmetric collision matrix, authored in <code>project.json</code> under <code>extensions.physics_layers</code>. Layer indices are positions in the &quot;names&quot; array; a <code><a href="physics-layers/physics-layer-mask.html">physics_layer_mask</a></code> is a <code><a href="../core/types.html#symbol-u32">u32</a></code> bit set over those indices (bit i = layer i).</p>

## Structs

| File | Description |
|---|---|
| [`physics_layer_mask`](physics-layers/physics-layer-mask.md) | <code>A</code> set of layers as one bit per layer index. The default mask includes every layer. |

## Classes

| File | Description |
|---|---|
| [`physics_layer_table`](physics-layers/physics-layer-table.md) | The named layers and their symmetric collision matrix. <code>A</code> value type; immutable once a <code><a href="physics-world/physics-world.html">physics_world</a></code> was constructed from it (the world keeps its own copy). |

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-MAX_PHYSICS_LAYERS"></a><code>MAX_PHYSICS_LAYERS</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">32</span></code> | Most layers a table may hold; the mask is a <code><a href="../core/types.html#symbol-u32">u32</a></code>. |

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="max_physics_layers"></a>
<a id="physics_layer_mask"></a>
<a id="physics_layer_maskall"></a>
<a id="physics_layer_maskbits"></a>
<a id="physics_layer_maskincludes"></a>
<a id="physics_layer_masknone"></a>
<a id="physics_layer_maskonly"></a>
<a id="physics_layer_maskoperator"></a>
<a id="physics_layer_maskwith"></a>
<a id="physics_layer_maskwithout"></a>
<a id="physics_layer_table"></a>
<a id="physics_layer_tableadd_layer"></a>
<a id="physics_layer_tablecollides"></a>
<a id="physics_layer_tablecollision_mask"></a>
<a id="physics_layer_tablecount"></a>
<a id="physics_layer_tablefind"></a>
<a id="physics_layer_tablefrom_json"></a>
<a id="physics_layer_tablemcollides"></a>
<a id="physics_layer_tablemnames"></a>
<a id="physics_layer_tablename"></a>
<a id="physics_layer_tableoperator"></a>
<a id="physics_layer_tableoperator_1"></a>
<a id="physics_layer_tablephysics_layer_table"></a>
<a id="physics_layer_tablephysics_layer_table_1"></a>
<a id="physics_layer_tableset_collides"></a>
<a id="physics_layer_tableto_json"></a>
<a id="physics_layer_tablevalidate"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->

---
kind: header
---

[Engine](../../engine.md) → [Physics](../physics.md)

# collision_mesh_asset.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/physics/collision_mesh_asset.h` |
| API ↗ | [Bodies, colliders and characters](../../../api/physics/bodies-and-colliders.md), [Physics](../../../api/physics/index.md) |

<p>Triangle geometry for mesh and convex colliders. Values are validated on construction: every index is in range, the index count is a multiple of three and degenerate triangles are dropped.</p>



## Classes

| File | Description |
|---|---|
| [`collision_mesh_asset`](collision-mesh-asset/collision-mesh-asset.md) | Triangle geometry for mesh and convex colliders. Values are validated on construction: every index is in range, the index count is a multiple of three and degenerate triangles are dropped. |

## Forward Declarations

### `detail::collision_mesh_shapes` {#symbol-detail-collision-mesh-shapes}

```cpp
struct collision_mesh_shapes;
```

The Jolt shapes built for a loaded mesh. Opaque here: Jolt never reaches a public header.

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-COLLISION_MESH_ASSET_TYPE"></a><code>COLLISION_MESH_ASSET_TYPE</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code> | <code class="signature"><span class="s">"conjure.physics.collision_<wbr>mesh"</span></code> | Registered asset type name. |
| <a id="variable-COLLISION_MESH_IMPORTER_ID"></a><code>COLLISION_MESH_IMPORTER_ID</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code> | <code class="signature"><span class="s">"conjure.physics.collision_<wbr>mesh_<wbr>importer"</span></code> | Importer id recorded in .meta files. |
| <a id="variable-COLLISION_MESH_SECTION_ALIGNMENT"></a><code>COLLISION_MESH_SECTION_ALIGNMENT</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-usize">usize</a></code> | <code class="signature"><span class="mi">16</span></code> | Byte alignment of every section of the cooked payload. |

## Macros

### `CONJURE_DECLARE_ASSET_TYPE` {#symbol-conjure-declare-asset-type}

```cpp
CONJURE_DECLARE_ASSET_TYPE(conjure::physics::collision_mesh_asset, "conjure.physics.collision_mesh")
```

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="collision_mesh_asset"></a>
<a id="collision_mesh_asset_type"></a>
<a id="collision_mesh_assetbounds"></a>
<a id="collision_mesh_assetcollision_mesh_asset"></a>
<a id="collision_mesh_assetcollision_mesh_asset_1"></a>
<a id="collision_mesh_assetfrom_geometry"></a>
<a id="collision_mesh_assetfrom_payload"></a>
<a id="collision_mesh_assethas_shapes"></a>
<a id="collision_mesh_assetindices"></a>
<a id="collision_mesh_assetmbounds"></a>
<a id="collision_mesh_assetmindices"></a>
<a id="collision_mesh_assetmpositions"></a>
<a id="collision_mesh_assetmshapes"></a>
<a id="collision_mesh_assetoperator"></a>
<a id="collision_mesh_assetpositions"></a>
<a id="collision_mesh_assetset_shapes"></a>
<a id="collision_mesh_assetshapes"></a>
<a id="collision_mesh_assetto_payload"></a>
<a id="collision_mesh_assettriangle_count"></a>
<a id="collision_mesh_assetvertex_count"></a>
<a id="collision_mesh_importer_id"></a>
<a id="collision_mesh_section_alignment"></a>
<a id="collision_mesh_shapes"></a>
<a id="conjure_declare_asset_type"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="detailcollision_mesh_shapes"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="other-declarations"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->

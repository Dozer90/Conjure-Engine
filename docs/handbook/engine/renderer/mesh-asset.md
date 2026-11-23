---
kind: header
---

[Engine](../../engine.md) → [Renderer](../renderer.md)

# mesh_asset.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/renderer/mesh_asset.h` |
| API ↗ | [Rendering](../../../api/renderer/index.md), [Renderer assets](../../../api/renderer/renderer-assets.md) |

<p>The CPU mesh asset. Geometry is stored in the two GPU streams: stream 0 holds positions (<code>RGB32_FLOAT</code>), stream 1 the packed attributes (32 bytes: normal and tangent as <code>RGBA16_SNORM</code> with the tangent sign in the normal&#x27;s w, uv0 as <code>RG32_FLOAT</code>, uv1 as <code>RG16_FLOAT</code>, colour as <code>RGBA8_UNORM</code>). Indices are <code>R32_UINT</code>. Shadow and depth passes bind stream 0 only. Positions and indices stay resident for the editor&#x27;s CPU picking (<code>cpu_geometry</code>).</p>

## Structs

| File | Description |
|---|---|
| [`cpu_geometry_view`](mesh-asset/cpu-geometry-view.md) | Positions and indices for CPU picking. |
| [`mesh_geometry`](mesh-asset/mesh-geometry.md) | Unpacked geometry as importers produce it. Optional attributes are either empty or one per position; missing normals are computed flat per face, missing tangents as the <code><a href="mesh-asset.html#symbol-tangent-generation">tangent_generation</a></code> given to <code>from_geometry</code> says (from the UV derivatives by default), missing uv1 is zero and missing colours are white. |
| [`mesh_streams`](mesh-asset/mesh-streams.md) |  |
| [`packed_vertex_attributes`](mesh-asset/packed-vertex-attributes.md) | Stream 1 of a vertex (32 bytes). |
| [`submesh`](mesh-asset/submesh.md) |  |

## Classes

| File | Description |
|---|---|
| [`mesh_asset`](mesh-asset/mesh-asset.md) |  |
| [`mesh_loader`](mesh-asset/mesh-loader.md) | The <code>conjure.renderer.mesh</code> loader: parses the blob on the load job, finalise is trivial, the fallback is the unit cube. |

## Enumerations

### `tangent_generation` {#symbol-tangent-generation}

```cpp
enum class tangent_generation : u8 {
    FROM_UVS = 0,
    FROM_NORMAL
};
```

How <code>mesh_asset::from_geometry</code> builds the tangents of a geometry that carries none. <code>FROM_UVS</code> takes them from the uv0 derivatives of the triangles around each vertex, made perpendicular to the normal, with the handedness of the UV winding: the frame a tangent-space normal map needs. <code>A</code> geometry without uv0, and a vertex whose UVs give no direction (degenerate or cancelling triangles), gets the <code>FROM_NORMAL</code> tangent. <code>FROM_NORMAL</code> reads no UV: the tangent is a unit vector perpendicular to the normal, handedness +1. Lighting works with it; a tangent-space normal map does not shade correctly, because the tangent does not follow the texture.

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-MESH_ASSET_TYPE_NAME"></a><code>MESH_ASSET_TYPE_NAME</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code> | <code class="signature"><span class="s">"conjure.renderer.mesh"</span></code> |  |
| <a id="variable-MESH_BLOB_VERSION"></a><code>MESH_BLOB_VERSION</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">1</span></code> |  |

## Macros

### `CONJURE_DECLARE_ASSET_TYPE` {#symbol-conjure-declare-asset-type}

```cpp
CONJURE_DECLARE_ASSET_TYPE(conjure::renderer::mesh_asset, "conjure.renderer.mesh")
```

## Functions

### Public

#### `pack_half` {#function-pack-half}

<div class="function-declaration" id="function-pack-half-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/types.html#symbol-u16">u16</a> pack_half(
    <a href="../core/types.html#symbol-f32">f32</a> value) <span class="k">noexcept</span>;</code></pre></div>

<p>IEEE 754 half precision conversion (round to nearest even; out-of-range values saturate to infinity, NaN stays NaN).</p>

#### `pack_rgba8` {#function-pack-rgba8}

<div class="function-declaration" id="function-pack-rgba8-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/types.html#symbol-u32">u32</a> pack_rgba8(
    <span class="k">const</span> <a href="../core/math/glm-types.html#symbol-vec4">math::vec4</a> &amp;colour) <span class="k">noexcept</span>;</code></pre></div>

<p>Packs a colour with components in [0, 1] into RGBA8 (red in the low <code><a href="../core/types.html#symbol-byte">byte</a></code>).</p>

#### `pack_snorm16` {#function-pack-snorm16}

<div class="function-declaration" id="function-pack-snorm16-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/types.html#symbol-i16">i16</a> pack_snorm16(
    <a href="../core/types.html#symbol-f32">f32</a> value) <span class="k">noexcept</span>;</code></pre></div>

<p>Packs a value in [-1, 1] into a 16-bit signed normalised integer (D3D/Vulkan SNORM rules).</p>

#### `pack_vertex_attributes` {#function-pack-vertex-attributes}

<div class="function-declaration" id="function-pack-vertex-attributes-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="mesh-asset/packed-vertex-attributes.html">packed_vertex_attributes</a> pack_vertex_attributes(
    <span class="k">const</span> <a href="../core/math/glm-types.html#symbol-vec3">math::vec3</a> &amp;normal,
    <span class="k">const</span> <a href="../core/math/glm-types.html#symbol-vec3">math::vec3</a> &amp;tangent,
    <a href="../core/types.html#symbol-f32">f32</a> tangentSign,
    <span class="k">const</span> <a href="../core/math/glm-types.html#symbol-vec2">math::vec2</a> &amp;uv0,
    <span class="k">const</span> <a href="../core/math/glm-types.html#symbol-vec2">math::vec2</a> &amp;uv1,
    <span class="k">const</span> <a href="../core/math/glm-types.html#symbol-vec4">math::vec4</a> &amp;colour) <span class="k">noexcept</span>;</code></pre></div>

<p>Packs one vertex&#x27;s attributes. The normal and tangent are normalised first (a zero vector packs to +<code>Z</code> / +<code>X</code>); <code>tangentSign</code> is the handedness stored in the normal&#x27;s w.</p>

#### `unpack_half` {#function-unpack-half}

<div class="function-declaration" id="function-unpack-half-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/types.html#symbol-f32">f32</a> unpack_half(
    <a href="../core/types.html#symbol-u16">u16</a> value) <span class="k">noexcept</span>;</code></pre></div>

#### `unpack_rgba8` {#function-unpack-rgba8}

<div class="function-declaration" id="function-unpack-rgba8-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/math/glm-types.html#symbol-vec4">math::vec4</a> unpack_rgba8(
    <a href="../core/types.html#symbol-u32">u32</a> packed) <span class="k">noexcept</span>;</code></pre></div>

#### `unpack_snorm16` {#function-unpack-snorm16}

<div class="function-declaration" id="function-unpack-snorm16-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/types.html#symbol-f32">f32</a> unpack_snorm16(
    <a href="../core/types.html#symbol-i16">i16</a> value) <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="conjure_declare_asset_type"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="cpu_geometry_view"></a>
<a id="cpu_geometry_viewindices"></a>
<a id="cpu_geometry_viewpositions"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="mesh_asset"></a>
<a id="mesh_asset_type_name"></a>
<a id="mesh_assetbounds"></a>
<a id="mesh_assetcpu_geometry"></a>
<a id="mesh_assetfrom_geometry"></a>
<a id="mesh_assetgpu_byte_size"></a>
<a id="mesh_assetindex_count"></a>
<a id="mesh_assetmbounds"></a>
<a id="mesh_assetmesh_asset"></a>
<a id="mesh_assetmesh_asset_1"></a>
<a id="mesh_assetmname"></a>
<a id="mesh_assetmstreams"></a>
<a id="mesh_assetmsubmeshes"></a>
<a id="mesh_assetname"></a>
<a id="mesh_assetoperator"></a>
<a id="mesh_assetoperator_1"></a>
<a id="mesh_assetparse_blob"></a>
<a id="mesh_assetquad"></a>
<a id="mesh_assetstreams"></a>
<a id="mesh_assetsubmeshes"></a>
<a id="mesh_assettriangle_count"></a>
<a id="mesh_assetunit_cube"></a>
<a id="mesh_assetuv_sphere"></a>
<a id="mesh_assetvertex_count"></a>
<a id="mesh_assetwrite_blob"></a>
<a id="mesh_blob_version"></a>
<a id="mesh_geometry"></a>
<a id="mesh_geometrycolours"></a>
<a id="mesh_geometryindices"></a>
<a id="mesh_geometrynormals"></a>
<a id="mesh_geometrypositions"></a>
<a id="mesh_geometrysubmeshindexcounts"></a>
<a id="mesh_geometrysubmeshmaterialslots"></a>
<a id="mesh_geometrytangents"></a>
<a id="mesh_geometryuv0"></a>
<a id="mesh_geometryuv1"></a>
<a id="mesh_loader"></a>
<a id="mesh_loadercreate_fallback_typed"></a>
<a id="mesh_loaderfinalise_typed"></a>
<a id="mesh_loaderload_cpu_typed"></a>
<a id="mesh_loadermesh_loader"></a>
<a id="mesh_loadermesh_loader_1"></a>
<a id="mesh_streams"></a>
<a id="mesh_streamsattributes"></a>
<a id="mesh_streamsindices"></a>
<a id="mesh_streamspositions"></a>
<a id="other-declarations"></a>
<a id="pack_half"></a>
<a id="pack_rgba8"></a>
<a id="pack_snorm16"></a>
<a id="pack_vertex_attributes"></a>
<a id="packed_vertex_attributes"></a>
<a id="packed_vertex_attributescolour"></a>
<a id="packed_vertex_attributesnormal"></a>
<a id="packed_vertex_attributesoperator"></a>
<a id="packed_vertex_attributestangent"></a>
<a id="packed_vertex_attributesuv0"></a>
<a id="packed_vertex_attributesuv1"></a>
<a id="structs-and-types"></a>
<a id="submesh"></a>
<a id="submeshbounds"></a>
<a id="submeshfirstindex"></a>
<a id="submeshindexcount"></a>
<a id="submeshmaterialslot"></a>
<a id="submeshoperator"></a>
<a id="tangent_generation"></a>
<a id="tangent_generationfrom_normal"></a>
<a id="tangent_generationfrom_uvs"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="unpack_half"></a>
<a id="unpack_rgba8"></a>
<a id="unpack_snorm16"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->

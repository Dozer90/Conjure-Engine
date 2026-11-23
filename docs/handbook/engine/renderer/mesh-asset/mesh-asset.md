---
kind: class
---

[Engine](../../../engine.md) → [Renderer](../../renderer.md) → [mesh_asset.h](../mesh-asset.md)

# mesh_asset
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::renderer` |
| Include File Path | `engine/include/conjure/renderer/mesh_asset.h` |
| Source File Path | `engine/src/renderer/assets/mesh_asset.cpp` |
| API ↗ | [Rendering](../../../../api/renderer/index.md), [Renderer assets](../../../../api/renderer/renderer-assets.md) |

## Syntax

```cpp
class CONJURE_API mesh_asset
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">mesh_asset();</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">mesh_asset(
    <span class="k">const</span> <a href="mesh-asset.html">mesh_asset</a> &amp;) = <span class="k">default</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">mesh_asset(
    <a href="mesh-asset.html">mesh_asset</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~mesh_asset();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mBounds</code></td><td><code class="signature"><a href="../../core/math/aabb/aabb.html">math::aabb</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mName</code></td><td><code class="signature">std::string</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mStreams</code></td><td><code class="signature"><a href="mesh-streams.html">mesh_streams</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mSubmeshes</code></td><td><code class="signature">std::vector&lt;<a href="submesh.html">submesh</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `bounds` {#function-bounds}

<div class="function-declaration" id="function-bounds-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../../core/math/aabb/aabb.html">math::aabb</a> &amp;bounds() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `cpu_geometry` {#function-cpu-geometry}

<div class="function-declaration" id="function-cpu-geometry-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="cpu-geometry-view.html">cpu_geometry_view</a> cpu_geometry() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Positions and indices for the editor&#x27;s CPU picking.</p>

#### `from_geometry` {#function-from-geometry}

<div class="function-declaration" id="function-from-geometry-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../core/result/result.html">core::result</a>&lt;<a href="mesh-asset.html">mesh_asset</a>&gt; from_geometry(
    <span class="k">const</span> <a href="mesh-geometry.html">mesh_geometry</a> &amp;geometry,
    std::string name = {},
    <a href="../mesh-asset.html#symbol-tangent-generation">tangent_generation</a> missingTangents = tangent_generation::FROM_UVS);</code></pre></div>

<p>Packs geometry into streams. Fails with <code>INVALID_ARGUMENT</code> for no positions, an attribute array whose size differs from the position count, an index count that is not a multiple of three, an index out of range, a non-finite position, or <code><a href="submesh.html">submesh</a></code> counts that do not partition the index list. Missing attributes are generated as described above; <code>missingTangents</code> chooses how and is not read when the geometry has tangents.</p>

#### `gpu_byte_size` {#function-gpu-byte-size}

<div class="function-declaration" id="function-gpu-byte-size-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u64">u64</a> gpu_byte_size() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Bytes of stream 0, stream 1 and the index buffer together.</p>

#### `index_count` {#function-index-count}

<div class="function-declaration" id="function-index-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> index_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `name` {#function-name}

<div class="function-declaration" id="function-name-1"><pre><code>[[<span class="na">nodiscard</span>]] std::string_view name() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `parse_blob` {#function-parse-blob}

<div class="function-declaration" id="function-parse-blob-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../core/result/result.html">core::result</a>&lt;<a href="mesh-asset.html">mesh_asset</a>&gt; parse_blob(
    std::span&lt;<span class="k">const</span> std::byte&gt; bytes);</code></pre></div>

<p>Parses a .cmesh blob. Fails with <code>renderer_error::INVALID_ASSET</code> naming the problem.</p>

#### `quad` {#function-quad}

<div class="function-declaration" id="function-quad-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="mesh-asset.html">mesh_asset</a> quad(
    <a href="../../core/types.html#symbol-f32">f32</a> halfSize = <span class="mi">0.5</span>f);</code></pre></div>

<p><code>A</code> quad in the XY plane facing +<code>Z</code> with the given half size.</p>

#### `streams` {#function-streams}

<div class="function-declaration" id="function-streams-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="mesh-streams.html">mesh_streams</a> &amp;streams() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `submeshes` {#function-submeshes}

<div class="function-declaration" id="function-submeshes-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> <a href="submesh.html">submesh</a>&gt; submeshes() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `triangle_count` {#function-triangle-count}

<div class="function-declaration" id="function-triangle-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> triangle_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `unit_cube` {#function-unit-cube}

<div class="function-declaration" id="function-unit-cube-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="mesh-asset.html">mesh_asset</a> unit_cube();</code></pre></div>

<p><code>A</code> unit cube centred on the origin with flat normals: the fallback mesh.</p>

#### `uv_sphere` {#function-uv-sphere}

<div class="function-declaration" id="function-uv-sphere-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="mesh-asset.html">mesh_asset</a> uv_sphere(
    <a href="../../core/types.html#symbol-f32">f32</a> radius,
    <a href="../../core/types.html#symbol-u32">u32</a> rings = <span class="mi">16</span>,
    <a href="../../core/types.html#symbol-u32">u32</a> segments = <span class="mi">32</span>);</code></pre></div>

<p><code>A</code> UV sphere of the given radius.</p>

#### `vertex_count` {#function-vertex-count}

<div class="function-declaration" id="function-vertex-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> vertex_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `write_blob` {#function-write-blob}

<div class="function-declaration" id="function-write-blob-1"><pre><code>[[<span class="na">nodiscard</span>]] std::vector&lt;std::byte&gt; write_blob() <span class="k">const</span>;</code></pre></div>

<p>Serialises the mesh as a .cmesh blob (little-endian, deterministic).</p>

## Operator Overloads

### Public

#### `operator=` {#operator-3d}

<div class="function-declaration" id="operator-3d-1"><pre><code><a href="mesh-asset.html">mesh_asset</a> &amp;<span class="k">operator</span>=(
    <span class="k">const</span> <a href="mesh-asset.html">mesh_asset</a> &amp;) = <span class="k">default</span>;</code></pre></div>

<div class="function-declaration" id="operator-3d-2"><pre><code><a href="mesh-asset.html">mesh_asset</a> &amp;<span class="k">operator</span>=(
    <a href="mesh-asset.html">mesh_asset</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="mesh-asset.html">mesh_asset</a> &amp;other) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

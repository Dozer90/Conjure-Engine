---
kind: class
---

[Engine](../../../engine.md) → [Physics](../../physics.md) → [collision_mesh_asset.h](../collision-mesh-asset.md)

# collision_mesh_asset
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::physics` |
| Include File Path | `engine/include/conjure/physics/collision_mesh_asset.h` |
| Source File Path | `engine/src/physics/collision_mesh_asset.cpp` |
| API ↗ | [Bodies, colliders and characters](../../../../api/physics/bodies-and-colliders.md), [Physics](../../../../api/physics/index.md) |

## Syntax

```cpp
class CONJURE_API collision_mesh_asset
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">collision_mesh_asset();</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">collision_mesh_asset(
    <span class="k">const</span> <a href="collision-mesh-asset.html">collision_mesh_asset</a> &amp;) = <span class="k">default</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">collision_mesh_asset(
    <a href="collision-mesh-asset.html">collision_mesh_asset</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~collision_mesh_asset();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mBounds</code></td><td><code class="signature"><a href="../../core/math/aabb/aabb.html">math::aabb</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mIndices</code></td><td><code class="signature">std::vector&lt;<a href="../../core/types.html#symbol-u32">u32</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mPositions</code></td><td><code class="signature">std::vector&lt;<a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mShapes</code></td><td><code class="signature">std::<wbr>shared_<wbr>ptr&lt;<wbr><span class="k">const</span> <a href="../collision-mesh-asset.html">detail::<wbr>collision_<wbr>mesh_<wbr>shapes</a>&gt;</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `bounds` {#function-bounds}

<div class="function-declaration" id="function-bounds-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../../core/math/aabb/aabb.html">math::aabb</a> &amp;bounds() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Bounds of every position (invalid for an empty mesh).</p>

#### `from_geometry` {#function-from-geometry}

<div class="function-declaration" id="function-from-geometry-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../core/result/result.html">core::result</a>&lt;<a href="collision-mesh-asset.html">collision_mesh_asset</a>&gt; from_geometry(
    std::span&lt;<span class="k">const</span> <a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a>&gt; positions,
    std::span&lt;<span class="k">const</span> <a href="../../core/types.html#symbol-u32">u32</a>&gt; indices);</code></pre></div>

<p>Builds a mesh from positions and triangle indices. <code>GEOMETRY_INVALID</code> when the index count is not a multiple of three, an index is out of range, a position is not finite or no non-degenerate triangle remains. Degenerate triangles (repeated vertex or zero area) are dropped; unreferenced vertices are kept.</p>

#### `from_payload` {#function-from-payload}

<div class="function-declaration" id="function-from-payload-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../core/result/result.html">core::result</a>&lt;<a href="collision-mesh-asset.html">collision_mesh_asset</a>&gt; from_payload(
    std::span&lt;<span class="k">const</span> std::byte&gt; payload);</code></pre></div>

<p>Parses a cooked payload. <code>GEOMETRY_INVALID</code> for a short or inconsistent buffer.</p>

#### `has_shapes` {#function-has-shapes}

<div class="function-declaration" id="function-has-shapes-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> has_shapes() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True once the loader built the Jolt shapes (<code>false</code> for a mesh made by <code>from_geometry</code>).</p>

#### `indices` {#function-indices}

<div class="function-declaration" id="function-indices-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> <a href="../../core/types.html#symbol-u32">u32</a>&gt; indices() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `positions` {#function-positions}

<div class="function-declaration" id="function-positions-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> <a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a>&gt; positions() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `set_shapes` {#function-set-shapes}

<div class="function-declaration" id="function-set-shapes-1"><pre><code><span class="k">void</span> set_shapes(
    std::shared_ptr&lt;<span class="k">const</span> <a href="../collision-mesh-asset.html">detail::collision_mesh_shapes</a>&gt; shapes) <span class="k">noexcept</span>;</code></pre></div>

<p>Installs the Jolt shapes. For the physics module&#x27;s own use.</p>

#### `shapes` {#function-shapes}

<div class="function-declaration" id="function-shapes-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../collision-mesh-asset.html">detail::collision_mesh_shapes</a> *shapes() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The Jolt shapes, or <code>nullptr</code>. For the physics module&#x27;s own use.</p>

#### `to_payload` {#function-to-payload}

<div class="function-declaration" id="function-to-payload-1"><pre><code>[[<span class="na">nodiscard</span>]] std::vector&lt;std::byte&gt; to_payload() <span class="k">const</span>;</code></pre></div>

<p>The cooked payload of this mesh (deterministic bytes).</p>

#### `triangle_count` {#function-triangle-count}

<div class="function-declaration" id="function-triangle-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> triangle_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `vertex_count` {#function-vertex-count}

<div class="function-declaration" id="function-vertex-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> vertex_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

## Operator Overloads

### Public

#### `operator=` {#operator-3d}

<div class="function-declaration" id="operator-3d-1"><pre><code><a href="collision-mesh-asset.html">collision_mesh_asset</a> &amp;<span class="k">operator</span>=(
    <span class="k">const</span> <a href="collision-mesh-asset.html">collision_mesh_asset</a> &amp;) = <span class="k">default</span>;</code></pre></div>

<div class="function-declaration" id="operator-3d-2"><pre><code><a href="collision-mesh-asset.html">collision_mesh_asset</a> &amp;<span class="k">operator</span>=(
    <a href="collision-mesh-asset.html">collision_mesh_asset</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

---
kind: struct
---

[Engine](../../../../engine.md) → [Core](../../../core.md) → [frustum.h](../frustum.md)

# frustum
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::math` |
| Include File Path | `engine/include/conjure/core/math/frustum.h` |
| Source File Path | `engine/src/core/math/frustum.cpp` |
| API ↗ | [Math](../../../../../api/core/math.md) |

## Syntax

```cpp
struct CONJURE_API frustum
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">planes</code></td><td><code class="signature">std::<wbr>array&lt;<wbr>plane,<wbr> FRUSTUM_<wbr>PLANE_<wbr>COUNT&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `contains` {#function-contains}

<div class="function-declaration" id="function-contains-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> contains(
    <span class="k">const</span> <a href="../aabb/aabb.html">aabb</a> &amp;box) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when the box is entirely inside or touching all six planes (n-vertex test).</p>

<div class="function-declaration" id="function-contains-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> contains(
    <a href="../glm-types.html#symbol-vec3">vec3</a> point) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when the point is inside or on every plane.</p>

#### `corners` {#function-corners}

<div class="function-declaration" id="function-corners-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> std::array&lt;vec3, <span class="mi">8</span>&gt; corners(
    <span class="k">const</span> <a href="../glm-types.html#symbol-mat4">mat4</a> &amp;inverseViewProjection) <span class="k">noexcept</span>;</code></pre></div>

<p>The eight corners of the volume described by an inverse view-projection matrix, in the order near bottom-left, near bottom-right, near top-right, near top-left, then the same for the far plane. Requires a finite projection: an infinite projection has no finite far corners, so the call asserts once and leaves the affected corners at their undivided clip-space value. With a reverse-<code>Z</code> matrix the first four corners lie on the far plane and the last four on the near plane.</p>

#### `from_view_projection` {#function-from-view-projection}

<div class="function-declaration" id="function-from-view-projection-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="frustum.html">frustum</a> from_view_projection(
    <span class="k">const</span> <a href="../glm-types.html#symbol-mat4">mat4</a> &amp;viewProjection) <span class="k">noexcept</span>;</code></pre></div>

<p>Extracts the planes from a view-projection (or projection) matrix using the Gribb-Hartmann method for clip space -w &lt;= x &lt;= w, -w &lt;= y &lt;= w, 0 &lt;= z &lt;= w (<code>GLM_FORCE_DEPTH_ZERO_TO_ONE</code>). Works for perspective, infinite and orthographic projections; planes are normalised. With a reverse-<code>Z</code> matrix the planes still bound the same volume, but <code>NEAR_PLANE</code> and <code>FAR_PLANE</code> are swapped; <code>renderer::frustum_from_clip</code> corrects the labels.</p>

#### `get` {#function-get}

<div class="function-declaration" id="function-get-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../plane/plane.html">plane</a> &amp;get(
    <a href="../frustum.html#symbol-frustum-plane">frustum_plane</a> which) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The plane with the given index.</p>

#### `intersects` {#function-intersects}

<div class="function-declaration" id="function-intersects-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> intersects(
    <span class="k">const</span> <a href="../aabb/aabb.html">aabb</a> &amp;box) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Conservative box test using the p-vertex (the corner furthest along each plane normal): <code>false</code> only when the box is entirely behind at least one plane, so boxes near an edge may report an intersection they do not have. Invalid boxes never intersect.</p>

#### `intersects_sphere` {#function-intersects-sphere}

<div class="function-declaration" id="function-intersects-sphere-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> intersects_sphere(
    <a href="../glm-types.html#symbol-vec3">vec3</a> center,
    <a href="../../types.html#symbol-f32">f32</a> radius) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when the sphere is not entirely behind any plane (exact for the planes, so conservative near edges like <code><a href="#function-intersects-1">intersects</a>(aabb)</code>). Negative radii are treated as 0.</p>

## Operator Overloads

### Public

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="frustum.html">frustum</a> &amp;other) <span class="k">const</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->

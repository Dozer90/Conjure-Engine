---
kind: struct
---

[Engine](../../../../engine.md) → [Core](../../../core.md) → [ray.h](../ray.md)

# ray
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::math` |
| Include File Path | `engine/include/conjure/core/math/ray.h` |
| Source File Path | `engine/src/core/math/ray.cpp` |
| API ↗ | [Math](../../../../../api/core/math.md) |

## Syntax

```cpp
struct CONJURE_API ray
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">direction</code></td><td><code class="signature"><a href="../glm-types.html#symbol-vec3">vec3</a></code></td><td><code class="signature"><a href="../glm-types.html#symbol-vec3">vec3</a>(<span class="mi">0.0</span>f,<wbr> <span class="mi">0.0</span>f,<wbr> -<span class="mi">1.0</span>f)</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">origin</code></td><td><code class="signature"><a href="../glm-types.html#symbol-vec3">vec3</a></code></td><td><code class="signature"><a href="../glm-types.html#symbol-vec3">vec3</a>(<span class="mi">0.0</span>f)</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `at` {#function-at}

<div class="function-declaration" id="function-at-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../glm-types.html#symbol-vec3">vec3</a> at(
    <a href="../../types.html#symbol-f32">f32</a> t) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The point at parameter t.</p>

#### `from_points` {#function-from-points}

<div class="function-declaration" id="function-from-points-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="ray.html">ray</a> from_points(
    <a href="../glm-types.html#symbol-vec3">vec3</a> a,
    <a href="../glm-types.html#symbol-vec3">vec3</a> b) <span class="k">noexcept</span>;</code></pre></div>

<p>The <code><a href="ray.html">ray</a></code> from a to b with a normalised direction; a == b yields the default direction.</p>

#### `from_viewport` {#function-from-viewport}

<div class="function-declaration" id="function-from-viewport-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="ray.html">ray</a> from_viewport(
    <a href="../glm-types.html#symbol-vec2">vec2</a> pixel,
    <a href="../glm-types.html#symbol-uvec2">uvec2</a> viewportSize,
    <span class="k">const</span> <a href="../glm-types.html#symbol-mat4">mat4</a> &amp;inverseViewProjection) <span class="k">noexcept</span>;</code></pre></div>

<p>The world-space pick <code><a href="ray.html">ray</a></code> through a viewport pixel (origin top-left, y down; pixel centres are at +0.5). The origin lies on the near plane (depth 0). Works for perspective, infinite and orthographic projections. <code>A</code> zero-sized viewport asserts and uses the viewport centre. Assumes forward depth (near plane at depth 0, far plane at 1, as <code>math::perspective</code> builds). The renderer&#x27;s matrices are reverse-<code>Z</code>: use <code>renderer::ray_from_viewport</code> for them.</p>

#### `intersect` {#function-intersect}

<div class="function-declaration" id="function-intersect-1"><pre><code>[[<span class="na">nodiscard</span>]] std::optional&lt;<a href="../../types.html#symbol-f32">f32</a>&gt; intersect(
    <span class="k">const</span> <a href="../aabb/aabb.html">aabb</a> &amp;box) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Slab test against a box. Returns 0 when the origin is inside, nullopt for a miss, an invalid box or a box entirely behind the origin. Rays parallel to a slab are handled.</p>

#### `intersect_plane` {#function-intersect-plane}

<div class="function-declaration" id="function-intersect-plane-1"><pre><code>[[<span class="na">nodiscard</span>]] std::optional&lt;<a href="../../types.html#symbol-f32">f32</a>&gt; intersect_plane(
    <span class="k">const</span> <a href="../plane/plane.html">plane</a> &amp;surface) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Hit with an infinite plane from either side. Returns nullopt when the <code><a href="ray.html">ray</a></code> is parallel to the plane (including when it lies in it) or the plane is behind the origin.</p>

#### `intersect_sphere` {#function-intersect-sphere}

<div class="function-declaration" id="function-intersect-sphere-1"><pre><code>[[<span class="na">nodiscard</span>]] std::optional&lt;<a href="../../types.html#symbol-f32">f32</a>&gt; intersect_sphere(
    <a href="../glm-types.html#symbol-vec3">vec3</a> center,
    <a href="../../types.html#symbol-f32">f32</a> radius) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Nearest hit with a sphere. Returns 0 when the origin is inside; nullopt for a miss, a sphere behind the origin or a negative radius.</p>

#### `intersect_triangle` {#function-intersect-triangle}

<div class="function-declaration" id="function-intersect-triangle-1"><pre><code>[[<span class="na">nodiscard</span>]] std::optional&lt;<a href="../../types.html#symbol-f32">f32</a>&gt; intersect_triangle(
    <a href="../glm-types.html#symbol-vec3">vec3</a> a,
    <a href="../glm-types.html#symbol-vec3">vec3</a> b,
    <a href="../glm-types.html#symbol-vec3">vec3</a> c,
    <span class="k">bool</span> cullBackFaces = true) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Moller-Trumbore triangle test. Triangles are counter-clockwise when seen from the front; with <code>cullBackFaces</code> the back side is ignored. Degenerate (collinear) triangles and rays parallel to the triangle never hit; the test is relative to the triangle size and the direction length, so millimetre triangles and grazing angles work. Edges and vertices count as hits.</p>

## Operator Overloads

### Public

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="ray.html">ray</a> &amp;other) <span class="k">const</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->

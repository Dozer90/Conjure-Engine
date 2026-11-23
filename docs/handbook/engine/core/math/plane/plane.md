---
kind: struct
---

[Engine](../../../../engine.md) → [Core](../../../core.md) → [plane.h](../plane.md)

# plane
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::math` |
| Include File Path | `engine/include/conjure/core/math/plane.h` |
| Source File Path | N/A |
| API ↗ | [Math](../../../../../api/core/math.md) |

## Syntax

```cpp
struct plane
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">distance</code></td><td><code class="signature"><a href="../../types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">0.0</span>f</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">normal</code></td><td><code class="signature"><a href="../glm-types.html#symbol-vec3">vec3</a></code></td><td><code class="signature"><a href="../glm-types.html#symbol-vec3">vec3</a>(<span class="mi">0.0</span>f,<wbr> <span class="mi">1.0</span>f,<wbr> <span class="mi">0.0</span>f)</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `from_point_normal` {#function-from-point-normal}

<div class="function-declaration" id="function-from-point-normal-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="plane.html">plane</a> from_point_normal(
    <a href="../glm-types.html#symbol-vec3">vec3</a> point,
    <a href="../glm-types.html#symbol-vec3">vec3</a> direction) <span class="k">noexcept</span>;</code></pre></div>

<p>The <code><a href="plane.html">plane</a></code> through point with the given normal, normalised. <code>A</code> zero normal yields the default (<code>Y</code> up) <code><a href="plane.html">plane</a></code> through the point.</p>

#### `from_points` {#function-from-points}

<div class="function-declaration" id="function-from-points-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="plane.html">plane</a> from_points(
    <a href="../glm-types.html#symbol-vec3">vec3</a> a,
    <a href="../glm-types.html#symbol-vec3">vec3</a> b,
    <a href="../glm-types.html#symbol-vec3">vec3</a> c) <span class="k">noexcept</span>;</code></pre></div>

<p>The <code><a href="plane.html">plane</a></code> through three counter-clockwise points (normal = <code>cross(b - a, c - a)</code>), normalised. Collinear points yield the default (<code>Y</code> up) <code><a href="plane.html">plane</a></code> through a.</p>

#### `normalize` {#function-normalize}

<div class="function-declaration" id="function-normalize-1"><pre><code><span class="k">void</span> normalize() <span class="k">noexcept</span>;</code></pre></div>

<p>Scales the equation so that the normal has unit length. <code>A</code> zero normal is left as is.</p>

#### `normalized` {#function-normalized}

<div class="function-declaration" id="function-normalized-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="plane.html">plane</a> normalized() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p><code>A</code> copy with <code><a href="#function-normalize-1">normalize</a>()</code> applied.</p>

#### `project` {#function-project}

<div class="function-declaration" id="function-project-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../glm-types.html#symbol-vec3">vec3</a> project(
    <a href="../glm-types.html#symbol-vec3">vec3</a> point) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The orthogonal projection of a point onto the <code><a href="plane.html">plane</a></code> (requires a unit normal).</p>

#### `signed_distance` {#function-signed-distance}

<div class="function-declaration" id="function-signed-distance-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-f32">f32</a> signed_distance(
    <a href="../glm-types.html#symbol-vec3">vec3</a> point) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p><code>dot(normal, point)</code> + distance: positive in front of the <code><a href="plane.html">plane</a></code>, negative behind, zero on it. <code>A</code> metric distance only when the normal has unit length.</p>

## Operator Overloads

### Public

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="plane.html">plane</a> &amp;other) <span class="k">const</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->

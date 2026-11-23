---
kind: struct
---

[Engine](../../../../engine.md) → [Core](../../../core.md) → [aabb.h](../aabb.md)

# aabb
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::math` |
| Include File Path | `engine/include/conjure/core/math/aabb.h` |
| Source File Path | `engine/src/core/math/aabb.cpp` |
| API ↗ | [Math](../../../../../api/core/math.md) |

## Syntax

```cpp
struct CONJURE_API aabb
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">max</code></td><td><code class="signature"><a href="../glm-types.html#symbol-vec3">vec3</a></code></td><td><code class="signature"><a href="../glm-types.html#symbol-vec3">vec3</a>(-F32_MAX)</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">min</code></td><td><code class="signature"><a href="../glm-types.html#symbol-vec3">vec3</a></code></td><td><code class="signature"><a href="../glm-types.html#symbol-vec3">vec3</a>(F32_MAX)</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `center` {#function-center}

<div class="function-declaration" id="function-center-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../glm-types.html#symbol-vec3">vec3</a> center() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Midpoint of min and max.</p>

#### `closest_point` {#function-closest-point}

<div class="function-declaration" id="function-closest-point-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../glm-types.html#symbol-vec3">vec3</a> closest_point(
    <a href="../glm-types.html#symbol-vec3">vec3</a> point) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The point inside or on the box nearest to point (point itself when inside). An invalid box has no surface and returns point unchanged.</p>

#### `contains` {#function-contains}

<div class="function-declaration" id="function-contains-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> contains(
    <a href="../glm-types.html#symbol-vec3">vec3</a> point) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when the point lies inside or on the box. Always <code>false</code> for an invalid box.</p>

<div class="function-declaration" id="function-contains-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> contains(
    <span class="k">const</span> <a href="aabb.html">aabb</a> &amp;other) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when other lies entirely inside or on this box. False when either box is invalid.</p>

#### `corners` {#function-corners}

<div class="function-declaration" id="function-corners-1"><pre><code>[[<span class="na">nodiscard</span>]] std::array&lt;vec3, <span class="mi">8</span>&gt; corners() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The eight corners, in the order min, +x, +y, +xy, +z, +xz, +yz, max (bit 0 = x at max, bit 1 = y at max, bit 2 = z at max).</p>

#### `distance_squared` {#function-distance-squared}

<div class="function-declaration" id="function-distance-squared-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-f32">f32</a> distance_squared(
    <a href="../glm-types.html#symbol-vec3">vec3</a> point) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Squared distance from point to the box surface, 0 when inside. 0 for an invalid box (like <code>surface_area</code> and volume).</p>

#### `expand` {#function-expand}

<div class="function-declaration" id="function-expand-1"><pre><code><span class="k">void</span> expand(
    <a href="../glm-types.html#symbol-vec3">vec3</a> point) <span class="k">noexcept</span>;</code></pre></div>

<p>Grows the box to include a point.</p>

<div class="function-declaration" id="function-expand-2"><pre><code><span class="k">void</span> expand(
    <span class="k">const</span> <a href="aabb.html">aabb</a> &amp;other) <span class="k">noexcept</span>;</code></pre></div>

<p>Grows the box to include another box. An invalid other box is ignored.</p>

#### `extents` {#function-extents}

<div class="function-declaration" id="function-extents-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../glm-types.html#symbol-vec3">vec3</a> extents() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Half the size on each axis.</p>

#### `from_center_extents` {#function-from-center-extents}

<div class="function-declaration" id="function-from-center-extents-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="aabb.html">aabb</a> from_center_extents(
    <a href="../glm-types.html#symbol-vec3">vec3</a> centerPoint,
    <a href="../glm-types.html#symbol-vec3">vec3</a> halfExtents) <span class="k">noexcept</span>;</code></pre></div>

<p><code>A</code> box with the given center and half-size on each axis (negative extents are made positive).</p>

#### `from_points` {#function-from-points}

<div class="function-declaration" id="function-from-points-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="aabb.html">aabb</a> from_points(
    <a href="../glm-types.html#symbol-vec3">vec3</a> a,
    <a href="../glm-types.html#symbol-vec3">vec3</a> b) <span class="k">noexcept</span>;</code></pre></div>

<p><code>A</code> box from two arbitrary corners (they need not be ordered).</p>

#### `inflated` {#function-inflated}

<div class="function-declaration" id="function-inflated-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="aabb.html">aabb</a> inflated(
    <a href="../../types.html#symbol-f32">f32</a> amount) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Grows the box by amount on every side (shrinks for negative amounts).</p>

#### `intersection` {#function-intersection}

<div class="function-declaration" id="function-intersection-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="aabb.html">aabb</a> intersection(
    <span class="k">const</span> <a href="aabb.html">aabb</a> &amp;other) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The overlap of both boxes; invalid when they do not intersect.</p>

#### `intersects` {#function-intersects}

<div class="function-declaration" id="function-intersects-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> intersects(
    <span class="k">const</span> <a href="aabb.html">aabb</a> &amp;other) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when the boxes overlap or touch. False when either box is invalid.</p>

#### `is_valid` {#function-is-valid}

<div class="function-declaration" id="function-is-valid-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_valid() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when min &lt;= max on every axis (a point or a degenerate flat box is valid).</p>

#### `size` {#function-size}

<div class="function-declaration" id="function-size-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../glm-types.html#symbol-vec3">vec3</a> size() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>max - min.</p>

#### `surface_area` {#function-surface-area}

<div class="function-declaration" id="function-surface-area-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-f32">f32</a> surface_area() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Total area of the six faces; 0 for an invalid box.</p>

#### `transformed` {#function-transformed}

<div class="function-declaration" id="function-transformed-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="aabb.html">aabb</a> transformed(
    <span class="k">const</span> <a href="../glm-types.html#symbol-mat4">mat4</a> &amp;matrix) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The axis-aligned box enclosing all eight transformed corners (conservative under rotation). Affine matrices only: the bottom row is ignored (no perspective divide), so a projection matrix yields a meaningless box; use <code>frustum::corners</code> for view volumes. An invalid box stays invalid.</p>

#### `volume` {#function-volume}

<div class="function-declaration" id="function-volume-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-f32">f32</a> volume() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Enclosed volume; 0 for an invalid box.</p>

## Operator Overloads

### Public

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="aabb.html">aabb</a> &amp;other) <span class="k">const</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->

---
kind: struct
---

[Engine](../../../../engine.md) → [Core](../../../core.md) → [transform.h](../transform.md)

# transform
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::math` |
| Include File Path | `engine/include/conjure/core/math/transform.h` |
| Source File Path | `engine/src/core/math/transform.cpp` |
| API ↗ | [Math](../../../../../api/core/math.md) |

## Syntax

```cpp
struct CONJURE_API transform
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">position</code></td><td><code class="signature"><a href="../glm-types.html#symbol-vec3">vec3</a></code></td><td><code class="signature"><a href="../glm-types.html#symbol-vec3">vec3</a>(<span class="mi">0.0</span>f)</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">rotation</code></td><td><code class="signature"><a href="../glm-types.html#symbol-quat">quat</a></code></td><td><code class="signature"><a href="../glm-types.html#symbol-quat">quat</a>(<span class="mi">1.0</span>f,<wbr> <span class="mi">0.0</span>f,<wbr> <span class="mi">0.0</span>f,<wbr> <span class="mi">0.0</span>f)</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">scale</code></td><td><code class="signature"><a href="../glm-types.html#symbol-vec3">vec3</a></code></td><td><code class="signature"><a href="../glm-types.html#symbol-vec3">vec3</a>(<span class="mi">1.0</span>f)</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `forward` {#function-forward}

<div class="function-declaration" id="function-forward-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../glm-types.html#symbol-vec3">vec3</a> forward() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The rotated -<code>Z</code> axis.</p>

#### `from_matrix` {#function-from-matrix}

<div class="function-declaration" id="function-from-matrix-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="transform.html">transform</a> from_matrix(
    <span class="k">const</span> <a href="../glm-types.html#symbol-mat4">mat4</a> &amp;matrix) <span class="k">noexcept</span>;</code></pre></div>

<p>Decomposes a TRS matrix (no shear). <code>A</code> negative determinant (mirroring) is expressed as a negative x scale; a zero-length axis yields a zero scale on that axis and the identity direction for its rotation column.</p>

#### `from_position` {#function-from-position}

<div class="function-declaration" id="function-from-position-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="transform.html">transform</a> from_position(
    <a href="../glm-types.html#symbol-vec3">vec3</a> value) <span class="k">noexcept</span>;</code></pre></div>

<p><code>A</code> <code><a href="transform.html">transform</a></code> with only a position.</p>

#### `from_rotation` {#function-from-rotation}

<div class="function-declaration" id="function-from-rotation-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="transform.html">transform</a> from_rotation(
    <a href="../glm-types.html#symbol-quat">quat</a> value) <span class="k">noexcept</span>;</code></pre></div>

<p><code>A</code> <code><a href="transform.html">transform</a></code> with only a rotation.</p>

#### `from_scale` {#function-from-scale}

<div class="function-declaration" id="function-from-scale-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="transform.html">transform</a> from_scale(
    <a href="../glm-types.html#symbol-vec3">vec3</a> value) <span class="k">noexcept</span>;</code></pre></div>

<p><code>A</code> <code><a href="transform.html">transform</a></code> with only a scale.</p>

#### `inverse` {#function-inverse}

<div class="function-declaration" id="function-inverse-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="transform.html">transform</a> inverse() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The <code><a href="transform.html">transform</a></code> that undoes this one: <code><a href="#function-inverse-1">inverse</a>()</code> * (*this) is the identity for every <code><a href="transform.html">transform</a></code>, and <code><a href="#function-inverse-1">inverse</a>()</code>.<code><a href="#function-to-matrix-1">to_matrix</a>()</code> equals <code>glm::inverse(<a href="#function-to-matrix-1">to_matrix</a>())</code> when the scale is uniform. <code>A</code> (nearly) zero scale component inverts to zero instead of infinity.</p>

#### `inverse_transform_direction` {#function-inverse-transform-direction}

<div class="function-declaration" id="function-inverse-transform-direction-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../glm-types.html#symbol-vec3">vec3</a> inverse_transform_direction(
    <a href="../glm-types.html#symbol-vec3">vec3</a> direction) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Rotates a direction from the parent space back into local space.</p>

#### `inverse_transform_point` {#function-inverse-transform-point}

<div class="function-declaration" id="function-inverse-transform-point-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../glm-types.html#symbol-vec3">vec3</a> inverse_transform_point(
    <a href="../glm-types.html#symbol-vec3">vec3</a> point) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Maps a point from the parent space back into this <code><a href="transform.html">transform</a></code>&#x27;s local space; exact for every scale (the exact inverse of <code>transform_point</code>, including non-uniform scale).</p>

#### `lerp` {#function-lerp}

<div class="function-declaration" id="function-lerp-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="transform.html">transform</a> lerp(
    <span class="k">const</span> <a href="transform.html">transform</a> &amp;a,
    <span class="k">const</span> <a href="transform.html">transform</a> &amp;b,
    <a href="../../types.html#symbol-f32">f32</a> t) <span class="k">noexcept</span>;</code></pre></div>

<p>Interpolates position and scale linearly and the rotation by shortest-path slerp. t is not clamped (like <code>math::lerp</code>): values outside [0, 1] extrapolate.</p>

#### `look_at` {#function-look-at}

<div class="function-declaration" id="function-look-at-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="transform.html">transform</a> look_at(
    <a href="../glm-types.html#symbol-vec3">vec3</a> eye,
    <a href="../glm-types.html#symbol-vec3">vec3</a> target,
    <a href="../glm-types.html#symbol-vec3">vec3</a> upHint) <span class="k">noexcept</span>;</code></pre></div>

<p><code>A</code> <code><a href="transform.html">transform</a></code> at eye whose <code><a href="#function-forward-1">forward</a>()</code> points at target with the given up hint and unit scale. <code>A</code> target equal to eye keeps the identity rotation; an up hint parallel to the view direction (or zero) is replaced by the world axis least aligned with the direction so the result is always a valid rotation.</p>

#### `right` {#function-right}

<div class="function-declaration" id="function-right-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../glm-types.html#symbol-vec3">vec3</a> right() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The rotated +<code>X</code> axis.</p>

#### `to_matrix` {#function-to-matrix}

<div class="function-declaration" id="function-to-matrix-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../glm-types.html#symbol-mat4">mat4</a> to_matrix() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The 4x4 matrix <code>translate(position)</code> * <code>rotate(rotation)</code> * <code>scale(scale)</code>.</p>

#### `transform_direction` {#function-transform-direction}

<div class="function-declaration" id="function-transform-direction-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../glm-types.html#symbol-vec3">vec3</a> transform_direction(
    <a href="../glm-types.html#symbol-vec3">vec3</a> direction) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Applies only the rotation to a direction (no scale, no translation).</p>

#### `transform_point` {#function-transform-point}

<div class="function-declaration" id="function-transform-point-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../glm-types.html#symbol-vec3">vec3</a> transform_point(
    <a href="../glm-types.html#symbol-vec3">vec3</a> point) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Applies scale, rotation and translation to a point in this <code><a href="transform.html">transform</a></code>&#x27;s local space.</p>

#### `up` {#function-up}

<div class="function-declaration" id="function-up-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../glm-types.html#symbol-vec3">vec3</a> up() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The rotated +<code>Y</code> axis.</p>

## Operator Overloads

### Public

#### `operator*` {#operator-2a}

<div class="function-declaration" id="operator-2a-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="transform.html">transform</a> <span class="k">operator</span>*(
    <span class="k">const</span> <a href="transform.html">transform</a> &amp;child) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Composes parent * child: the child expressed in the parent&#x27;s space is moved into the parent&#x27;s parent space. Equals <code>from_matrix(to_matrix() * child.to_matrix())</code> exactly when the parent scale is uniform or the child rotation is the identity.</p>

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="transform.html">transform</a> &amp;other) <span class="k">const</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->

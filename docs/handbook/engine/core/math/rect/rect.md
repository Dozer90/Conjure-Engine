---
kind: struct
---

[Engine](../../../../engine.md) → [Core](../../../core.md) → [rect.h](../rect.md)

# rect
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::math` |
| Include File Path | `engine/include/conjure/core/math/rect.h` |
| Source File Path | N/A |
| API ↗ | [Math](../../../../../api/core/math.md) |

## Syntax

```cpp
struct rect
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">max</code></td><td><code class="signature"><a href="../glm-types.html#symbol-vec2">vec2</a></code></td><td><code class="signature"><a href="../glm-types.html#symbol-vec2">vec2</a>(<span class="mi">0.0</span>f)</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">min</code></td><td><code class="signature"><a href="../glm-types.html#symbol-vec2">vec2</a></code></td><td><code class="signature"><a href="../glm-types.html#symbol-vec2">vec2</a>(<span class="mi">0.0</span>f)</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `area` {#function-area}

<div class="function-declaration" id="function-area-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-f32">f32</a> area() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>width * height; 0 for an invalid rectangle.</p>

#### `center` {#function-center}

<div class="function-declaration" id="function-center-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../glm-types.html#symbol-vec2">vec2</a> center() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Midpoint of min and max.</p>

#### `closest_point` {#function-closest-point}

<div class="function-declaration" id="function-closest-point-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../glm-types.html#symbol-vec2">vec2</a> closest_point(
    <a href="../glm-types.html#symbol-vec2">vec2</a> point) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The point inside or on the rectangle nearest to point (point itself when inside). An invalid rectangle has no surface and returns point unchanged.</p>

#### `contains` {#function-contains}

<div class="function-declaration" id="function-contains-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> contains(
    <a href="../glm-types.html#symbol-vec2">vec2</a> point) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when the point lies inside or on the rectangle.</p>

<div class="function-declaration" id="function-contains-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> contains(
    <span class="k">const</span> <a href="rect.html">rect</a> &amp;other) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when other lies entirely inside or on this rectangle (<code>false</code> when either is invalid).</p>

#### `expand` {#function-expand}

<div class="function-declaration" id="function-expand-1"><pre><code><span class="k">void</span> expand(
    <a href="../glm-types.html#symbol-vec2">vec2</a> point) <span class="k">noexcept</span>;</code></pre></div>

<p>Grows the rectangle to include a point.</p>

<div class="function-declaration" id="function-expand-2"><pre><code><span class="k">void</span> expand(
    <span class="k">const</span> <a href="rect.html">rect</a> &amp;other) <span class="k">noexcept</span>;</code></pre></div>

<p>Grows the rectangle to include another (an invalid other is ignored).</p>

#### `from_center_extents` {#function-from-center-extents}

<div class="function-declaration" id="function-from-center-extents-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="rect.html">rect</a> from_center_extents(
    <a href="../glm-types.html#symbol-vec2">vec2</a> centerPoint,
    <a href="../glm-types.html#symbol-vec2">vec2</a> halfExtents) <span class="k">noexcept</span>;</code></pre></div>

<p><code>A</code> rectangle from its centre and half-size on each axis (negative extents are made positive).</p>

#### `from_points` {#function-from-points}

<div class="function-declaration" id="function-from-points-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="rect.html">rect</a> from_points(
    <a href="../glm-types.html#symbol-vec2">vec2</a> a,
    <a href="../glm-types.html#symbol-vec2">vec2</a> b) <span class="k">noexcept</span>;</code></pre></div>

<p><code>A</code> rectangle from two arbitrary corners.</p>

#### `from_position_size` {#function-from-position-size}

<div class="function-declaration" id="function-from-position-size-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="rect.html">rect</a> from_position_size(
    <a href="../glm-types.html#symbol-vec2">vec2</a> position,
    <a href="../glm-types.html#symbol-vec2">vec2</a> extent) <span class="k">noexcept</span>;</code></pre></div>

<p>The rectangle spanned by position and position + extent; a negative extent component extends to the other side of position (<code><a href="#function-from-position-size-1">from_position_size</a>((10, 10), (-4, 2))</code> is min (6, 10), max (10, 12)).</p>

#### `height` {#function-height}

<div class="function-declaration" id="function-height-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-f32">f32</a> height() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p><code>max.y</code> - <code>min.y</code>.</p>

#### `inflated` {#function-inflated}

<div class="function-declaration" id="function-inflated-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="rect.html">rect</a> inflated(
    <a href="../../types.html#symbol-f32">f32</a> amount) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p><code>A</code> copy grown by amount on every side (shrunk for negative amounts).</p>

#### `intersection` {#function-intersection}

<div class="function-declaration" id="function-intersection-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="rect.html">rect</a> intersection(
    <span class="k">const</span> <a href="rect.html">rect</a> &amp;other) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The overlap of both rectangles; invalid when they do not intersect.</p>

#### `intersects` {#function-intersects}

<div class="function-declaration" id="function-intersects-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> intersects(
    <span class="k">const</span> <a href="rect.html">rect</a> &amp;other) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when the rectangles overlap or touch (<code>false</code> when either is invalid).</p>

#### `is_empty` {#function-is-empty}

<div class="function-declaration" id="function-is-empty-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_empty() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when the rectangle has no area (invalid or zero width or height).</p>

#### `is_valid` {#function-is-valid}

<div class="function-declaration" id="function-is-valid-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_valid() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when min &lt;= max on both axes (an empty rectangle is valid).</p>

#### `size` {#function-size}

<div class="function-declaration" id="function-size-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../glm-types.html#symbol-vec2">vec2</a> size() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>max - min.</p>

#### `translated` {#function-translated}

<div class="function-declaration" id="function-translated-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="rect.html">rect</a> translated(
    <a href="../glm-types.html#symbol-vec2">vec2</a> offset) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p><code>A</code> copy moved by offset.</p>

#### `width` {#function-width}

<div class="function-declaration" id="function-width-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-f32">f32</a> width() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p><code>max.x</code> - <code>min.x</code>.</p>

## Operator Overloads

### Public

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="rect.html">rect</a> &amp;other) <span class="k">const</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->

---
kind: struct
---

[Engine](../../../../engine.md) → [Core](../../../core.md) → [rect.h](../rect.md)

# urect
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::math` |
| Include File Path | `engine/include/conjure/core/math/rect.h` |
| Source File Path | N/A |
| API ↗ | [Math](../../../../../api/core/math.md) |

## Syntax

```cpp
struct urect
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">height</code></td><td><code class="signature"><a href="../../types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">width</code></td><td><code class="signature"><a href="../../types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">x</code></td><td><code class="signature"><a href="../../types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">y</code></td><td><code class="signature"><a href="../../types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `area` {#function-area}

<div class="function-declaration" id="function-area-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-u64">u64</a> area() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Number of covered pixels.</p>

#### `bottom` {#function-bottom}

<div class="function-declaration" id="function-bottom-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-u32">u32</a> bottom() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>One past the last covered row. Asserts when y + height overflows <code><a href="../../types.html#symbol-u32">u32</a></code>.</p>

#### `contains` {#function-contains}

<div class="function-declaration" id="function-contains-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> contains(
    <a href="../../types.html#symbol-u32">u32</a> pixelX,
    <a href="../../types.html#symbol-u32">u32</a> pixelY) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when the pixel at (<code>pixelX</code>, <code>pixelY</code>) is covered.</p>

<div class="function-declaration" id="function-contains-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> contains(
    <span class="k">const</span> <a href="urect.html">urect</a> &amp;other) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when every pixel of other is covered by this rectangle (an empty other is contained when its origin is inside or on the edge).</p>

#### `intersection` {#function-intersection}

<div class="function-declaration" id="function-intersection-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="urect.html">urect</a> intersection(
    <span class="k">const</span> <a href="urect.html">urect</a> &amp;other) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The pixels covered by both rectangles; empty (zero size at the clamped origin) when they do not intersect.</p>

#### `intersects` {#function-intersects}

<div class="function-declaration" id="function-intersects-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> intersects(
    <span class="k">const</span> <a href="urect.html">urect</a> &amp;other) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when both rectangles cover at least one common pixel.</p>

#### `is_empty` {#function-is-empty}

<div class="function-declaration" id="function-is-empty-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_empty() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when the rectangle covers no pixel.</p>

#### `right` {#function-right}

<div class="function-declaration" id="function-right-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-u32">u32</a> right() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>One past the last covered column. Asserts when x + width overflows <code><a href="../../types.html#symbol-u32">u32</a></code>.</p>

#### `to_rect` {#function-to-rect}

<div class="function-declaration" id="function-to-rect-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="rect.html">rect</a> to_rect() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The same region as a float rectangle (min = origin, max = origin + size).</p>

## Operator Overloads

### Public

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="urect.html">urect</a> &amp;other) <span class="k">const</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->

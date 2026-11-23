---
kind: class
---

[Engine](../../../engine.md) → [Renderer](../../renderer.md) → [debug_draw.h](../debug-draw.md)

# debug_draw_buffer
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::renderer` |
| Include File Path | `engine/include/conjure/renderer/debug_draw.h` |
| Source File Path | `engine/src/renderer/debug_draw.cpp` |
| API ↗ | [Rendering](../../../../api/renderer/index.md), [Rendering a frame](../../../../api/renderer/rendering-a-frame.md) |

## Syntax

```cpp
class CONJURE_API debug_draw_buffer
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">debug_draw_buffer();</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">debug_draw_buffer(
    <span class="k">const</span> <a href="debug-draw-buffer.html">debug_draw_buffer</a> &amp;) = <span class="k">default</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">debug_draw_buffer(
    <a href="debug-draw-buffer.html">debug_draw_buffer</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~debug_draw_buffer();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mDepthTested</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="debug-vertex.html">debug_<wbr>vertex</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mDropped</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mOverlay</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="debug-vertex.html">debug_<wbr>vertex</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `append` {#function-append}

<div class="function-declaration" id="function-append-1"><pre><code><span class="k">void</span> append(
    <span class="k">const</span> <a href="debug-draw-buffer.html">debug_draw_buffer</a> &amp;other);</code></pre></div>

<p>Appends every vertex of another buffer and adds its dropped count to this one&#x27;s.</p>

#### `axes` {#function-axes}

<div class="function-declaration" id="function-axes-1"><pre><code><span class="k">void</span> axes(
    <span class="k">const</span> <a href="../../core/math/glm-types.html#symbol-mat4">math::mat4</a> &amp;worldFromLocal,
    <a href="../../core/types.html#symbol-f32">f32</a> length = <span class="mi">1.0</span>f,
    <a href="../debug-draw.html#symbol-debug-depth">debug_depth</a> depth = debug_depth::TESTED);</code></pre></div>

<p>Three axis lines (red <code>X</code>, green <code>Y</code>, blue <code>Z</code>) of <code>length</code> from the origin of a transform.</p>

#### `box` {#function-box}

<div class="function-declaration" id="function-box-1"><pre><code><span class="k">void</span> box(
    <span class="k">const</span> <a href="../../core/math/aabb/aabb.html">math::aabb</a> &amp;box,
    <span class="k">const</span> <a href="../../core/math/glm-types.html#symbol-vec4">math::vec4</a> &amp;colour,
    <a href="../debug-draw.html#symbol-debug-depth">debug_depth</a> depth = debug_depth::TESTED);</code></pre></div>

<p>The twelve edges of a box (24 vertices). An invalid box adds nothing.</p>

#### `clear` {#function-clear}

<div class="function-declaration" id="function-clear-1"><pre><code><span class="k">void</span> clear() <span class="k">noexcept</span>;</code></pre></div>

<p>Empties both lists and resets the dropped counter; capacity is kept.</p>

#### `depth_tested` {#function-depth-tested}

<div class="function-declaration" id="function-depth-tested-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> <a href="debug-vertex.html">debug_vertex</a>&gt; depth_tested() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `dropped_count` {#function-dropped-count}

<div class="function-declaration" id="function-dropped-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> dropped_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Vertices refused because a list was at <code>MAX_DEBUG_VERTICES</code>.</p>

#### `empty` {#function-empty}

<div class="function-declaration" id="function-empty-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> empty() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `frustum` {#function-frustum}

<div class="function-declaration" id="function-frustum-1"><pre><code><span class="k">void</span> frustum(
    <span class="k">const</span> <a href="../../core/math/glm-types.html#symbol-mat4">math::mat4</a> &amp;clipFromWorld,
    <span class="k">const</span> <a href="../../core/math/glm-types.html#symbol-vec4">math::vec4</a> &amp;colour,
    <a href="../debug-draw.html#symbol-debug-depth">debug_depth</a> depth = debug_depth::TESTED);</code></pre></div>

<p>The twelve edges of the view volume of a clip-from-world matrix (24 vertices).</p>

#### `grid` {#function-grid}

<div class="function-declaration" id="function-grid-1"><pre><code><span class="k">void</span> grid(
    <span class="k">const</span> <a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a> &amp;centre,
    <a href="../../core/types.html#symbol-u32">u32</a> halfLines,
    <a href="../../core/types.html#symbol-f32">f32</a> spacing,
    <span class="k">const</span> <a href="../../core/math/glm-types.html#symbol-vec4">math::vec4</a> &amp;colour,
    <a href="../debug-draw.html#symbol-debug-depth">debug_depth</a> depth = debug_depth::TESTED);</code></pre></div>

<p><code>A</code> grid in the XZ plane centred on <code>centre</code>: (2 * <code>halfLines</code> + 1) lines along each axis, <code>spacing</code> apart.</p>

#### `line` {#function-line}

<div class="function-declaration" id="function-line-1"><pre><code><span class="k">void</span> line(
    <span class="k">const</span> <a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a> &amp;a,
    <span class="k">const</span> <a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a> &amp;b,
    <span class="k">const</span> <a href="../../core/math/glm-types.html#symbol-vec4">math::vec4</a> &amp;colour,
    <a href="../debug-draw.html#symbol-debug-depth">debug_depth</a> depth = debug_depth::TESTED);</code></pre></div>

<p>One line from a to b.</p>

#### `overlay` {#function-overlay}

<div class="function-declaration" id="function-overlay-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> <a href="debug-vertex.html">debug_vertex</a>&gt; overlay() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `sphere` {#function-sphere}

<div class="function-declaration" id="function-sphere-1"><pre><code><span class="k">void</span> sphere(
    <span class="k">const</span> <a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a> &amp;centre,
    <a href="../../core/types.html#symbol-f32">f32</a> radius,
    <span class="k">const</span> <a href="../../core/math/glm-types.html#symbol-vec4">math::vec4</a> &amp;colour,
    <a href="../debug-draw.html#symbol-debug-depth">debug_depth</a> depth = debug_depth::TESTED,
    <a href="../../core/types.html#symbol-u32">u32</a> segments = <span class="mi">24</span>);</code></pre></div>

<p>Three great circles of <code>segments</code> lines each.</p>

### Private

#### `push_line` {#function-push-line}

<div class="function-declaration" id="function-push-line-1"><pre><code><span class="k">void</span> push_line(
    <span class="k">const</span> <a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a> &amp;a,
    <span class="k">const</span> <a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a> &amp;b,
    <a href="../../core/types.html#symbol-u32">u32</a> packed,
    <a href="../debug-draw.html#symbol-debug-depth">debug_depth</a> depth);</code></pre></div>

## Operator Overloads

### Public

#### `operator=` {#operator-3d}

<div class="function-declaration" id="operator-3d-1"><pre><code><a href="debug-draw-buffer.html">debug_draw_buffer</a> &amp;<span class="k">operator</span>=(
    <span class="k">const</span> <a href="debug-draw-buffer.html">debug_draw_buffer</a> &amp;) = <span class="k">default</span>;</code></pre></div>

<div class="function-declaration" id="operator-3d-2"><pre><code><a href="debug-draw-buffer.html">debug_draw_buffer</a> &amp;<span class="k">operator</span>=(
    <a href="debug-draw-buffer.html">debug_draw_buffer</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

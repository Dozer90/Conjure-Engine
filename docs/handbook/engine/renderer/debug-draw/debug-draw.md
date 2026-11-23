---
kind: class
---

[Engine](../../../engine.md) → [Renderer](../../renderer.md) → [debug_draw.h](../debug-draw.md)

# debug_draw
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::renderer` |
| Include File Path | `engine/include/conjure/renderer/debug_draw.h` |
| Source File Path | `engine/src/renderer/debug_draw.cpp` |
| API ↗ | [Rendering](../../../../api/renderer/index.md), [Rendering a frame](../../../../api/renderer/rendering-a-frame.md) |

## Syntax

```cpp
class CONJURE_API debug_draw
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">debug_draw();</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~debug_draw();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mImmediate</code></td><td><code class="signature"><a href="debug-draw-buffer.html">debug_draw_buffer</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mMutex</code></td><td><code class="signature"><span class="k">mutable</span> std::mutex</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mTimed</code></td><td><code class="signature">std::vector&lt;<a href="debug-draw-timed-entry.html">timed_entry</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `append_to` {#function-append-to}

<div class="function-declaration" id="function-append-to-1"><pre><code><span class="k">void</span> append_to(
    <a href="debug-draw-buffer.html">debug_draw_buffer</a> &amp;target);</code></pre></div>

<p>Appends the immediate shapes and every live timed shape to <code>target</code> and clears the immediate list.</p>

#### `axes` {#function-axes}

<div class="function-declaration" id="function-axes-1"><pre><code><span class="k">void</span> axes(
    <span class="k">const</span> <a href="../../core/math/glm-types.html#symbol-mat4">math::mat4</a> &amp;worldFromLocal,
    <a href="../../core/types.html#symbol-f32">f32</a> length = <span class="mi">1.0</span>f,
    <a href="../../core/types.html#symbol-f32">f32</a> durationSeconds = <span class="mi">0.0</span>f,
    <a href="../debug-draw.html#symbol-debug-depth">debug_depth</a> depth = debug_depth::TESTED);</code></pre></div>

<p>Axis lines (<code>debug_draw_buffer::axes</code>) that live this frame (<code>durationSeconds</code> 0) or for <code>durationSeconds</code>.</p>

#### `box` {#function-box}

<div class="function-declaration" id="function-box-1"><pre><code><span class="k">void</span> box(
    <span class="k">const</span> <a href="../../core/math/aabb/aabb.html">math::aabb</a> &amp;box,
    <span class="k">const</span> <a href="../../core/math/glm-types.html#symbol-vec4">math::vec4</a> &amp;colour,
    <a href="../../core/types.html#symbol-f32">f32</a> durationSeconds = <span class="mi">0.0</span>f,
    <a href="../debug-draw.html#symbol-debug-depth">debug_depth</a> depth = debug_depth::TESTED);</code></pre></div>

<p><code>A</code> box (<code>debug_draw_buffer::box</code>) that lives this frame (<code>durationSeconds</code> 0) or for <code>durationSeconds</code>.</p>

#### `clear` {#function-clear}

<div class="function-declaration" id="function-clear-1"><pre><code><span class="k">void</span> clear();</code></pre></div>

<p>Drops every immediate and timed entry.</p>

#### `line` {#function-line}

<div class="function-declaration" id="function-line-1"><pre><code><span class="k">void</span> line(
    <span class="k">const</span> <a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a> &amp;a,
    <span class="k">const</span> <a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a> &amp;b,
    <span class="k">const</span> <a href="../../core/math/glm-types.html#symbol-vec4">math::vec4</a> &amp;colour,
    <a href="../../core/types.html#symbol-f32">f32</a> durationSeconds = <span class="mi">0.0</span>f,
    <a href="../debug-draw.html#symbol-debug-depth">debug_depth</a> depth = debug_depth::TESTED);</code></pre></div>

<p><code>A</code> line that lives this frame (<code>durationSeconds</code> 0) or for <code>durationSeconds</code>.</p>

#### `sphere` {#function-sphere}

<div class="function-declaration" id="function-sphere-1"><pre><code><span class="k">void</span> sphere(
    <span class="k">const</span> <a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a> &amp;centre,
    <a href="../../core/types.html#symbol-f32">f32</a> radius,
    <span class="k">const</span> <a href="../../core/math/glm-types.html#symbol-vec4">math::vec4</a> &amp;colour,
    <a href="../../core/types.html#symbol-f32">f32</a> durationSeconds = <span class="mi">0.0</span>f,
    <a href="../debug-draw.html#symbol-debug-depth">debug_depth</a> depth = debug_depth::TESTED);</code></pre></div>

<p><code>A</code> sphere (<code>debug_draw_buffer::sphere</code>) that lives this frame (<code>durationSeconds</code> 0) or for <code>durationSeconds</code>.</p>

#### `submit` {#function-submit}

<div class="function-declaration" id="function-submit-1"><pre><code><span class="k">void</span> submit(
    <span class="k">const</span> <a href="debug-draw-buffer.html">debug_draw_buffer</a> &amp;shape,
    <a href="../../core/types.html#symbol-f32">f32</a> durationSeconds = <span class="mi">0.0</span>f);</code></pre></div>

<p>Adds a prepared buffer as an immediate (<code>durationSeconds</code> 0) or timed entry.</p>

#### `tick` {#function-tick}

<div class="function-declaration" id="function-tick-1"><pre><code><span class="k">void</span> tick(
    <a href="../../core/types.html#symbol-f32">f32</a> deltaSeconds);</code></pre></div>

<p>Ages the timed entries and drops the expired ones.</p>

#### `timed_entry_count` {#function-timed-entry-count}

<div class="function-declaration" id="function-timed-entry-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-usize">usize</a> timed_entry_count() <span class="k">const</span>;</code></pre></div>

<p>Number of timed entries still alive.</p>

### Private

#### `add_timed` {#function-add-timed}

<div class="function-declaration" id="function-add-timed-1"><pre><code><span class="k">void</span> add_timed(
    <a href="debug-draw-buffer.html">debug_draw_buffer</a> &amp;&amp;shape,
    <a href="../../core/types.html#symbol-f32">f32</a> durationSeconds);</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

---
kind: class
---

[Engine](../../../engine.md) → [Physics](../../physics.md) → [physics_debug_draw.h](../physics-debug-draw.md)

# physics_debug_draw
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::physics` |
| Include File Path | `engine/include/conjure/physics/physics_debug_draw.h` |
| Source File Path | `engine/src/physics/physics_debug_draw.cpp` |
| API ↗ | [Physics](../../../../api/physics/index.md), [The physics world and queries](../../../../api/physics/physics-world-and-queries.md) |

## Syntax

```cpp
class CONJURE_API physics_debug_draw
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">physics_debug_draw();</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">virtual</span> ~physics_debug_draw();</code></td><td></td></tr>
</tbody>
</table>



## Functions

### Public

#### `line` {#function-line}

<div class="function-declaration" id="function-line-1"><pre><code><span class="k">virtual</span> <span class="k">void</span> line(
    <span class="k">const</span> <a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a> &amp;from,
    <span class="k">const</span> <a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a> &amp;to,
    <a href="../../core/types.html#symbol-u32">u32</a> rgba) = <span class="mi">0</span>;</code></pre></div>

<p><code>A</code> world-space line segment.</p>

#### `text` {#function-text}

<div class="function-declaration" id="function-text-1"><pre><code><span class="k">virtual</span> <span class="k">void</span> text(
    <span class="k">const</span> <a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a> &amp;position,
    std::string_view text,
    <a href="../../core/types.html#symbol-u32">u32</a> rgba,
    <a href="../../core/types.html#symbol-f32">f32</a> height);</code></pre></div>

<p><code>A</code> world-space label of the given height in metres. The default implementation draws nothing.</p>

#### `triangle` {#function-triangle}

<div class="function-declaration" id="function-triangle-1"><pre><code><span class="k">virtual</span> <span class="k">void</span> triangle(
    <span class="k">const</span> <a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a> &amp;a,
    <span class="k">const</span> <a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a> &amp;b,
    <span class="k">const</span> <a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a> &amp;c,
    <a href="../../core/types.html#symbol-u32">u32</a> rgba) = <span class="mi">0</span>;</code></pre></div>

<p><code>A</code> world-space filled triangle.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="variables"></a>
<!-- doc-legacy-anchors:end -->

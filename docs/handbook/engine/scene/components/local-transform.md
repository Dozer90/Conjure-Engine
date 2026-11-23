---
kind: struct
---

[Engine](../../../engine.md) → [Scene](../../scene.md) → [components.h](../components.md)

# local_transform
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::scene` |
| Include File Path | `engine/include/conjure/scene/components.h` |
| Source File Path | `engine/src/scene/components.cpp` |
| API ↗ | [Scenes, hierarchy and prefabs](../../../../api/scene/index.md), [Scenes and the hierarchy](../../../../api/scene/scenes-and-hierarchy.md) |

## Syntax

```cpp
struct CONJURE_API local_transform
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">position</code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a></code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a>(<span class="mi">0.0</span>f)</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">rotation</code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-quat">math::quat</a></code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-quat">math::<wbr>quat</a>(<span class="mi">1.0</span>f,<wbr> <span class="mi">0.0</span>f,<wbr> <span class="mi">0.0</span>f,<wbr> <span class="mi">0.0</span>f)</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">scale</code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a></code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a>(<span class="mi">1.0</span>f)</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `from_transform` {#function-from-transform}

<div class="function-declaration" id="function-from-transform-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="local-transform.html">local_transform</a> from_transform(
    <span class="k">const</span> <a href="../../core/math/transform/transform.html">math::transform</a> &amp;value) <span class="k">noexcept</span>;</code></pre></div>

<p><code>A</code> <code><a href="local-transform.html">local_transform</a></code> holding the values of a <code><a href="../../core/math/transform/transform.html">math::transform</a></code>.</p>

#### `to_transform` {#function-to-transform}

<div class="function-declaration" id="function-to-transform-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/math/transform/transform.html">math::transform</a> to_transform() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The same values as a <code><a href="../../core/math/transform/transform.html">math::transform</a></code>.</p>

## Operator Overloads

### Public

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="local-transform.html">local_transform</a> &amp;) <span class="k">const</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->

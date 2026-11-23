---
kind: struct
---

[Engine](../../../engine.md) → [Physics](../../physics.md) → [components.h](../components.md)

# rigid_body
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::physics` |
| Include File Path | `engine/include/conjure/physics/components.h` |
| Source File Path | N/A |
| API ↗ | [Bodies, colliders and characters](../../../../api/physics/bodies-and-colliders.md), [Physics](../../../../api/physics/index.md) |

## Syntax

```cpp
struct rigid_body
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">angular<wbr>Damping</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">0.05</span>f</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">angular<wbr>Velocity</code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a></code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a>(<span class="mi">0.0</span>f)</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">flags</code></td><td><code class="signature"><a href="../components.html#symbol-rigid-body-flags">rigid_body_flags</a></code></td><td><code class="signature">rigid_<wbr>body_<wbr>flags::<wbr>ALLOW_<wbr>SLEEPING</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">gravity<wbr>Factor</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">1.0</span>f</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">isSleeping</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">layer</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u8">u8</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">linear<wbr>Damping</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">0.05</span>f</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">linear<wbr>Velocity</code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a></code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a>(<span class="mi">0.0</span>f)</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">mass</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">1.0</span>f</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">max<wbr>Linear<wbr>Velocity</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">500.0</span>f</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">motionType</code></td><td><code class="signature"><a href="../components.html#symbol-motion-type">motion_type</a></code></td><td><code class="signature">motion_<wbr>type::<wbr>DYNAMIC</code></td><td></td></tr>
</tbody>
</table>



## Operator Overloads

### Public

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="rigid-body.html">rigid_body</a> &amp;) <span class="k">const</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->

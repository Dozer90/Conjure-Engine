---
kind: struct
---

[Engine](../../../engine.md) → [Physics](../../physics.md) → [character_controller.h](../character-controller.md)

# character_controller
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::physics` |
| Include File Path | `engine/include/conjure/physics/character_controller.h` |
| Source File Path | N/A |
| API ↗ | [Bodies, colliders and characters](../../../../api/physics/bodies-and-colliders.md), [Physics](../../../../api/physics/index.md) |

## Syntax

```cpp
struct character_controller
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">flags</code></td><td><code class="signature"><a href="../character-controller.html#symbol-character-flags">character_flags</a></code></td><td><code class="signature">character_<wbr>flags::<wbr>STICK_<wbr>TO_<wbr>FLOOR | character_<wbr>flags::<wbr>WALK_<wbr>STAIRS | character_<wbr>flags::<wbr>APPLY_<wbr>GRAVITY</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">ground</code></td><td><code class="signature"><a href="../character-controller.html#symbol-ground-state">ground_state</a></code></td><td><code class="signature">ground_<wbr>state::<wbr>IN_<wbr>AIR</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">groundEntity</code></td><td><code class="signature"><a href="../../ecs/entity/entity.html">ecs::entity</a></code></td><td><code class="signature">ecs::NULL_ENTITY</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">groundNormal</code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a></code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-vec3">math::<wbr>vec3</a>(<span class="mi">0.0</span>f,<wbr> <span class="mi">1.0</span>f,<wbr> <span class="mi">0.0</span>f)</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">height</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">1.8</span>f</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">layer</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u8">u8</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">mass</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">70.0</span>f</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">max<wbr>Slope<wbr>Angle<wbr>Radians</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f32">f32</a></code></td><td><code class="signature">std::<wbr>numbers::<wbr>pi_<wbr>v&lt;<wbr><a href="../../core/types.html#symbol-f32">f32</a>&gt; / <span class="mi">4.0</span>f</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">maxStrength</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">100.0</span>f</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">padding</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">0.02</span>f</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">radius</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">0.3</span>f</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">stepHeight</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">0.4</span>f</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">velocity</code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a></code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a>(<span class="mi">0.0</span>f)</code></td><td></td></tr>
</tbody>
</table>



## Operator Overloads

### Public

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="character-controller.html">character_controller</a> &amp;) <span class="k">const</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->

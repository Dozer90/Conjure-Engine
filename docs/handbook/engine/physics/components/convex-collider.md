---
kind: struct
---

[Engine](../../../engine.md) → [Physics](../../physics.md) → [components.h](../components.md)

# convex_collider
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::physics` |
| Include File Path | `engine/include/conjure/physics/components.h` |
| Source File Path | N/A |
| API ↗ | [Bodies, colliders and characters](../../../../api/physics/bodies-and-colliders.md), [Physics](../../../../api/physics/index.md) |

## Syntax

```cpp
struct convex_collider
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">convexRadius</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">0.05</span>f</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">isTrigger</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">mesh</code></td><td><code class="signature"><a href="../../assets/asset-ref/asset-ref.html">assets::<wbr>asset_<wbr>ref</a>&lt;<wbr><a href="../collision-mesh-asset/collision-mesh-asset.html">collision_<wbr>mesh_<wbr>asset</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">offset</code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a></code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a>(<span class="mi">0.0</span>f)</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">rotation</code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-quat">math::quat</a></code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-quat">math::<wbr>quat</a>(<span class="mi">1.0</span>f,<wbr> <span class="mi">0.0</span>f,<wbr> <span class="mi">0.0</span>f,<wbr> <span class="mi">0.0</span>f)</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->

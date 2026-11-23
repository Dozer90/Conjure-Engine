---
kind: struct
---

[Engine](../../../engine.md) → [Scripting](../../scripting.md) → [script_runtime.h](../script-runtime.md)

# collision_event
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::scripting` |
| Include File Path | `engine/include/conjure/scripting/script_runtime.h` |
| Source File Path | `engine/src/scripting/script_runtime.cpp` |
| API ↗ | [Scripting](../../../../api/scripting/index.md), [Scripts, components and the runtime](../../../../api/scripting/scripts-and-components.md) |

## Syntax

```cpp
struct collision_event
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">a</code></td><td><code class="signature"><a href="../../ecs/entity/entity.html">ecs::entity</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">b</code></td><td><code class="signature"><a href="../../ecs/entity/entity.html">ecs::entity</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">impulse</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">0.0</span>f</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">kind</code></td><td><code class="signature"><a href="../native-api.html#symbol-collision-kind">collision_kind</a></code></td><td><code class="signature">collision_<wbr>kind::<wbr>COLLISION_<wbr>ENTER</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">normal</code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a></code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a>(<span class="mi">0.0</span>f)</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">point</code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a></code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a>(<span class="mi">0.0</span>f)</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->

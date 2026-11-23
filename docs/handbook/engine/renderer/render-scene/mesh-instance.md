---
kind: struct
---

[Engine](../../../engine.md) → [Renderer](../../renderer.md) → [render_scene.h](../render-scene.md)

# mesh_instance
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::renderer` |
| Include File Path | `engine/include/conjure/renderer/render_scene.h` |
| Source File Path | `engine/src/renderer/render_scene.cpp` |
| API ↗ | [Rendering](../../../../api/renderer/index.md), [Rendering a frame](../../../../api/renderer/rendering-a-frame.md) |

## Syntax

```cpp
struct mesh_instance
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">castsShadows</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">true</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">entityId</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">layerMask</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">1</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">materials</code></td><td><code class="signature">std::<wbr>array&lt;<wbr>gpu_<wbr>material_<wbr>id,<wbr> MAX_<wbr>MATERIAL_<wbr>SLOTS&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">mesh</code></td><td><code class="signature"><a href="../gpu-ids.html#symbol-gpu-mesh-id">gpu_mesh_id</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">receives<wbr>Shadows</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">true</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">worldBounds</code></td><td><code class="signature"><a href="../../core/math/aabb/aabb.html">math::aabb</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">world<wbr>From<wbr>Local</code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-mat4">math::mat4</a></code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-mat4">math::mat4</a>(<span class="mi">1.0</span>f)</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->

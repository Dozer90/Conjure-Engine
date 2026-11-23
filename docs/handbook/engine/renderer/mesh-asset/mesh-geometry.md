---
kind: struct
---

[Engine](../../../engine.md) → [Renderer](../../renderer.md) → [mesh_asset.h](../mesh-asset.md)

# mesh_geometry
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::renderer` |
| Include File Path | `engine/include/conjure/renderer/mesh_asset.h` |
| Source File Path | `engine/src/renderer/assets/mesh_asset.cpp` |
| API ↗ | [Rendering](../../../../api/renderer/index.md), [Renderer assets](../../../../api/renderer/renderer-assets.md) |

## Syntax

```cpp
struct mesh_geometry
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">colours</code></td><td><code class="signature">std::vector&lt;<a href="../../core/math/glm-types.html#symbol-vec4">math::vec4</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">indices</code></td><td><code class="signature">std::vector&lt;<a href="../../core/types.html#symbol-u32">u32</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">normals</code></td><td><code class="signature">std::vector&lt;<a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">positions</code></td><td><code class="signature">std::vector&lt;<a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">submesh<wbr>Index<wbr>Counts</code></td><td><code class="signature">std::vector&lt;<a href="../../core/types.html#symbol-u32">u32</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">submesh<wbr>Material<wbr>Slots</code></td><td><code class="signature">std::vector&lt;<a href="../../core/types.html#symbol-u32">u32</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">tangents</code></td><td><code class="signature">std::vector&lt;<a href="../../core/math/glm-types.html#symbol-vec4">math::vec4</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">uv0</code></td><td><code class="signature">std::vector&lt;<a href="../../core/math/glm-types.html#symbol-vec2">math::vec2</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">uv1</code></td><td><code class="signature">std::vector&lt;<a href="../../core/math/glm-types.html#symbol-vec2">math::vec2</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->

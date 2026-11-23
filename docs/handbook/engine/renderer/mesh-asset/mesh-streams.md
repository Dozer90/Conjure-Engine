---
kind: struct
---

[Engine](../../../engine.md) → [Renderer](../../renderer.md) → [mesh_asset.h](../mesh-asset.md)

# mesh_streams
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::renderer` |
| Include File Path | `engine/include/conjure/renderer/mesh_asset.h` |
| Source File Path | `engine/src/renderer/assets/mesh_asset.cpp` |
| API ↗ | [Rendering](../../../../api/renderer/index.md), [Renderer assets](../../../../api/renderer/renderer-assets.md) |

## Syntax

```cpp
struct mesh_streams
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">attributes</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="packed-vertex-attributes.html">packed_<wbr>vertex_<wbr>attributes</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">indices</code></td><td><code class="signature">std::vector&lt;<a href="../../core/types.html#symbol-u32">u32</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">positions</code></td><td><code class="signature">std::vector&lt;<a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->

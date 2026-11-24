---
kind: struct
---

[Engine](../../../engine.md) → [Renderer](../../renderer.md) → [render_target.h](../render-target.md)

# depth_read_back
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::renderer` |
| Include File Path | `engine/include/conjure/renderer/render_target.h` |
| Source File Path | `engine/src/renderer/render_target.cpp` |
| API ↗ | [Rendering](../../../../api/renderer/index.md), [Rendering a frame](../../../../api/renderer/rendering-a-frame.md) |

## Syntax

```cpp
struct depth_read_back
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">depth</code></td><td><code class="signature">std::vector&lt;<a href="../../core/types.html#symbol-f32">f32</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">size</code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-uvec2">math::uvec2</a></code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-uvec2">math::<wbr>uvec2</a>(<span class="mi">0</span>,<wbr> <span class="mi">0</span>)</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

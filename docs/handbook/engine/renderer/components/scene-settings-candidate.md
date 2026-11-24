---
kind: struct
---

[Engine](../../../engine.md) → [Renderer](../../renderer.md) → [components.h](../components.md)

# scene_settings_candidate
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::renderer` |
| Include File Path | `engine/include/conjure/renderer/components.h` |
| Source File Path | `engine/src/renderer/components.cpp` |
| API ↗ | [Rendering](../../../../api/renderer/index.md), [Rendering a frame](../../../../api/renderer/rendering-a-frame.md) |

## Syntax

```cpp
struct scene_settings_candidate
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">component</code></td><td><code class="signature"><span class="k">const</span> <a href="render-settings-component.html">render_<wbr>settings_<wbr>component</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">entityIndex</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">guid</code></td><td><code class="signature"><a href="../../core/uuid/uuid.html">core::uuid</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

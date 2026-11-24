---
kind: struct
---

[Engine](../../../engine.md) → [Renderer](../../renderer.md) → [render_settings.h](../render-settings.md)

# render_settings_reading
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::renderer` |
| Include File Path | `engine/include/conjure/renderer/render_settings.h` |
| Source File Path | `engine/src/renderer/render_settings.cpp` |
| API ↗ | [Rendering](../../../../api/renderer/index.md), [Rendering a frame](../../../../api/renderer/rendering-a-frame.md) |

## Syntax

```cpp
struct render_settings_reading
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">errors</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="render-settings-problem.html">render_<wbr>settings_<wbr>problem</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">settings</code></td><td><code class="signature"><a href="render-settings.html">render_settings</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">unknownKeys</code></td><td><code class="signature">std::vector&lt;std::string&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

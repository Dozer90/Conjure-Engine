---
kind: struct
---

[Engine](../../../engine.md) → [Renderer](../../renderer.md) → [components.h](../components.md)

# resolved_render_settings
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::renderer` |
| Include File Path | `engine/include/conjure/renderer/components.h` |
| Source File Path | `engine/src/renderer/components.cpp` |
| API ↗ | [Rendering](../../../../api/renderer/index.md), [Rendering a frame](../../../../api/renderer/rendering-a-frame.md) |

## Syntax

```cpp
struct resolved_render_settings
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">ambiguous</code></td><td><code class="signature">std::<wbr>array&lt;<wbr><span class="k">bool</span>,<wbr> SETTINGS_<wbr>GROUP_<wbr>COUNT&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">post<wbr>Process<wbr>Disabled</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">sceneWinner</code></td><td><code class="signature">std::<wbr>array&lt;<wbr>u32,<wbr> SETTINGS_<wbr>GROUP_<wbr>COUNT&gt;</code></td><td><code class="signature">{ NO_<wbr>SETTINGS_<wbr>CANDIDATE,<wbr> NO_<wbr>SETTINGS_<wbr>CANDIDATE,<wbr> NO_<wbr>SETTINGS_<wbr>CANDIDATE }</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">settings</code></td><td><code class="signature"><a href="../render-settings/render-settings.html">render_settings</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">source</code></td><td><code class="signature">std::<wbr>array&lt;<wbr>render_<wbr>settings_<wbr>source,<wbr> SETTINGS_<wbr>GROUP_<wbr>COUNT&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

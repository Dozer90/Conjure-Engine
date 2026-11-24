---
kind: struct
---

[Engine](../../../engine.md) → [Renderer](../../renderer.md) → [render_scene.h](../render-scene.md)

# view_render_settings
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::renderer` |
| Include File Path | `engine/include/conjure/renderer/render_scene.h` |
| Source File Path | `engine/src/renderer/render_scene.cpp` |
| API ↗ | [Rendering](../../../../api/renderer/index.md), [Rendering a frame](../../../../api/renderer/rendering-a-frame.md) |

## Syntax

```cpp
struct view_render_settings
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">postProcess</code></td><td><code class="signature"><a href="../render-settings/post-process-settings.html">post_process_settings</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">ssao</code></td><td><code class="signature"><a href="../render-settings/ssao-settings.html">ssao_settings</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>



## Operator Overloads

### Public

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="view-render-settings.html">view_render_settings</a> &amp;other) <span class="k">const</span> <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

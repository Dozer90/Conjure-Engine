---
kind: struct
---

[Engine](../../../engine.md) → [Renderer](../../renderer.md) → [components.h](../components.md)

# post_process_component
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::renderer` |
| Include File Path | `engine/include/conjure/renderer/components.h` |
| Source File Path | `engine/src/renderer/components.cpp` |
| API ↗ | [Rendering](../../../../api/renderer/index.md), [Rendering a frame](../../../../api/renderer/rendering-a-frame.md) |

## Syntax

```cpp
struct post_process_component
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">override<wbr>Post<wbr>Process</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">overrideSsao</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">postProcess</code></td><td><code class="signature"><a href="../render-settings/post-process-settings.html">post_process_settings</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">ssao</code></td><td><code class="signature"><a href="../render-settings/ssao-settings.html">ssao_settings</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>



## Operator Overloads

### Public

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="post-process-component.html">post_process_component</a> &amp;other) <span class="k">const</span> <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

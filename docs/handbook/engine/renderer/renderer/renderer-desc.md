---
kind: struct
---

[Engine](../../../engine.md) → [Renderer](../../renderer.md) → [renderer.h](../renderer.md)

# renderer_desc
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::renderer` |
| Include File Path | `engine/include/conjure/renderer/renderer.h` |
| Source File Path | `engine/src/renderer/renderer.cpp` |
| API ↗ | [Rendering](../../../../api/renderer/index.md), [Renderer assets](../../../../api/renderer/renderer-assets.md), [Rendering a frame](../../../../api/renderer/rendering-a-frame.md) |

## Syntax

```cpp
struct renderer_desc
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">atlas<wbr>Resolution</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">4096</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">cascade<wbr>Resolution</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">2048</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">device</code></td><td><code class="signature"><a href="../../rhi/device/device.html">rhi::device</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">enableImGui</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">true</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">jobs</code></td><td><code class="signature"><a href="../../core/jobs/job-system/jobs-job-system.html">core::jobs::job_system</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">shaders</code></td><td><code class="signature"><a href="../../rhi/shader-library/shader-library.html">rhi::shader_library</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">swapchain</code></td><td><code class="signature"><a href="../../rhi/swapchain/swapchain.html">rhi::swapchain</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->

---
kind: struct
---

[Engine](../../../engine.md) → [RHI](../../rhi.md) → [swapchain.h](../swapchain.md)

# swapchain_desc
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::rhi` |
| Include File Path | `engine/include/conjure/rhi/swapchain.h` |
| Source File Path | `engine/src/rhi/swapchain.cpp` |
| API ↗ | [Devices and swapchains](../../../../api/rhi/devices-and-swapchains.md) |

## Syntax

```cpp
struct swapchain_desc
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">bufferCount</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">3</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">format</code></td><td><code class="signature"><a href="../texture-format.html#symbol-texture-format">texture_format</a></code></td><td><code class="signature">texture_<wbr>format::<wbr>RGBA8_<wbr>SRGB</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">height</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">vsync</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">true</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">width</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->

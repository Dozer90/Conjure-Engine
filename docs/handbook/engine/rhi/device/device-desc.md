---
kind: struct
---

[Engine](../../../engine.md) → [RHI](../../rhi.md) → [device.h](../device.md)

# device_desc
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::rhi` |
| Include File Path | `engine/include/conjure/rhi/device.h` |
| Source File Path | `engine/src/rhi/device.cpp` |
| API ↗ | [Devices and swapchains](../../../../api/rhi/devices-and-swapchains.md), [Shaders](../../../../api/rhi/shaders.md) |

## Syntax

```cpp
struct device_desc
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">adapterIndex</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">backend</code></td><td><code class="signature"><a href="../backend.html#symbol-graphics-backend">graphics_backend</a></code></td><td><code class="signature">graphics_<wbr>backend::<wbr>NULL_<wbr>DEVICE</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">enable<wbr>Debug<wbr>Layer</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">enable<wbr>Gpu<wbr>Validation</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">prefer<wbr>Software<wbr>Adapter</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">window</code></td><td><code class="signature"><a href="../native-window/native-window.html">native_window</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->

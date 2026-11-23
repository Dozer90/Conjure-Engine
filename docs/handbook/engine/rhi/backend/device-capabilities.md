---
kind: struct
---

[Engine](../../../engine.md) → [RHI](../../rhi.md) → [backend.h](../backend.md)

# device_capabilities
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::rhi` |
| Include File Path | `engine/include/conjure/rhi/backend.h` |
| Source File Path | `engine/src/rhi/backend.cpp` |
| API ↗ | [Devices and swapchains](../../../../api/rhi/devices-and-swapchains.md) |

## Syntax

```cpp
struct device_capabilities
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">adapterName</code></td><td><code class="signature">std::string</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">backend</code></td><td><code class="signature"><a href="../backend.html#symbol-graphics-backend">graphics_backend</a></code></td><td><code class="signature">graphics_<wbr>backend::<wbr>NULL_<wbr>DEVICE</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">computeQueue</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">dedicated<wbr>Video<wbr>Memory<wbr>Bytes</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">max<wbr>Texture<wbr>Dimension2d</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">16384</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">software<wbr>Adapter</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">timerQueries</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->

---
kind: struct
---

[Engine](../../../engine.md) → [Scripting](../../scripting.md) → [native_api.h](../native-api.md)

# conjure_frame_stats_block
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::scripting` |
| Include File Path | `engine/include/conjure/scripting/native_api.h` |
| Source File Path | `engine/src/scripting/native_api.cpp` |
| API ↗ | [Hosting, the native API and hot reload](../../../../api/scripting/hosting-and-hot-reload.md), [Scripting](../../../../api/scripting/index.md) |

## Syntax

```cpp
struct conjure_frame_stats_block
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">fixed_steps</code></td><td><code class="signature">uint32_t</code></td><td>—</td><td></td></tr>
<tr><td>public</td><td><code class="signature">frame_<wbr>memory_<wbr>high_<wbr>water</code></td><td><code class="signature">uint64_t</code></td><td>—</td><td></td></tr>
<tr><td>public</td><td><code class="signature">frame_ms</code></td><td><code class="signature"><span class="k">float</span></code></td><td>—</td><td></td></tr>
<tr><td>public</td><td><code class="signature">jobs_<wbr>executed</code></td><td><code class="signature">uint32_t</code></td><td>—</td><td></td></tr>
<tr><td>public</td><td><code class="signature">managed_<wbr>allocated_<wbr>bytes</code></td><td><code class="signature">uint64_t</code></td><td>—</td><td></td></tr>
<tr><td>public</td><td><code class="signature">managed_<wbr>collections</code></td><td><code class="signature">uint32_t</code></td><td>—</td><td></td></tr>
<tr><td>public</td><td><code class="signature">managed_<wbr>full_<wbr>collections</code></td><td><code class="signature">uint32_t</code></td><td>—</td><td></td></tr>
<tr><td>public</td><td><code class="signature">managed_<wbr>heap_<wbr>bytes</code></td><td><code class="signature">uint64_t</code></td><td>—</td><td></td></tr>
<tr><td>public</td><td><code class="signature">phase_ms</code></td><td><code class="signature"><span class="k">float</span>[<span class="na">10</span>]</code></td><td>—</td><td></td></tr>
<tr><td>public</td><td><code class="signature">submit_ms</code></td><td><code class="signature"><span class="k">float</span></code></td><td>—</td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->

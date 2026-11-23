---
kind: struct
---

[Engine](../../../engine.md) → [Scripting](../../scripting.md) → [native_api.h](../native-api.md)

# conjure_native_api
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::scripting` |
| Include File Path | `engine/include/conjure/scripting/native_api.h` |
| Source File Path | `engine/src/scripting/native_api.cpp` |
| API ↗ | [Hosting, the native API and hot reload](../../../../api/scripting/hosting-and-hot-reload.md), [Scripting](../../../../api/scripting/index.md) |

## Syntax

```cpp
struct conjure_native_api
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">frame</code></td><td><code class="signature"><span class="k">const</span> <a href="conjure-frame-block.html">conjure_<wbr>frame_<wbr>block</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">layout_hash</code></td><td><code class="signature">uint64_t</code></td><td><code class="signature"><span class="na">CONJURE_<wbr>NATIVE_<wbr>API_<wbr>HASH</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">size</code></td><td><code class="signature">uint32_t</code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">version</code></td><td><code class="signature">uint32_t</code></td><td><code class="signature"><span class="na">CONJURE_<wbr>NATIVE_<wbr>API_<wbr>VERSION</span></code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->

---
kind: struct
---

[Engine](../../../engine.md) → [Scripting](../../scripting.md) → [native_api.h](../native-api.md)

# conjure_managed_api
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::scripting` |
| Include File Path | `engine/include/conjure/scripting/native_api.h` |
| Source File Path | `engine/src/scripting/native_api.cpp` |
| API ↗ | [Hosting, the native API and hot reload](../../../../api/scripting/hosting-and-hot-reload.md), [Scripting](../../../../api/scripting/index.md) |

## Syntax

```cpp
struct conjure_managed_api
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">apply_fields</code></td><td><code class="signature">int32_<wbr>t (*)(int32_<wbr>t slot,<wbr> <span class="k">const</span> uint8_<wbr>t *blob,<wbr> int32_<wbr>t size)</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">collect_<wbr>fields</code></td><td><code class="signature">int32_<wbr>t (*)(int32_<wbr>t slot,<wbr> uint8_<wbr>t *buffer,<wbr> int32_<wbr>t capacity)</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">collect_<wbr>garbage</code></td><td><code class="signature">int32_t (*)(<span class="k">void</span>)</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">create_<wbr>instances</code></td><td><code class="signature">int32_<wbr>t (*)(<a href="conjure-create-request.html">conjure_<wbr>create_<wbr>request</a> *requests,<wbr> int32_<wbr>t count)</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">destroy_<wbr>instances</code></td><td><code class="signature">int32_<wbr>t (*)(<span class="k">const</span> int32_<wbr>t *slots,<wbr> int32_<wbr>t count,<wbr> int32_<wbr>t call<wbr>On<wbr>Destroy)</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">dispatch_<wbr>collisions</code></td><td><code class="signature">int32_<wbr>t (*)(<span class="k">const</span> <a href="conjure-collision-record.html">conjure_<wbr>collision_<wbr>record</a> *records,<wbr> int32_<wbr>t count)</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">layout_hash</code></td><td><code class="signature">uint64_t</code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">load_<wbr>game_<wbr>assembly</code></td><td><code class="signature">int32_<wbr>t (*)(<span class="k">const</span> <span class="k">char</span> *utf8<wbr>Path,<wbr> int32_<wbr>t length)</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">run_phase</code></td><td><code class="signature">int32_<wbr>t (*)(uint8_<wbr>t phase,<wbr> <span class="k">float</span> delta<wbr>Time)</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">sample_<wbr>memory</code></td><td><code class="signature">int32_<wbr>t (*)(<a href="conjure-managed-memory.html">conjure_<wbr>managed_<wbr>memory</a> *out)</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">set_active</code></td><td><code class="signature">int32_<wbr>t (*)(<span class="k">const</span> int32_<wbr>t *slots,<wbr> int32_<wbr>t count,<wbr> int32_<wbr>t active)</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">shutdown</code></td><td><code class="signature">int32_t (*)(<span class="k">void</span>)</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">size</code></td><td><code class="signature">uint32_t</code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">unload_<wbr>game_<wbr>assembly</code></td><td><code class="signature">int32_t (*)(<span class="k">void</span>)</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">version</code></td><td><code class="signature">uint32_t</code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->

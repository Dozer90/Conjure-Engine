---
kind: struct
---

[Engine](../../../engine.md) → [Scripting](../../scripting.md) → [native_api.h](../native-api.md)

# conjure_shape_query
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::scripting` |
| Include File Path | `engine/include/conjure/scripting/native_api.h` |
| Source File Path | `engine/src/scripting/native_api.cpp` |
| API ↗ | [Hosting, the native API and hot reload](../../../../api/scripting/hosting-and-hot-reload.md), [Scripting](../../../../api/scripting/index.md) |

## Syntax

```cpp
struct conjure_shape_query
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">half_extents</code></td><td><code class="signature"><a href="conjure-vec3.html">conjure_vec3</a></code></td><td>—</td><td></td></tr>
<tr><td>public</td><td><code class="signature">pad</code></td><td><code class="signature">uint8_t[<span class="na">3</span>]</code></td><td>—</td><td></td></tr>
<tr><td>public</td><td><code class="signature">radius</code></td><td><code class="signature"><span class="k">float</span></code></td><td>—</td><td></td></tr>
<tr><td>public</td><td><code class="signature">ray</code></td><td><code class="signature"><a href="conjure-ray-query.html">conjure_ray_query</a></code></td><td>—</td><td></td></tr>
<tr><td>public</td><td><code class="signature">rotation</code></td><td><code class="signature"><a href="conjure-quat-xyzw.html">conjure_quat_xyzw</a></code></td><td>—</td><td></td></tr>
<tr><td>public</td><td><code class="signature">shape</code></td><td><code class="signature">uint8_t</code></td><td>—</td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->

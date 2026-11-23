---
kind: struct
---

[Engine](../../../engine.md) → [Scripting](../../scripting.md) → [null_script_host.h](../null-script-host.md)

# null_host_recorder::collision_record
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::scripting` |
| Include File Path | `engine/include/conjure/scripting/null_script_host.h` |
| Source File Path | `engine/src/scripting/null_script_host.cpp` |
| API ↗ | [Hosting, the native API and hot reload](../../../../api/scripting/hosting-and-hot-reload.md), [Scripting](../../../../api/scripting/index.md) |

## Syntax

```cpp
struct collision_record
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">impulse</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">0.0</span>f</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">kind</code></td><td><code class="signature"><a href="../native-api.html#symbol-collision-kind">collision_kind</a></code></td><td><code class="signature">collision_<wbr>kind::<wbr>COUNT</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">other</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">slot</code></td><td><code class="signature"><a href="../../core/types.html#symbol-i32">i32</a></code></td><td><code class="signature">-<span class="mi">1</span></code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->

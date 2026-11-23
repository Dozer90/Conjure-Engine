---
kind: struct
---

[Engine](../../../engine.md) → [Scripting](../../scripting.md) → [script_runtime.h](../script-runtime.md)

# script_runtime::faulted_slot
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::scripting` |
| Include File Path | `engine/include/conjure/scripting/script_runtime.h` |
| Source File Path | `engine/src/scripting/script_runtime.cpp` |
| API ↗ | [Scripting](../../../../api/scripting/index.md), [Scripts, components and the runtime](../../../../api/scripting/scripts-and-components.md) |

## Syntax

```cpp
struct faulted_slot
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">callback</code></td><td><code class="signature"><a href="../native-api.html#symbol-script-callback">script_callback</a></code></td><td><code class="signature">script_<wbr>callback::<wbr>COUNT</code></td><td></td></tr>
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

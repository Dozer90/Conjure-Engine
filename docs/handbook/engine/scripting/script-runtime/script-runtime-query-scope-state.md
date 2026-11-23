---
kind: struct
---

[Engine](../../../engine.md) → [Scripting](../../scripting.md) → [script_runtime.h](../script-runtime.md)

# script_runtime::query_scope_state
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::scripting` |
| Include File Path | `engine/include/conjure/scripting/script_runtime.h` |
| Source File Path | `engine/src/scripting/script_runtime.cpp` |
| API ↗ | [Scripting](../../../../api/scripting/index.md), [Scripts, components and the runtime](../../../../api/scripting/scripts-and-components.md) |

## Syntax

```cpp
struct query_scope_state
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">chunks</code></td><td><code class="signature"><a href="../../core/containers/small-vector/small-vector.html">core::<wbr>small_<wbr>vector</a>&lt;<wbr>conjure_<wbr>chunk_<wbr>view,<wbr> QUERY_<wbr>SCOPE_<wbr>INLINE_<wbr>CHUNKS&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">id</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">lock</code></td><td><code class="signature"><a href="../../ecs/world/iteration-scope.html">ecs::iteration_scope</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">types</code></td><td><code class="signature"><a href="../../core/containers/small-vector/small-vector.html">core::<wbr>small_<wbr>vector</a>&lt;<wbr>ecs::<wbr>component_<wbr>id,<wbr> static_<wbr>cast&lt;<wbr><a href="../../core/types.html#symbol-usize">usize</a>&gt;(QUERY_<wbr>MAX_<wbr>COMPONENTS)&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->

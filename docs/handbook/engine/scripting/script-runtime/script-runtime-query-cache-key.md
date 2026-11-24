---
kind: struct
---

[Engine](../../../engine.md) → [Scripting](../../scripting.md) → [script_runtime.h](../script-runtime.md)

# script_runtime::query_cache_key
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::scripting` |
| Include File Path | `engine/include/conjure/scripting/script_runtime.h` |
| Source File Path | `engine/src/scripting/script_runtime.cpp` |
| API ↗ | [Scripting](../../../../api/scripting/index.md), [Scripts, components and the runtime](../../../../api/scripting/scripts-and-components.md) |

## Syntax

```cpp
struct query_cache_key
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">with</code></td><td><code class="signature">std::<wbr>array&lt;<wbr>u16,<wbr> static_<wbr>cast&lt;<wbr><a href="../../core/types.html#symbol-usize">usize</a>&gt;(QUERY_<wbr>MAX_<wbr>COMPONENTS)&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">without</code></td><td><code class="signature">std::<wbr>array&lt;<wbr>u16,<wbr> static_<wbr>cast&lt;<wbr><a href="../../core/types.html#symbol-usize">usize</a>&gt;(QUERY_<wbr>MAX_<wbr>COMPONENTS)&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>



## Operator Overloads

### Public

#### `operator<=>` {#operator-3c--3d--3e}

<div class="function-declaration" id="operator-3c--3d--3e-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">auto</span> <span class="k">operator</span>&lt;=&gt;(
    <span class="k">const</span> <a href="script-runtime-query-cache-key.html">query_cache_key</a> &amp;) <span class="k">const</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

---
kind: struct
---

[Engine](../../../engine.md) → [Scripting](../../scripting.md) → [script_runtime.h](../script-runtime.md)

# script_runtime_config
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::scripting` |
| Include File Path | `engine/include/conjure/scripting/script_runtime.h` |
| Source File Path | `engine/src/scripting/script_runtime.cpp` |
| API ↗ | [Scripting](../../../../api/scripting/index.md), [Scripts, components and the runtime](../../../../api/scripting/scripts-and-components.md) |

## Syntax

```cpp
struct script_runtime_config
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">game<wbr>Assembly<wbr>Path</code></td><td><code class="signature">std::filesystem::path</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">host</code></td><td><code class="signature"><a href="../script-host/script-host-config.html">script_host_config</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">initialMode</code></td><td><code class="signature"><a href="../script-runtime.html#symbol-script-mode">script_mode</a></code></td><td><code class="signature">script_<wbr>mode::<wbr>EDIT</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->

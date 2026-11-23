---
kind: struct
---

[Engine](../../../engine.md) → [Bindings](../../bindings.md) → [call_counters.h](../call-counters.md)

# native_entry_descriptor
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::bindings` |
| Include File Path | `engine/include/conjure/bindings/call_counters.h` |
| Source File Path | `engine/src/bindings/call_counters.cpp` |
| API ↗ | [The C# API](../../../../api/bindings/csharp-api.md), [Scripting from C#: the bindings](../../../../api/bindings/index.md), [The native table](../../../../api/bindings/native-table.md) |

## Syntax

```cpp
struct native_entry_descriptor
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">group</code></td><td><code class="signature">std::string_view</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">id</code></td><td><code class="signature"><a href="../call-counters.html#symbol-native-function-id">native_function_id</a></code></td><td><code class="signature">native_<wbr>function_<wbr>id::<wbr>COUNT</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">name</code></td><td><code class="signature">std::string_view</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">owned<wbr>By<wbr>Bindings</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">signature</code></td><td><code class="signature">std::string_view</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->

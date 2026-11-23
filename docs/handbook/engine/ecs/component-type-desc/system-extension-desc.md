---
kind: struct
---

[Engine](../../../engine.md) → [ECS](../../ecs.md) → [component_type_desc.h](../component-type-desc.md)

# system_extension_desc
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::ecs` |
| Include File Path | `engine/include/conjure/ecs/component_type_desc.h` |
| Source File Path | N/A |
| API ↗ | [Entities and components](../../../../api/ecs/entities-and-components.md) |

## Syntax

```cpp
struct system_extension_desc
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">after</code></td><td><code class="signature"><span class="k">const</span> <a href="../../core/string-id/string-id.html">core::string_id</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">afterCount</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">before</code></td><td><code class="signature"><span class="k">const</span> <a href="../../core/string-id/string-id.html">core::string_id</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">beforeCount</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">EXTENSION_<wbr>POINT</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../core/string-id/string-id.html">core::<wbr>string_<wbr>id</a></code></td><td><code class="signature">{<span class="s">"ecs.system"</span>}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">flags</code></td><td><code class="signature"><a href="../system.html#symbol-system-flags">system_flags</a></code></td><td><code class="signature">system_<wbr>flags::<wbr>NONE</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">name</code></td><td><code class="signature"><span class="k">const</span> <span class="k">char</span> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">phase</code></td><td><code class="signature"><a href="../system-phase.html#symbol-system-phase">system_phase</a></code></td><td><code class="signature">system_<wbr>phase::<wbr>UPDATE</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">readCount</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">reads</code></td><td><code class="signature"><span class="k">const</span> <a href="../../core/string-id/string-id.html">core::string_id</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">run</code></td><td><code class="signature"><span class="k">void</span> (*)(<a href="../system/system-context.html">system_<wbr>context</a> &amp;context)</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">writeCount</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">writes</code></td><td><code class="signature"><span class="k">const</span> <a href="../../core/string-id/string-id.html">core::string_id</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->

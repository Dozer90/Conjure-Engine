---
kind: struct
---

[Engine](../../../engine.md) → [ECS](../../ecs.md) → [component_type_desc.h](../component-type-desc.md)

# component_type_desc
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::ecs` |
| Include File Path | `engine/include/conjure/ecs/component_type_desc.h` |
| Source File Path | N/A |
| API ↗ | [Entities and components](../../../../api/ecs/entities-and-components.md) |

## Syntax

```cpp
struct component_type_desc
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">alignment</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">1</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">EXTENSION_<wbr>POINT</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../core/string-id/string-id.html">core::<wbr>string_<wbr>id</a></code></td><td><code class="signature">{<span class="s">"ecs.component"</span>}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">flags</code></td><td><code class="signature"><a href="../component.html#symbol-component-flags">component_flags</a></code></td><td><code class="signature">component_<wbr>flags::<wbr>NONE</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">name</code></td><td><code class="signature"><span class="k">const</span> <span class="k">char</span> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">ops</code></td><td><code class="signature"><a href="../component/component-ops.html">component_ops</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">reflection</code></td><td><code class="signature"><span class="k">const</span> <a href="../../core/reflect/type-info/reflect-type-info.html">core::<wbr>reflect::<wbr>type_<wbr>info</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">serializer</code></td><td><code class="signature"><a href="../component/component-serializer.html">component_serializer</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">size</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->

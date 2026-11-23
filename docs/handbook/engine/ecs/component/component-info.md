---
kind: struct
---

[Engine](../../../engine.md) → [ECS](../../ecs.md) → [component.h](../component.md)

# component_info
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::core::json` |
| Include File Path | `engine/include/conjure/ecs/component.h` |
| Source File Path | N/A |
| API ↗ | [Entities and components](../../../../api/ecs/entities-and-components.md) |

## Syntax

```cpp
struct component_info
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">alignment</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">1</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">asset<wbr>Ref<wbr>Field<wbr>Offsets</code></td><td><code class="signature">std::vector&lt;<a href="../../core/types.html#symbol-u32">u32</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">defaultValue</code></td><td><code class="signature">std::vector&lt;std::byte&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">entity<wbr>Field<wbr>Offsets</code></td><td><code class="signature">std::vector&lt;<a href="../../core/types.html#symbol-u32">u32</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">flags</code></td><td><code class="signature"><a href="../component.html#symbol-component-flags">component_flags</a></code></td><td><code class="signature">component_<wbr>flags::<wbr>NONE</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">id</code></td><td><code class="signature"><a href="component-id.html">component_id</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">name</code></td><td><code class="signature"><a href="../../core/string-id/string-id.html">core::string_id</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">ops</code></td><td><code class="signature"><a href="component-ops.html">component_ops</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">reflection</code></td><td><code class="signature"><span class="k">const</span> <a href="../../core/reflect/type-info/reflect-type-info.html">core::<wbr>reflect::<wbr>type_<wbr>info</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">serializer</code></td><td><code class="signature"><a href="component-serializer.html">component_serializer</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">size</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `is_tag` {#function-is-tag}

<div class="function-declaration" id="function-is-tag-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_tag() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when the type has the <code>TAG</code> flag (no storage).</p>

#### `is_transient` {#function-is-transient}

<div class="function-declaration" id="function-is-transient-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_transient() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when the type is never serialised.</p>

#### `is_trivial` {#function-is-trivial}

<div class="function-declaration" id="function-is-trivial-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_trivial() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when the type is relocated and copied with memcpy and needs no destructor.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->

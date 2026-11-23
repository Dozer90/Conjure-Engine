---
kind: struct
---

[Engine](../../../../engine.md) → [Core](../../../core.md) → [field_info.h](../field-info.md)

# reflect::field_info
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::core::reflect` |
| Include File Path | `engine/include/conjure/core/reflect/field_info.h` |
| Source File Path | `engine/src/core/reflect/field_info.cpp` |
| API ↗ | [Reflection](../../../../../api/core/reflection.md) |

## Syntax

```cpp
struct field_info
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">arrayCount</code></td><td><code class="signature"><a href="../../types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">attributes</code></td><td><code class="signature"><a href="reflect-field-attributes.html">field_attributes</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">default<wbr>Value<wbr>Json</code></td><td><code class="signature">std::string</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">elementKind</code></td><td><code class="signature"><a href="../field-info.html#symbol-reflect-field-kind">field_kind</a></code></td><td><code class="signature">field_<wbr>kind::<wbr>COUNT</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">enumType</code></td><td><code class="signature"><span class="k">const</span> <a href="reflect-enum-info.html">enum_info</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">kind</code></td><td><code class="signature"><a href="../field-info.html#symbol-reflect-field-kind">field_kind</a></code></td><td><code class="signature">field_<wbr>kind::<wbr>COUNT</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">name</code></td><td><code class="signature">std::string</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">offset</code></td><td><code class="signature"><a href="../../types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">size</code></td><td><code class="signature"><a href="../../types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">structType</code></td><td><code class="signature"><span class="k">const</span> <a href="../type-info/reflect-type-info.html">type_info</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">type</code></td><td><code class="signature"><a href="../type-id/reflect-type-id.html">type_id</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `element_size` {#function-element-size}

<div class="function-declaration" id="function-element-size-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-u32">u32</a> element_size() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Size of one value in bytes: size / <code>arrayCount</code> for arrays, size otherwise.</p>

#### `is_array` {#function-is-array}

<div class="function-declaration" id="function-is-array-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_array() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True for <code>ARRAY</code> fields.</p>

#### `value_kind` {#function-value-kind}

<div class="function-declaration" id="function-value-kind-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../field-info.html#symbol-reflect-field-kind">field_kind</a> value_kind() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The kind of one value: <code>elementKind</code> for arrays, kind otherwise.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->

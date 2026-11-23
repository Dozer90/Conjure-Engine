---
kind: struct
---

[Engine](../../../../engine.md) → [Core](../../../core.md) → [json_conversion.h](../json-conversion.md)

# reflect::field_ref
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::core::json` |
| Include File Path | `engine/include/conjure/core/reflect/json_conversion.h` |
| Source File Path | `engine/src/core/reflect/json_conversion.cpp` |
| API ↗ | [Reflection](../../../../../api/core/reflection.md) |

## Syntax

```cpp
struct field_ref
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">data</code></td><td><code class="signature"><span class="k">void</span> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">field</code></td><td><code class="signature"><span class="k">const</span> <a href="../field-info/reflect-field-info.html">field_info</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">isElement</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">path</code></td><td><code class="signature">std::string</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `is_valid` {#function-is-valid}

<div class="function-declaration" id="function-is-valid-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_valid() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when both the data and the description are set.</p>

#### `kind` {#function-kind}

<div class="function-declaration" id="function-kind-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../field-info.html#symbol-reflect-field-kind">field_kind</a> kind() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The kind of the referenced value (the element kind for an array element).</p>

#### `size` {#function-size}

<div class="function-declaration" id="function-size-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-u32">u32</a> size() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Size in bytes of the referenced value.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->

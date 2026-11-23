---
kind: struct
---

[Engine](../../../../engine.md) → [Core](../../../core.md) → [field_info.h](../field-info.md)

# reflect::enum_info
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::core::reflect` |
| Include File Path | `engine/include/conjure/core/reflect/field_info.h` |
| Source File Path | `engine/src/core/reflect/field_info.cpp` |
| API ↗ | [Reflection](../../../../../api/core/reflection.md) |

## Syntax

```cpp
struct CONJURE_API enum_info
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">isFlags</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">isSigned</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">name</code></td><td><code class="signature">std::string</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">type</code></td><td><code class="signature"><a href="../type-id/reflect-type-id.html">type_id</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">underlying<wbr>Size</code></td><td><code class="signature"><a href="../../types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">4</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">values</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr>std::<wbr>pair&lt;<wbr>std::<wbr>string,<wbr> <a href="../../types.html#symbol-i64">i64</a>&gt;&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `find_name` {#function-find-name}

<div class="function-declaration" id="function-find-name-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> std::string *find_name(
    <a href="../../types.html#symbol-i64">i64</a> value) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The name of the first enumerator with this exact value, or <code>nullptr</code>.</p>

#### `find_value` {#function-find-value}

<div class="function-declaration" id="function-find-value-1"><pre><code>[[<span class="na">nodiscard</span>]] std::optional&lt;<a href="../../types.html#symbol-i64">i64</a>&gt; find_value(
    std::string_view enumeratorName) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The enumerator value named <code>name</code>, or nullopt. Exact, case-sensitive match.</p>

#### `format_value` {#function-format-value}

<div class="function-declaration" id="function-format-value-1"><pre><code>[[<span class="na">nodiscard</span>]] std::optional&lt;std::string&gt; format_value(
    <a href="../../types.html#symbol-i64">i64</a> value) <span class="k">const</span>;</code></pre></div>

<p>Text form of a value: the enumerator name when one matches exactly; for flags, the single-bit enumerator names joined with &#x27;|&#x27; (&quot;&quot; for zero without a zero enumerator). Returns nullopt when the value cannot be spelled with the known enumerators.</p>

#### `is_representable` {#function-is-representable}

<div class="function-declaration" id="function-is-representable-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_representable(
    <a href="../../types.html#symbol-i64">i64</a> value) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when <code>value</code> fits the underlying type (bit pattern for 64-bit unsigned). Always <code>false</code> when <code>underlyingSize</code> is not 1, 2, 4 or 8.</p>

#### `parse_value` {#function-parse-value}

<div class="function-declaration" id="function-parse-value-1"><pre><code>[[<span class="na">nodiscard</span>]] std::optional&lt;<a href="../../types.html#symbol-i64">i64</a>&gt; parse_value(
    std::string_view text) <span class="k">const</span>;</code></pre></div>

<p>Inverse of <code><a href="#function-format-value-1">format_value</a>()</code>: an enumerator name, or for flags names separated by &#x27;|&#x27; (whitespace around names ignored, empty pieces ignored, &quot;&quot; is zero). Returns nullopt on any unknown name.</p>

## Operator Overloads

### Public

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="reflect-enum-info.html">enum_info</a> &amp;) <span class="k">const</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->

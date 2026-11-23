---
kind: struct
---

[Engine](../../../../engine.md) → [Core](../../../core.md) → [type_info.h](../type-info.md)

# reflect::type_info
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::core::reflect` |
| Include File Path | `engine/include/conjure/core/reflect/type_info.h` |
| Source File Path | `engine/src/core/reflect/type_info.cpp` |
| API ↗ | [Reflection](../../../../../api/core/reflection.md) |

## Syntax

```cpp
struct CONJURE_API type_info
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">alignment</code></td><td><code class="signature"><a href="../../types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">fields</code></td><td><code class="signature">std::vector&lt;<a href="../field-info/reflect-field-info.html">field_info</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">is<wbr>Trivially<wbr>Copyable</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">true</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">name</code></td><td><code class="signature">std::string</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">size</code></td><td><code class="signature"><a href="../../types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">type</code></td><td><code class="signature"><a href="../type-id/reflect-type-id.html">type_id</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `find_field` {#function-find-field}

<div class="function-declaration" id="function-find-field-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../field-info/reflect-field-info.html">field_info</a> *find_field(
    std::string_view fieldName) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The field named <code>fieldName</code>, or <code>nullptr</code>. Exact match on the declared member name.</p>

#### `has_same_layout` {#function-has-same-layout}

<div class="function-declaration" id="function-has-same-layout-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> has_same_layout(
    <span class="k">const</span> <a href="reflect-type-info.html">type_info</a> &amp;other) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when size, alignment and every field&#x27;s name, kind, offset, size, array count and element kind (arrays) and, for <code>STRUCT</code> and <code>ENUM</code> values, type id match; implies equal <code><a href="#function-layout-hash-1">layout_hash</a>()</code>, which covers fewer components.</p>

#### `layout_hash` {#function-layout-hash}

<div class="function-declaration" id="function-layout-hash-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-u64">u64</a> layout_hash() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>FNV-1a 64 over (name, kind, offset, size) of every field in order: the UTF-8 bytes of the name, the kind as one <code><a href="../../types.html#symbol-byte">byte</a></code>, then offset and size as little-endian <code><a href="../../types.html#symbol-u32">u32</a></code>. Starts from the FNV offset basis and ignores the type name, so two types with the same fields hash alike. Mirrored by the <code>C</code># side.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->

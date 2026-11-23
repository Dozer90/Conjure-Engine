---
kind: struct
---

[Engine](../../../engine.md) → [Scripting](../../scripting.md) → [script_class_registry.h](../script-class-registry.md)

# script_class_info
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::scripting` |
| Include File Path | `engine/include/conjure/scripting/script_class_registry.h` |
| Source File Path | `engine/src/scripting/script_class_registry.cpp` |
| API ↗ | [Scripting](../../../../api/scripting/index.md), [Scripts, components and the runtime](../../../../api/scripting/scripts-and-components.md) |

## Syntax

```cpp
struct CONJURE_API script_class_info
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">callbackMask</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u16">u16</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">fields</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="script-field-info.html">script_<wbr>field_<wbr>info</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">fullName</code></td><td><code class="signature">std::string</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">hash</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `find_field` {#function-find-field}

<div class="function-declaration" id="function-find-field-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="script-field-info.html">script_field_info</a> *find_field(
    <a href="../../core/types.html#symbol-u64">u64</a> fieldHash) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The field with this hash, or <code>nullptr</code> (binary search; fields must be sorted).</p>

<div class="function-declaration" id="function-find-field-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="script-field-info.html">script_field_info</a> *find_field(
    std::string_view fieldName) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The field with this name, or <code>nullptr</code>.</p>

#### `has_callback` {#function-has-callback}

<div class="function-declaration" id="function-has-callback-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> has_callback(
    <a href="../native-api.html#symbol-script-callback">script_callback</a> callback) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when the class overrides the callback.</p>

#### `sort_fields` {#function-sort-fields}

<div class="function-declaration" id="function-sort-fields-1"><pre><code><span class="k">void</span> sort_fields();</code></pre></div>

<p>Sorts the fields by hash (what <code>find_field</code> needs).</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->

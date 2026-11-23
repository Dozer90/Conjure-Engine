---
kind: class
---

[Engine](../../../engine.md) → [Scripting](../../scripting.md) → [script_fields.h](../script-fields.md)

# field_blob_reader
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::scripting` |
| Include File Path | `engine/include/conjure/scripting/script_fields.h` |
| Source File Path | `engine/src/scripting/script_fields.cpp` |
| API ↗ | [Scripting](../../../../api/scripting/index.md), [Scripts, components and the runtime](../../../../api/scripting/scripts-and-components.md) |

## Syntax

```cpp
class CONJURE_API field_blob_reader
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">explicit</span> field_blob_reader(
    std::span&lt;<span class="k">const</span> <a href="../../core/types.html#symbol-u8">u8</a>&gt; blob) <span class="k">noexcept</span>;</code></td><td>Validates the header; a blob of 0 bytes is treated as an empty, valid blob.</td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~field_blob_reader();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mBlob</code></td><td><code class="signature">std::span&lt;<span class="k">const</span> <a href="../../core/types.html#symbol-u8">u8</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mCursor</code></td><td><code class="signature"><a href="../../core/types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mFieldCount</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mFieldsRead</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mValid</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `field_count` {#function-field-count}

<div class="function-declaration" id="function-field-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> field_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Fields the header announces.</p>

#### `is_valid` {#function-is-valid}

<div class="function-declaration" id="function-is-valid-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_valid() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>False when the header is bad (magic, version, truncated).</p>

#### `next` {#function-next}

<div class="function-declaration" id="function-next-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">bool</span>&gt; next(
    <a href="field-blob-field.html">field_blob_field</a> &amp;out);</code></pre></div>

<p>Reads the next field. Returns <code>false</code> at the end, or <code>FIELD_BLOB_INVALID</code> when the field header or payload runs past the blob.</p>

#### `validate` {#function-validate}

<div class="function-declaration" id="function-validate-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; validate(
    std::span&lt;<span class="k">const</span> <a href="../../core/types.html#symbol-u8">u8</a>&gt; blob);</code></pre></div>

<p>Reads the whole blob and fails on the first malformed field.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

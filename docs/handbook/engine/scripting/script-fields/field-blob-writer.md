---
kind: class
---

[Engine](../../../engine.md) → [Scripting](../../scripting.md) → [script_fields.h](../script-fields.md)

# field_blob_writer
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::scripting` |
| Include File Path | `engine/include/conjure/scripting/script_fields.h` |
| Source File Path | `engine/src/scripting/script_fields.cpp` |
| API ↗ | [Scripting](../../../../api/scripting/index.md), [Scripts, components and the runtime](../../../../api/scripting/scripts-and-components.md) |

## Syntax

```cpp
class CONJURE_API field_blob_writer
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">explicit</span> field_blob_writer(
    std::span&lt;<a href="../../core/types.html#symbol-u8">u8</a>&gt; output) <span class="k">noexcept</span>;</code></td><td>Writes into <code>output</code>; an empty span measures (<code>required_size()</code> reports what a real pass needs). Every write past the end sets <code>overflowed()</code> and keeps counting bytes.</td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~field_blob_writer();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mBegun</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mCursor</code></td><td><code class="signature"><a href="../../core/types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mFieldCount</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mFieldOpen</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Open<wbr>Field<wbr>Payload<wbr>Start</code></td><td><code class="signature"><a href="../../core/types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Open<wbr>Field<wbr>Size<wbr>Offset</code></td><td><code class="signature"><a href="../../core/types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mOutput</code></td><td><code class="signature">std::span&lt;<a href="../../core/types.html#symbol-u8">u8</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mOverflowed</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `begin` {#function-begin}

<div class="function-declaration" id="function-begin-1"><pre><code><span class="k">void</span> begin();</code></pre></div>

<p>Writes the header (the field count is patched by <code><a href="#function-finish-1">finish</a>()</code>).</p>

#### `begin_field` {#function-begin-field}

<div class="function-declaration" id="function-begin-field-1"><pre><code><span class="k">void</span> begin_field(
    <a href="../../core/types.html#symbol-u64">u64</a> hash,
    <a href="../native-api.html#symbol-script-field-kind">script_field_kind</a> kind,
    <a href="../native-api.html#symbol-script-field-kind">script_field_kind</a> elementKind);</code></pre></div>

<p>Starts a field whose payload is written piecewise through the raw helpers; <code><a href="#function-end-field-1">end_field</a>()</code> patches the payload size. Nested <code>begin_field</code> is not allowed.</p>

#### `end_field` {#function-end-field}

<div class="function-declaration" id="function-end-field-1"><pre><code><span class="k">void</span> end_field();</code></pre></div>

#### `field_count` {#function-field-count}

<div class="function-declaration" id="function-field-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> field_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `finish` {#function-finish}

<div class="function-declaration" id="function-finish-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-usize">usize</a> finish();</code></pre></div>

<p>Patches the field count and returns the bytes the blob needs. When that is more than the output size (<code><a href="#function-overflowed-1">overflowed</a>()</code>), the output is NOT a valid blob: the header counts every field while the bytes stop at the overflow, possibly inside a field header, so <code><a href="field-blob-reader.html">field_blob_reader</a></code> rejects it. Retry with an output of the returned size (the measure-then-write idiom <code>json_to_blob</code> uses).</p>

#### `overflowed` {#function-overflowed}

<div class="function-declaration" id="function-overflowed-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> overflowed() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `pad_to_4` {#function-pad-to-4}

<div class="function-declaration" id="function-pad-to-4-1"><pre><code><span class="k">void</span> pad_to_4();</code></pre></div>

#### `required_size` {#function-required-size}

<div class="function-declaration" id="function-required-size-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-usize">usize</a> required_size() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `write_asset` {#function-write-asset}

<div class="function-declaration" id="function-write-asset-1"><pre><code><span class="k">void</span> write_asset(
    <a href="../../core/types.html#symbol-u64">u64</a> hash,
    <span class="k">const</span> <a href="../../core/uuid/uuid.html">core::uuid</a> &amp;value);</code></pre></div>

#### `write_bool` {#function-write-bool}

<div class="function-declaration" id="function-write-bool-1"><pre><code><span class="k">void</span> write_bool(
    <a href="../../core/types.html#symbol-u64">u64</a> hash,
    <span class="k">bool</span> value);</code></pre></div>

#### `write_bytes` {#function-write-bytes}

<div class="function-declaration" id="function-write-bytes-1"><pre><code><span class="k">void</span> write_bytes(
    std::span&lt;<span class="k">const</span> <a href="../../core/types.html#symbol-u8">u8</a>&gt; bytes);</code></pre></div>

#### `write_double` {#function-write-double}

<div class="function-declaration" id="function-write-double-1"><pre><code><span class="k">void</span> write_double(
    <a href="../../core/types.html#symbol-u64">u64</a> hash,
    <a href="../../core/types.html#symbol-f64">f64</a> value);</code></pre></div>

#### `write_entity` {#function-write-entity}

<div class="function-declaration" id="function-write-entity-1"><pre><code><span class="k">void</span> write_entity(
    <a href="../../core/types.html#symbol-u64">u64</a> hash,
    <a href="../../ecs/entity/entity.html">ecs::entity</a> value);</code></pre></div>

#### `write_field` {#function-write-field}

<div class="function-declaration" id="function-write-field-1"><pre><code><span class="k">void</span> write_field(
    <a href="../../core/types.html#symbol-u64">u64</a> hash,
    <a href="../native-api.html#symbol-script-field-kind">script_field_kind</a> kind,
    <a href="../native-api.html#symbol-script-field-kind">script_field_kind</a> elementKind,
    std::span&lt;<span class="k">const</span> <a href="../../core/types.html#symbol-u8">u8</a>&gt; payload);</code></pre></div>

<p>Writes one field header plus payload, padded to 4 bytes.</p>

#### `write_float` {#function-write-float}

<div class="function-declaration" id="function-write-float-1"><pre><code><span class="k">void</span> write_float(
    <a href="../../core/types.html#symbol-u64">u64</a> hash,
    <a href="../../core/types.html#symbol-f32">f32</a> value);</code></pre></div>

#### `write_floats` {#function-write-floats}

<div class="function-declaration" id="function-write-floats-1"><pre><code><span class="k">void</span> write_floats(
    <a href="../../core/types.html#symbol-u64">u64</a> hash,
    <a href="../native-api.html#symbol-script-field-kind">script_field_kind</a> kind,
    std::span&lt;<span class="k">const</span> <a href="../../core/types.html#symbol-f32">f32</a>&gt; values);</code></pre></div>

#### `write_int32` {#function-write-int32}

<div class="function-declaration" id="function-write-int32-1"><pre><code><span class="k">void</span> write_int32(
    <a href="../../core/types.html#symbol-u64">u64</a> hash,
    <a href="../../core/types.html#symbol-i32">i32</a> value,
    <a href="../native-api.html#symbol-script-field-kind">script_field_kind</a> kind = script_field_kind::INT32);</code></pre></div>

#### `write_int64` {#function-write-int64}

<div class="function-declaration" id="function-write-int64-1"><pre><code><span class="k">void</span> write_int64(
    <a href="../../core/types.html#symbol-u64">u64</a> hash,
    <a href="../../core/types.html#symbol-i64">i64</a> value);</code></pre></div>

#### `write_string` {#function-write-string}

<div class="function-declaration" id="function-write-string-1"><pre><code><span class="k">void</span> write_string(
    <a href="../../core/types.html#symbol-u64">u64</a> hash,
    std::string_view value);</code></pre></div>

#### `write_u32` {#function-write-u32}

<div class="function-declaration" id="function-write-u32-1"><pre><code><span class="k">void</span> write_u32(
    <a href="../../core/types.html#symbol-u32">u32</a> value);</code></pre></div>

### Private

#### `patch_u16` {#function-patch-u16}

<div class="function-declaration" id="function-patch-u16-1"><pre><code><span class="k">void</span> patch_u16(
    <a href="../../core/types.html#symbol-usize">usize</a> offset,
    <a href="../../core/types.html#symbol-u16">u16</a> value);</code></pre></div>

#### `patch_u32` {#function-patch-u32}

<div class="function-declaration" id="function-patch-u32-1"><pre><code><span class="k">void</span> patch_u32(
    <a href="../../core/types.html#symbol-usize">usize</a> offset,
    <a href="../../core/types.html#symbol-u32">u32</a> value);</code></pre></div>

#### `put` {#function-put}

<div class="function-declaration" id="function-put-1"><pre><code><span class="k">void</span> put(
    <span class="k">const</span> <span class="k">void</span> *data,
    <a href="../../core/types.html#symbol-usize">usize</a> size);</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

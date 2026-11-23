---
kind: header
---

[Engine](../../engine.md) → [Scripting](../scripting.md)

# script_fields.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/scripting/script_fields.h` |
| API ↗ | [Scripting](../../../api/scripting/index.md), [Scripts, components and the runtime](../../../api/scripting/scripts-and-components.md) |

<p>The field blob: the single wire format of <code>apply_fields</code> and <code>collect_fields</code>.</p>

## Structs

| File | Description |
|---|---|
| [`field_blob_field`](script-fields/field-blob-field.md) | One field as the reader yields it. The payload view points into the blob. |

## Classes

| File | Description |
|---|---|
| [`field_blob_reader`](script-fields/field-blob-reader.md) |  |
| [`field_blob_writer`](script-fields/field-blob-writer.md) |  |

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-FIELD_BLOB_FIELD_HEADER_SIZE"></a><code>FIELD_BLOB_FIELD_HEADER_SIZE</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-usize">usize</a></code> | <code class="signature"><span class="mi">16</span></code> |  |
| <a id="variable-FIELD_BLOB_HEADER_SIZE"></a><code>FIELD_BLOB_HEADER_SIZE</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-usize">usize</a></code> | <code class="signature"><span class="mi">8</span></code> |  |
| <a id="variable-FIELD_BLOB_MAGIC"></a><code>FIELD_BLOB_MAGIC</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">0</span>x42465343u</code> |  |
| <a id="variable-FIELD_BLOB_VERSION"></a><code>FIELD_BLOB_VERSION</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u16">u16</a></code> | <code class="signature"><span class="mi">1</span></code> |  |

## Functions

### Public

#### `blob_to_json` {#function-blob-to-json}

<div class="function-declaration" id="function-blob-to-json-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<a href="../core/json/document/json-document.html">core::json::document</a>&gt; blob_to_json(
    <span class="k">const</span> <a href="script-class-registry/script-class-info.html">script_class_info</a> &amp;classInfo,
    std::span&lt;<span class="k">const</span> <a href="../core/types.html#symbol-u8">u8</a>&gt; blob);</code></pre></div>

<p>Decodes a blob into a JSON object keyed by field name. Fields whose hash the class does not know are skipped (counted in a <code>DEBUG</code> log). <code>FIELD_BLOB_INVALID</code> for a malformed blob.</p>

#### `entity_from_json` {#function-entity-from-json}

<div class="function-declaration" id="function-entity-from-json-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../ecs/entity/entity.html">ecs::entity</a> entity_from_json(
    <span class="k">const</span> <a href="../core/json/value/json-value.html">core::json::value</a> &amp;value) <span class="k">noexcept</span>;</code></pre></div>

<p>The JSON form of an entity reference in script fields: the packed id as a number (0 = <code>null</code>).</p>

#### `is_value_compatible` {#function-is-value-compatible}

<div class="function-declaration" id="function-is-value-compatible-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <span class="k">bool</span> is_value_compatible(
    <a href="native-api.html#symbol-script-field-kind">script_field_kind</a> kind,
    <a href="native-api.html#symbol-script-field-kind">script_field_kind</a> elementKind,
    <span class="k">const</span> <a href="../core/json/value/json-value.html">core::json::value</a> &amp;value) <span class="k">noexcept</span>;</code></pre></div>

<p>True when a JSON value can be encoded as this kind (number for <code>INT32</code>, array of 3 numbers for <code>VEC3</code>, ...). An invalid value is never compatible.</p>

#### `json_to_blob` {#function-json-to-blob}

<div class="function-declaration" id="function-json-to-blob-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<a href="../core/types.html#symbol-usize">usize</a>&gt; json_to_blob(
    <span class="k">const</span> <a href="script-class-registry/script-class-info.html">script_class_info</a> &amp;classInfo,
    <span class="k">const</span> <a href="../core/json/value/json-value.html">core::json::value</a> &amp;fields,
    std::span&lt;<a href="../core/types.html#symbol-u8">u8</a>&gt; output);</code></pre></div>

<p>Encodes the fields of a class from a JSON object into a blob. Missing or mismatched JSON members take the field default; JSON keys not in the schema are ignored (they stay in <code><a href="script-component/script-entry.html">script_entry</a>.fields</code>). Returns the bytes needed; with an output that is too small the content is cut at the overflow and the caller retries with the returned size. Only a pass with a non-empty output warns about mismatched members, so the measure-then-write idiom reports each once.</p>

#### `read_field_value` {#function-read-field-value}

<div class="function-declaration" id="function-read-field-value-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<a href="../core/json/document/json-document.html">core::json::document</a>&gt; read_field_value(
    <span class="k">const</span> <a href="script-class-registry/script-field-info.html">script_field_info</a> &amp;field,
    std::span&lt;<span class="k">const</span> <a href="../core/types.html#symbol-u8">u8</a>&gt; payload);</code></pre></div>

<p>Decodes one payload into JSON. <code>FIELD_BLOB_INVALID</code> when the payload is too short or malformed.</p>

#### `remap_entity_fields_json` {#function-remap-entity-fields-json}

<div class="function-declaration" id="function-remap-entity-fields-json-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/json/document/json-document.html">core::json::document</a> remap_entity_fields_json(
    <span class="k">const</span> <a href="script-class-registry/script-class-info.html">script_class_info</a> &amp;classInfo,
    <span class="k">const</span> <a href="../core/json/value/json-value.html">core::json::value</a> &amp;fields,
    <span class="k">const</span> std::function&lt;<a href="../ecs/entity/entity.html">ecs::entity</a>(ecs::entity)&gt; &amp;remap);</code></pre></div>

<p>Rewrites every <code>ENTITY</code> field and every element of a <code>LIST</code> of <code>ENTITY</code> in <code>fields</code> through <code>remap</code> and returns the result; members the schema does not know pass through unchanged.</p>

#### `write_field_value` {#function-write-field-value}

<div class="function-declaration" id="function-write-field-value-1"><pre><code><span class="na">CONJURE_API</span> <span class="k">void</span> write_field_value(
    <a href="script-fields/field-blob-writer.html">field_blob_writer</a> &amp;writer,
    <span class="k">const</span> <a href="script-class-registry/script-class-info.html">script_class_info</a> &amp;classInfo,
    <span class="k">const</span> <a href="script-class-registry/script-field-info.html">script_field_info</a> &amp;field,
    <span class="k">const</span> <a href="../core/json/value/json-value.html">core::json::value</a> &amp;value,
    <span class="k">bool</span> warnOnMismatch = true);</code></pre></div>

<p>Appends the payload of one value of <code>field</code> (default when <code>value</code> is invalid or mismatched, with a <code>WARN</code> naming the class and field unless <code>warnOnMismatch</code> is <code>false</code>) as one blob field.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="blob_to_json"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="entity_from_json"></a>
<a id="extension-points"></a>
<a id="field_blob_field"></a>
<a id="field_blob_field_header_size"></a>
<a id="field_blob_fieldelementkind"></a>
<a id="field_blob_fieldhash"></a>
<a id="field_blob_fieldkind"></a>
<a id="field_blob_fieldpayload"></a>
<a id="field_blob_header_size"></a>
<a id="field_blob_magic"></a>
<a id="field_blob_reader"></a>
<a id="field_blob_readerfield_blob_reader"></a>
<a id="field_blob_readerfield_blob_reader_1"></a>
<a id="field_blob_readerfield_count"></a>
<a id="field_blob_readeris_valid"></a>
<a id="field_blob_readermblob"></a>
<a id="field_blob_readermcursor"></a>
<a id="field_blob_readermfieldcount"></a>
<a id="field_blob_readermfieldsread"></a>
<a id="field_blob_readermvalid"></a>
<a id="field_blob_readernext"></a>
<a id="field_blob_readeroperator"></a>
<a id="field_blob_readervalidate"></a>
<a id="field_blob_version"></a>
<a id="field_blob_writer"></a>
<a id="field_blob_writerbegin"></a>
<a id="field_blob_writerbegin_field"></a>
<a id="field_blob_writerend_field"></a>
<a id="field_blob_writerfield_blob_writer"></a>
<a id="field_blob_writerfield_blob_writer_1"></a>
<a id="field_blob_writerfield_count"></a>
<a id="field_blob_writerfinish"></a>
<a id="field_blob_writermbegun"></a>
<a id="field_blob_writermcursor"></a>
<a id="field_blob_writermfieldcount"></a>
<a id="field_blob_writermfieldopen"></a>
<a id="field_blob_writermopenfieldpayloadstart"></a>
<a id="field_blob_writermopenfieldsizeoffset"></a>
<a id="field_blob_writermoutput"></a>
<a id="field_blob_writermoverflowed"></a>
<a id="field_blob_writeroperator"></a>
<a id="field_blob_writeroverflowed"></a>
<a id="field_blob_writerpad_to_4"></a>
<a id="field_blob_writerpatch_u16"></a>
<a id="field_blob_writerpatch_u32"></a>
<a id="field_blob_writerput"></a>
<a id="field_blob_writerrequired_size"></a>
<a id="field_blob_writerwrite_asset"></a>
<a id="field_blob_writerwrite_bool"></a>
<a id="field_blob_writerwrite_bytes"></a>
<a id="field_blob_writerwrite_double"></a>
<a id="field_blob_writerwrite_entity"></a>
<a id="field_blob_writerwrite_field"></a>
<a id="field_blob_writerwrite_float"></a>
<a id="field_blob_writerwrite_floats"></a>
<a id="field_blob_writerwrite_int32"></a>
<a id="field_blob_writerwrite_int64"></a>
<a id="field_blob_writerwrite_string"></a>
<a id="field_blob_writerwrite_u32"></a>
<a id="internals-and-algorithms"></a>
<a id="is_value_compatible"></a>
<a id="json_to_blob"></a>
<a id="read_field_value"></a>
<a id="remap_entity_fields_json"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<a id="write_field_value"></a>
<!-- doc-legacy-anchors:end -->

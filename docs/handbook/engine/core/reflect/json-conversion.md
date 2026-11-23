---
kind: header
---

[Engine](../../../engine.md) → [Core](../../core.md)

# json_conversion.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/core/reflect/json_conversion.h` |
| API ↗ | [Reflection](../../../../api/core/reflection.md) |

<p>JSON conversion driven by <code><a href="type-info/reflect-type-info.html">type_info</a></code>.</p>

## Structs

| File | Description |
|---|---|
| [`json::json_converter<math::mat4>`](json-conversion/json-json-converter-math-mat4.md) |  |
| [`json::json_converter<math::quat>`](json-conversion/json-json-converter-math-quat.md) |  |
| [`json::json_converter<math::transform>`](json-conversion/json-json-converter-math-transform.md) |  |
| [`json::json_converter<math::vec2>`](json-conversion/json-json-converter-math-vec2.md) |  |
| [`json::json_converter<math::vec3>`](json-conversion/json-json-converter-math-vec3.md) |  |
| [`json::json_converter<math::vec4>`](json-conversion/json-json-converter-math-vec4.md) |  |
| [`json::json_converter<string_id>`](json-conversion/json-json-converter-string-id.md) |  |
| [`reflect::field_ref`](json-conversion/reflect-field-ref.md) | <code>A</code> resolved field inside an object: the data address plus its description. When the path ended in an array index the reference designates one element and <code>kind()</code> is the element kind. <code>path</code> is the normalised path <code>resolve_path()</code> followed (&quot;parts[1].weight&quot;); error messages of <code>write_json</code>/<code>set_from_json</code>/<code>get_as_json</code> use it, falling back to the field name when it is empty. |

## Functions

### Public

#### `reflect::from_json` {#function-reflect-from-json}

<div class="function-declaration" id="function-reflect-from-json-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../result/result.html">result</a>&lt;<span class="k">void</span>&gt; from_json( <span class="k">const</span> <a href="type-info/reflect-type-info.html">type_info</a> &amp;type, <span class="k">void</span> *object, <span class="k">const</span> <a href="../json/value/json-value.html">json::value</a> &amp;in, std::vector&lt;std::string&gt; *unknownFields = nullptr );</code></pre></div>

<p>Updates <code>object</code> from a JSON object. Missing members keep their current value; unknown members are appended to <code>unknownFields</code> (dotted paths) when it is not <code>null</code>. Stops at the first failing field.</p>

<div class="function-declaration" id="function-reflect-from-json-2"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> T&gt; [[<span class="na">nodiscard</span>]] <a href="../result/result.html">result</a>&lt;<span class="k">void</span>&gt; from_json( T &amp;object, <span class="k">const</span> <a href="../json/value/json-value.html">json::value</a> &amp;in, std::vector&lt;std::string&gt; *unknownFields = nullptr );</code></pre></div>

<p><code>from_json()</code> for a <code><a href="../../audio/components.html#symbol-conjure-reflect-struct">CONJURE_REFLECT_STRUCT</a></code> type.</p>

#### `reflect::get_as_json` {#function-reflect-get-as-json}

<div class="function-declaration" id="function-reflect-get-as-json-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../result/result.html">result</a>&lt;<a href="../json/document/json-document.html">json::document</a>&gt; get_as_json(<a href="json-conversion/reflect-field-ref.html">field_ref</a> ref);</code></pre></div>

<p>The referenced value as a new document.</p>

#### `reflect::resolve_path` {#function-reflect-resolve-path}

<div class="function-declaration" id="function-reflect-resolve-path-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../result/result.html">result</a>&lt;<a href="json-conversion/reflect-field-ref.html">field_ref</a>&gt; resolve_path( <span class="k">const</span> <a href="type-info/reflect-type-info.html">type_info</a> &amp;type, <span class="k">void</span> *object, std::string_view path );</code></pre></div>

<p>Follows a path such as &quot;<code>a.b</code>[2].c&quot; through nested structs and arrays. Indices are plain decimal digits. <code>NOT_FOUND</code> for an unknown field name, <code>OUT_OF_RANGE</code> for an index past the array, <code>INVALID_ARGUMENT</code> for a malformed path (including a signed index such as &quot;[+1]&quot;), an index on a non-array or a &#x27;.&#x27; into a non-struct. The returned reference carries the normalised path.</p>

#### `reflect::set_from_json` {#function-reflect-set-from-json}

<div class="function-declaration" id="function-reflect-set-from-json-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../result/result.html">result</a>&lt;<span class="k">void</span>&gt; set_from_json(<a href="json-conversion/reflect-field-ref.html">field_ref</a> ref, <span class="k">const</span> <a href="../json/value/json-value.html">json::value</a> &amp;in);</code></pre></div>

<p>Replaces the referenced value from JSON with the same rules as <code>from_json()</code>. Errors name the reference&#x27;s path.</p>

#### `reflect::to_json` {#function-reflect-to-json}

<div class="function-declaration" id="function-reflect-to-json-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../result/result.html">result</a>&lt;<span class="k">void</span>&gt; to_json(<span class="k">const</span> <a href="type-info/reflect-type-info.html">type_info</a> &amp;type, <span class="k">const</span> <span class="k">void</span> *object, <a href="../json/writer/json-writer.html">json::writer</a> &amp;out);</code></pre></div>

<p>Serialises <code>object</code> (an instance of <code>type</code>) as a JSON object into <code>out</code>, fields in declaration order. Fails with <code>INVALID_STATE</code> for a <code>STRUCT</code>/<code>ENUM</code> field without its description and <code>UNSUPPORTED</code> for a custom kind of unsupported size; the writer may then be left mid-object.</p>

<div class="function-declaration" id="function-reflect-to-json-2"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> T&gt; [[<span class="na">nodiscard</span>]] <a href="../result/result.html">result</a>&lt;<span class="k">void</span>&gt; to_json(<span class="k">const</span> T &amp;object, <a href="../json/writer/json-writer.html">json::writer</a> &amp;out);</code></pre></div>

<p><code>to_json()</code> for a <code><a href="../../audio/components.html#symbol-conjure-reflect-struct">CONJURE_REFLECT_STRUCT</a></code> type. <code>INVALID_STATE</code> when the type has no registered description.</p>

#### `reflect::to_json_document` {#function-reflect-to-json-document}

<div class="function-declaration" id="function-reflect-to-json-document-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../result/result.html">result</a>&lt;<a href="../json/document/json-document.html">json::document</a>&gt; to_json_document(<span class="k">const</span> <a href="type-info/reflect-type-info.html">type_info</a> &amp;type, <span class="k">const</span> <span class="k">void</span> *object);</code></pre></div>

<p><code>to_json()</code> into a new document.</p>

<div class="function-declaration" id="function-reflect-to-json-document-2"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> T&gt; [[<span class="na">nodiscard</span>]] <a href="../result/result.html">result</a>&lt;<a href="../json/document/json-document.html">json::document</a>&gt; to_json_document(<span class="k">const</span> T &amp;object);</code></pre></div>

<p><code>to_json_document()</code> for a <code><a href="../../audio/components.html#symbol-conjure-reflect-struct">CONJURE_REFLECT_STRUCT</a></code> type.</p>

#### `reflect::write_json` {#function-reflect-write-json}

<div class="function-declaration" id="function-reflect-write-json-1"><pre><code><span class="na">CONJURE_API</span> <span class="k">void</span> write_json(<span class="k">const</span> <a href="../math/glm-types.html#symbol-vec2">math::vec2</a> &amp;value, <a href="../json/writer/json-writer.html">json::writer</a> &amp;out);</code></pre></div>

<p>Writes one math value in the representation documented above.</p>

<div class="function-declaration" id="function-reflect-write-json-2"><pre><code><span class="na">CONJURE_API</span> <span class="k">void</span> write_json(<span class="k">const</span> <a href="../math/glm-types.html#symbol-vec3">math::vec3</a> &amp;value, <a href="../json/writer/json-writer.html">json::writer</a> &amp;out);</code></pre></div>

<div class="function-declaration" id="function-reflect-write-json-3"><pre><code><span class="na">CONJURE_API</span> <span class="k">void</span> write_json(<span class="k">const</span> <a href="../math/glm-types.html#symbol-vec4">math::vec4</a> &amp;value, <a href="../json/writer/json-writer.html">json::writer</a> &amp;out);</code></pre></div>

<div class="function-declaration" id="function-reflect-write-json-4"><pre><code><span class="na">CONJURE_API</span> <span class="k">void</span> write_json(<span class="k">const</span> <a href="../math/glm-types.html#symbol-quat">math::quat</a> &amp;value, <a href="../json/writer/json-writer.html">json::writer</a> &amp;out);</code></pre></div>

<div class="function-declaration" id="function-reflect-write-json-5"><pre><code><span class="na">CONJURE_API</span> <span class="k">void</span> write_json(<span class="k">const</span> <a href="../math/glm-types.html#symbol-mat4">math::mat4</a> &amp;value, <a href="../json/writer/json-writer.html">json::writer</a> &amp;out);</code></pre></div>

<div class="function-declaration" id="function-reflect-write-json-6"><pre><code><span class="na">CONJURE_API</span> <span class="k">void</span> write_json(<span class="k">const</span> <a href="../math/transform/transform.html">math::transform</a> &amp;value, <a href="../json/writer/json-writer.html">json::writer</a> &amp;out);</code></pre></div>

<div class="function-declaration" id="function-reflect-write-json-7"><pre><code><span class="na">CONJURE_API</span> <span class="k">void</span> write_json(<span class="k">const</span> <a href="../string-id/string-id.html">string_id</a> &amp;value, <a href="../json/writer/json-writer.html">json::writer</a> &amp;out);</code></pre></div>

<div class="function-declaration" id="function-reflect-write-json-8"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../result/result.html">result</a>&lt;<span class="k">void</span>&gt; write_json(<a href="json-conversion/reflect-field-ref.html">field_ref</a> ref, <a href="../json/writer/json-writer.html">json::writer</a> &amp;out);</code></pre></div>

<p>Writes the referenced value (one field, or one array element) into <code>out</code>. Errors name the reference&#x27;s path (&quot;field &#x27;counts[1]&#x27;&quot;).</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="classes"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="field_ref"></a>
<a id="field_refdata"></a>
<a id="field_reffield"></a>
<a id="field_refis_valid"></a>
<a id="field_refiselement"></a>
<a id="field_refkind"></a>
<a id="field_refpath"></a>
<a id="field_refsize"></a>
<a id="from_json"></a>
<a id="get_as_json"></a>
<a id="internals-and-algorithms"></a>
<a id="json_converter"></a>
<a id="json_converter_1"></a>
<a id="json_converter_2"></a>
<a id="json_converter_3"></a>
<a id="json_converter_4"></a>
<a id="json_converter_5"></a>
<a id="json_converter_6"></a>
<a id="json_converterfrom_json"></a>
<a id="json_converterfrom_json_1"></a>
<a id="json_converterfrom_json_2"></a>
<a id="json_converterfrom_json_3"></a>
<a id="json_converterfrom_json_4"></a>
<a id="json_converterfrom_json_5"></a>
<a id="json_converterfrom_json_6"></a>
<a id="jsonjson_convertermathmat4"></a>
<a id="jsonjson_convertermathmat4from_json"></a>
<a id="jsonjson_convertermathquat"></a>
<a id="jsonjson_convertermathquatfrom_json"></a>
<a id="jsonjson_convertermathtransform"></a>
<a id="jsonjson_convertermathtransformfrom_json"></a>
<a id="jsonjson_convertermathvec2"></a>
<a id="jsonjson_convertermathvec2from_json"></a>
<a id="jsonjson_convertermathvec3"></a>
<a id="jsonjson_convertermathvec3from_json"></a>
<a id="jsonjson_convertermathvec4"></a>
<a id="jsonjson_convertermathvec4from_json"></a>
<a id="jsonjson_converterstring_id"></a>
<a id="jsonjson_converterstring_idfrom_json"></a>
<a id="reflectfield_ref"></a>
<a id="reflectfield_refdata"></a>
<a id="reflectfield_reffield"></a>
<a id="reflectfield_refis_valid"></a>
<a id="reflectfield_refiselement"></a>
<a id="reflectfield_refkind"></a>
<a id="reflectfield_refpath"></a>
<a id="reflectfield_refsize"></a>
<a id="reflectfrom_json"></a>
<a id="reflectget_as_json"></a>
<a id="reflectresolve_path"></a>
<a id="reflectset_from_json"></a>
<a id="reflectto_json"></a>
<a id="reflectto_json_document"></a>
<a id="reflectwrite_json"></a>
<a id="resolve_path"></a>
<a id="set_from_json"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="to_json"></a>
<a id="to_json_document"></a>
<a id="variables-and-properties"></a>
<a id="write_json"></a>
<!-- doc-legacy-anchors:end -->

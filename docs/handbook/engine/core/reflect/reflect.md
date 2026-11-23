---
kind: header
---

[Engine](../../../engine.md) → [Core](../../core.md)

# reflect.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/core/reflect/reflect.h` |
| API ↗ | [Reflection](../../../../api/core/reflection.md) |

<p>Umbrella header of the reflect area plus the description macros.</p>

## Functions

### Public

#### `reflect::detail::build_enum_description` {#function-reflect-detail-build-enum-description}

<div class="function-declaration" id="function-reflect-detail-build-enum-description-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> EnumType&gt; [[<span class="na">nodiscard</span>]] <a href="field-info/reflect-enum-info.html">enum_info</a> build_enum_description( std::string_view name, <span class="k">bool</span> isFlags, std::initializer_list&lt;std::pair&lt;std::string, <a href="../types.html#symbol-i64">i64</a>&gt;&gt; values );</code></pre></div>

<p>Assembles the <code><a href="field-info/reflect-enum-info.html">enum_info</a></code> of a macro-described enumeration.</p>

#### `reflect::detail::build_struct_description` {#function-reflect-detail-build-struct-description}

<div class="function-declaration" id="function-reflect-detail-build-struct-description-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> StructType&gt; [[<span class="na">nodiscard</span>]] <a href="type-info/reflect-type-info.html">type_info</a> build_struct_description(std::string_view name, std::initializer_list&lt;<a href="field-info/reflect-field-info.html">field_info</a>&gt; fields);</code></pre></div>

<p>Assembles the <code><a href="type-info/reflect-type-info.html">type_info</a></code> of a macro-described struct.</p>

#### `reflect::detail::describe_value_type` {#function-reflect-detail-describe-value-type}

<div class="function-declaration" id="function-reflect-detail-describe-value-type-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> ValueType&gt; <span class="k">void</span> describe_value_type(<a href="field-info/reflect-field-info.html">field_info</a> &amp;field, <span class="k">bool</span> asElement);</code></pre></div>

<p>Fills kind, type and the struct/enum description pointer for one value type. <code>asElement</code> targets elementKind instead of kind (array elements).</p>

#### `reflect::detail::fill_default_values` {#function-reflect-detail-fill-default-values}

<div class="function-declaration" id="function-reflect-detail-fill-default-values-1"><pre><code><span class="na">CONJURE_API</span> <span class="k">void</span> fill_default_values(<a href="type-info/reflect-type-info.html">type_info</a> &amp;info, <span class="k">const</span> <span class="k">void</span> *defaults);</code></pre></div>

<p>Serialises every field of <code>defaults</code> (an instance of the described type) into <code>field_info::defaultValueJson</code>.</p>

#### `reflect::detail::finish_struct_description` {#function-reflect-detail-finish-struct-description}

<div class="function-declaration" id="function-reflect-detail-finish-struct-description-1"><pre><code><span class="na">CONJURE_API</span> <span class="k">void</span> finish_struct_description(<a href="type-info/reflect-type-info.html">type_info</a> &amp;info);</code></pre></div>

<p>Validates a macro-built description; an inconsistent layout is a programming error and asserts.</p>

#### `reflect::detail::make_field` {#function-reflect-detail-make-field}

<div class="function-declaration" id="function-reflect-detail-make-field-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> MemberType&gt; [[<span class="na">nodiscard</span>]] <a href="field-info/reflect-field-info.html">field_info</a> make_field(std::string_view name, <a href="../types.html#symbol-usize">usize</a> offset, <a href="field-info/reflect-field-attributes.html">field_attributes</a> attributes);</code></pre></div>

<p>Builds the description of one member from its declared type, name and offset.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="build_enum_description"></a>
<a id="build_struct_description"></a>
<a id="classes"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="describe_value_type"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="fill_default_values"></a>
<a id="finish_struct_description"></a>
<a id="internals-and-algorithms"></a>
<a id="make_field"></a>
<a id="reflectdetailbuild_enum_description"></a>
<a id="reflectdetailbuild_struct_description"></a>
<a id="reflectdetaildescribe_value_type"></a>
<a id="reflectdetailfill_default_values"></a>
<a id="reflectdetailfinish_struct_description"></a>
<a id="reflectdetailmake_field"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->

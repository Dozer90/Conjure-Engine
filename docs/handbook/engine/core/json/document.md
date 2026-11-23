---
kind: header
---

[Engine](../../../engine.md) → [Core](../../core.md)

# document.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/core/json/document.h` |
| API ↗ | [JSON](../../../../api/core/json.md) |

<p>JSON documents.</p>

## Structs

| File | Description |
|---|---|
| [`json::init_member`](document/json-init-member.md) | One <code>{&quot;key&quot;, value}</code> entry of the <code>object()</code> builder. |

## Classes

| File | Description |
|---|---|
| [`json::document`](document/json-document.md) | An owning, mutable JSON document. |
| [`json::init_value`](document/json-init-value.md) | One argument of the <code>object()</code>/<code>array()</code> builders. Implicitly constructed from every scalar type the mutation API accepts, from a document (copied or moved) and from a value (copied). <code>A</code> <code>null</code> <code>C</code> string becomes JSON <code>null</code>, as in the mutation API. Intended only as a builder argument; do not store it. |

## Type Aliases

### `json::detail::widened_integer` {#symbol-json-detail-widened-integer}

```cpp
template <std::integral T> using widened_integer = std::conditional_t<std::is_signed_v<T>, i64, u64>;
```

<code><a href="../types.html#symbol-i64">i64</a></code> for signed integral types, <code><a href="../types.html#symbol-u64">u64</a></code> for unsigned ones; the storage width of the API.

### `json::init_value::storage` {#symbol-json-init-value-storage}

```cpp
using storage = std::variant<std::nullptr_t, bool, i64, u64, f64, std::string, document, value>;
```

## Concepts

### `json::detail::other_integer` {#symbol-json-detail-other-integer}

```cpp
template <typename T> concept other_integer = std::integral<T> && !std::same_as<T, bool> && !std::same_as<T, i32> && !std::same_as<T, u32> && !std::same_as<T, i64> && !std::same_as<T, u64>;
```

Integral types that are not bool and have no dedicated overload of their own.

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-json::MAX_NESTING_DEPTH"></a><code>json::MAX_NESTING_DEPTH</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../types.html#symbol-usize">usize</a></code> | <code class="signature"><span class="mi">1024</span></code> | Deepest container nesting a parsed document may have: &quot;[]&quot; is depth 1, &quot;[[]]&quot; depth 2, a scalar root depth 0. Sized so yyjson&#x27;s recursive copy fits a 512 KiB thread stack in Debug. |

## Functions

### Public

#### `json::array` {#function-json-array}

<div class="function-declaration" id="function-json-array-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="document/json-document.html">document</a> <a href="../reflect/field-info/reflect-detail-std-array-traits-std-array-elementtype-n.html">array</a>(std::initializer_list&lt;<a href="document/json-init-value.html">init_value</a>&gt; items);</code></pre></div>

<div class="function-declaration" id="function-json-array-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="document/json-document.html">document</a> <a href="../reflect/field-info/reflect-detail-std-array-traits-std-array-elementtype-n.html">array</a>(std::initializer_list&lt;<a href="document/json-init-value.html">init_value</a>&gt; items);</code></pre></div>

<p>Builds an array document: <code><a href="#function-json-array-1">json::array</a>({1, 2.5, &quot;three&quot;, nullptr})</code>.</p>

#### `json::object` {#function-json-object}

<div class="function-declaration" id="function-json-object-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="document/json-document.html">document</a> object(std::initializer_list&lt;<a href="document/json-init-member.html">init_member</a>&gt; members);</code></pre></div>

<div class="function-declaration" id="function-json-object-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="document/json-document.html">document</a> object(std::initializer_list&lt;<a href="document/json-init-member.html">init_member</a>&gt; members);</code></pre></div>

<p>Builds an object document: <code><a href="#function-json-object-1">json::object</a>({{&quot;a&quot;, 1}, {&quot;b&quot;, &quot;x&quot;}, {&quot;c&quot;, <a href="#function-json-array-1">json::array</a>({1, 2})}})</code>.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="array"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="document"></a>
<a id="documentarray"></a>
<a id="documentarray_root"></a>
<a id="documentat_pointer"></a>
<a id="documentclear"></a>
<a id="documentdocument"></a>
<a id="documentdocument_1"></a>
<a id="documenterase"></a>
<a id="documentfrom_value"></a>
<a id="documentinit_value"></a>
<a id="documentmerge_patch"></a>
<a id="documentmhandle"></a>
<a id="documentobject"></a>
<a id="documentobject_root"></a>
<a id="documentoperator"></a>
<a id="documentoperator_1"></a>
<a id="documentparse"></a>
<a id="documentparse_file"></a>
<a id="documentpush_back"></a>
<a id="documentpush_node"></a>
<a id="documentroot"></a>
<a id="documentroot_node"></a>
<a id="documentset"></a>
<a id="documentset_node"></a>
<a id="documentset_null"></a>
<a id="documentset_pointer"></a>
<a id="documentset_pointer_node"></a>
<a id="documentto_canonical_string"></a>
<a id="documentto_string"></a>
<a id="documentwrite_file"></a>
<a id="extension-points"></a>
<a id="init_member"></a>
<a id="init_memberitem"></a>
<a id="init_memberkey"></a>
<a id="init_value"></a>
<a id="init_valuearray"></a>
<a id="init_valueinit_value"></a>
<a id="init_valueinit_value_1"></a>
<a id="init_valuemake_node"></a>
<a id="init_valuemstorage"></a>
<a id="init_valueobject"></a>
<a id="init_valueoperator"></a>
<a id="init_valuestorage"></a>
<a id="init_valueto_document"></a>
<a id="internals-and-algorithms"></a>
<a id="jsonarray"></a>
<a id="jsondetailother_integer"></a>
<a id="jsondetailwidened_integer"></a>
<a id="jsondocument"></a>
<a id="jsondocumentarray"></a>
<a id="jsondocumentarray_root"></a>
<a id="jsondocumentat_pointer"></a>
<a id="jsondocumentclear"></a>
<a id="jsondocumentdocument"></a>
<a id="jsondocumentdocument_1"></a>
<a id="jsondocumenterase"></a>
<a id="jsondocumentfrom_value"></a>
<a id="jsondocumentinit_value"></a>
<a id="jsondocumentmerge_patch"></a>
<a id="jsondocumentmhandle"></a>
<a id="jsondocumentobject"></a>
<a id="jsondocumentobject_root"></a>
<a id="jsondocumentoperator"></a>
<a id="jsondocumentoperator_1"></a>
<a id="jsondocumentparse"></a>
<a id="jsondocumentparse_file"></a>
<a id="jsondocumentpush_back"></a>
<a id="jsondocumentpush_node"></a>
<a id="jsondocumentroot"></a>
<a id="jsondocumentroot_node"></a>
<a id="jsondocumentset"></a>
<a id="jsondocumentset_node"></a>
<a id="jsondocumentset_null"></a>
<a id="jsondocumentset_pointer"></a>
<a id="jsondocumentset_pointer_node"></a>
<a id="jsondocumentto_canonical_string"></a>
<a id="jsondocumentto_string"></a>
<a id="jsondocumentwrite_file"></a>
<a id="jsoninit_member"></a>
<a id="jsoninit_memberitem"></a>
<a id="jsoninit_memberkey"></a>
<a id="jsoninit_value"></a>
<a id="jsoninit_valuearray"></a>
<a id="jsoninit_valueinit_value"></a>
<a id="jsoninit_valueinit_value_1"></a>
<a id="jsoninit_valuemake_node"></a>
<a id="jsoninit_valuemstorage"></a>
<a id="jsoninit_valueobject"></a>
<a id="jsoninit_valueoperator"></a>
<a id="jsoninit_valuestorage"></a>
<a id="jsoninit_valueto_document"></a>
<a id="jsonmax_nesting_depth"></a>
<a id="jsonobject"></a>
<a id="max_nesting_depth"></a>
<a id="object"></a>
<a id="other-declarations"></a>
<a id="other_integer"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<a id="widened_integer"></a>
<!-- doc-legacy-anchors:end -->

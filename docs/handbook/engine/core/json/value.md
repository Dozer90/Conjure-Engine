---
kind: header
---

[Engine](../../../engine.md) → [Core](../../core.md)

# value.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/core/json/value.h` |
| API ↗ | [JSON](../../../../api/core/json.md) |

<p>JSON values.</p>

## Structs

| File | Description |
|---|---|
| [`json::detail::value_access`](value/json-detail-value-access.md) | Lets the document and the iterators build views; not for general use. |
| [`json::json_converter<bool>`](value/json-json-converter-bool.md) |  |
| [`json::json_converter<f32>`](value/json-json-converter-f32.md) |  |
| [`json::json_converter<f64>`](value/json-json-converter-f64.md) |  |
| [`json::json_converter<i16>`](value/json-json-converter-i16.md) |  |
| [`json::json_converter<i32>`](value/json-json-converter-i32.md) |  |
| [`json::json_converter<i64>`](value/json-json-converter-i64.md) |  |
| [`json::json_converter<i8>`](value/json-json-converter-i8.md) |  |
| [`json::json_converter<std::string>`](value/json-json-converter-std-string.md) |  |
| [`json::json_converter<std::vector<T>>`](value/json-json-converter-std-vector-t.md) | Converts a JSON array element by element. The first failing element stops the conversion and its error is returned with the element index prepended to the message. |
| [`json::json_converter<u16>`](value/json-json-converter-u16.md) |  |
| [`json::json_converter<u32>`](value/json-json-converter-u32.md) |  |
| [`json::json_converter<u64>`](value/json-json-converter-u64.md) |  |
| [`json::json_converter<u8>`](value/json-json-converter-u8.md) |  |
| [`json::json_converter<uuid>`](value/json-json-converter-uuid.md) |  |
| [`json::member`](value/json-member.md) | One key/value pair produced by iterating an object. |
| [`std::formatter<conjure::core::json::json_type>`](value/std-formatter-conjure-core-json-json-type.md) |  |

## Classes

| File | Description |
|---|---|
| [`json::value`](value/json-value.md) | <code>A</code> non-owning view of one JSON value inside a document. |
| [`json::value::array_iterator`](value/json-value-array-iterator.md) | Forward iterator over the elements of an array. Dereferencing yields a value view. |
| [`json::value::object_iterator`](value/json-value-object-iterator.md) | Forward iterator over the members of an object. Dereferencing yields a member (key view plus value view). |
| [`json::value::object_range`](value/json-value-object-range.md) | Range returned by <code>members()</code>: <code>for (auto [key, item] : object.members())</code>. |

## Enumerations

### `json::json_type` {#symbol-json-json-type}

```cpp
enum class json_type : u8 {
    NULL_VALUE,
    BOOL,
    NUMBER,
    STRING,
    ARRAY,
    OBJECT
};
```

The six JSON value kinds. Invalid values report <code>NULL_VALUE</code>.

## Type Aliases

### `json::value::array_iterator::difference_type` {#symbol-json-value-array-iterator-difference-type}

```cpp
using difference_type = isize;
```

### `json::value::array_iterator::iterator_category` {#symbol-json-value-array-iterator-iterator-category}

```cpp
using iterator_category = std::forward_iterator_tag;
```

### `json::value::array_iterator::pointer` {#symbol-json-value-array-iterator-pointer}

```cpp
using pointer = void;
```

### `json::value::array_iterator::reference` {#symbol-json-value-array-iterator-reference}

```cpp
using reference = value;
```

### `json::value::array_iterator::value_type` {#symbol-json-value-array-iterator-value-type}

```cpp
using value_type = value;
```

### `json::value::object_iterator::difference_type` {#symbol-json-value-object-iterator-difference-type}

```cpp
using difference_type = isize;
```

### `json::value::object_iterator::iterator_category` {#symbol-json-value-object-iterator-iterator-category}

```cpp
using iterator_category = std::forward_iterator_tag;
```

### `json::value::object_iterator::pointer` {#symbol-json-value-object-iterator-pointer}

```cpp
using pointer = void;
```

### `json::value::object_iterator::reference` {#symbol-json-value-object-iterator-reference}

```cpp
using reference = member;
```

### `json::value::object_iterator::value_type` {#symbol-json-value-object-iterator-value-type}

```cpp
using value_type = member;
```

## Concepts

### `json::json_convertible` {#symbol-json-json-convertible}

```cpp
template <typename T> concept json_convertible = requires(const value &source) { { json_converter<T>::from_json(source) } -> std::same_as<result<T>>; };
```

True when <code><a href="value/json-json-converter-bool.html">json_converter</a></code> is specialised for <code>T</code>.

## Forward Declarations

### `json::document` {#symbol-json-document}

```cpp
class document;
```

### `json::json_converter` {#symbol-json-json-converter}

```cpp
template <typename T> struct json_converter;
```

Customisation point for <code>value::to&lt;T&gt;()</code>. Specialise it for a type and provide <code>static result&lt;T&gt; from_json(const value &amp;source)</code>. Conversion errors use the generic core codes: <code>NOT_FOUND</code> for an invalid (missing) value, <code>INVALID_ARGUMENT</code> for a type mismatch, <code>OUT_OF_RANGE</code> for a number outside the target range and <code>PARSE_FAILURE</code> for a string that does not parse as the target.

## Functions

### Public

#### `json::to_string` {#function-json-to-string}

<div class="function-declaration" id="function-json-to-string-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(<a href="value.html#symbol-json-json-type">json_type</a> type) <span class="k">noexcept</span>;</code></pre></div>

<p>Returns the lower-case name of a type (&quot;object&quot;), used in error messages.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="document"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="json_converter"></a>
<a id="json_converter_1"></a>
<a id="json_converter_10"></a>
<a id="json_converter_11"></a>
<a id="json_converter_12"></a>
<a id="json_converter_13"></a>
<a id="json_converter_14"></a>
<a id="json_converter_2"></a>
<a id="json_converter_3"></a>
<a id="json_converter_4"></a>
<a id="json_converter_5"></a>
<a id="json_converter_6"></a>
<a id="json_converter_7"></a>
<a id="json_converter_8"></a>
<a id="json_converter_9"></a>
<a id="json_converterfrom_json"></a>
<a id="json_converterfrom_json_1"></a>
<a id="json_converterfrom_json_10"></a>
<a id="json_converterfrom_json_11"></a>
<a id="json_converterfrom_json_12"></a>
<a id="json_converterfrom_json_13"></a>
<a id="json_converterfrom_json_2"></a>
<a id="json_converterfrom_json_3"></a>
<a id="json_converterfrom_json_4"></a>
<a id="json_converterfrom_json_5"></a>
<a id="json_converterfrom_json_6"></a>
<a id="json_converterfrom_json_7"></a>
<a id="json_converterfrom_json_8"></a>
<a id="json_converterfrom_json_9"></a>
<a id="json_convertible"></a>
<a id="json_type"></a>
<a id="json_typearray"></a>
<a id="json_typebool"></a>
<a id="json_typenull_value"></a>
<a id="json_typenumber"></a>
<a id="json_typeobject"></a>
<a id="json_typestring"></a>
<a id="jsondetailvalue_access"></a>
<a id="jsondetailvalue_accessmake"></a>
<a id="jsondetailvalue_accessnode"></a>
<a id="jsondocument"></a>
<a id="jsonjson_converter"></a>
<a id="jsonjson_converterbool"></a>
<a id="jsonjson_converterboolfrom_json"></a>
<a id="jsonjson_converterf32"></a>
<a id="jsonjson_converterf32from_json"></a>
<a id="jsonjson_converterf64"></a>
<a id="jsonjson_converterf64from_json"></a>
<a id="jsonjson_converteri16"></a>
<a id="jsonjson_converteri16from_json"></a>
<a id="jsonjson_converteri32"></a>
<a id="jsonjson_converteri32from_json"></a>
<a id="jsonjson_converteri64"></a>
<a id="jsonjson_converteri64from_json"></a>
<a id="jsonjson_converteri8"></a>
<a id="jsonjson_converteri8from_json"></a>
<a id="jsonjson_converterstdstring"></a>
<a id="jsonjson_converterstdstringfrom_json"></a>
<a id="jsonjson_converterstdvectort"></a>
<a id="jsonjson_converterstdvectortfrom_json"></a>
<a id="jsonjson_converteru16"></a>
<a id="jsonjson_converteru16from_json"></a>
<a id="jsonjson_converteru32"></a>
<a id="jsonjson_converteru32from_json"></a>
<a id="jsonjson_converteru64"></a>
<a id="jsonjson_converteru64from_json"></a>
<a id="jsonjson_converteru8"></a>
<a id="jsonjson_converteru8from_json"></a>
<a id="jsonjson_converteruuid"></a>
<a id="jsonjson_converteruuidfrom_json"></a>
<a id="jsonjson_convertible"></a>
<a id="jsonjson_type"></a>
<a id="jsonjson_typearray"></a>
<a id="jsonjson_typebool"></a>
<a id="jsonjson_typenull_value"></a>
<a id="jsonjson_typenumber"></a>
<a id="jsonjson_typeobject"></a>
<a id="jsonjson_typestring"></a>
<a id="jsonmember"></a>
<a id="jsonmemberitem"></a>
<a id="jsonmemberkey"></a>
<a id="jsonto_string"></a>
<a id="jsonvalue"></a>
<a id="jsonvaluearray_iterator"></a>
<a id="jsonvaluearray_iteratorarray_iterator"></a>
<a id="jsonvaluearray_iteratordifference_type"></a>
<a id="jsonvaluearray_iteratoriterator_category"></a>
<a id="jsonvaluearray_iteratormnode"></a>
<a id="jsonvaluearray_iteratormowner"></a>
<a id="jsonvaluearray_iteratormremaining"></a>
<a id="jsonvaluearray_iteratoroperator"></a>
<a id="jsonvaluearray_iteratoroperator_1"></a>
<a id="jsonvaluearray_iteratoroperator_2"></a>
<a id="jsonvaluearray_iteratorpointer"></a>
<a id="jsonvaluearray_iteratorreference"></a>
<a id="jsonvaluearray_iteratorvalue"></a>
<a id="jsonvaluearray_iteratorvalue_type"></a>
<a id="jsonvalueas_bool"></a>
<a id="jsonvalueas_f32"></a>
<a id="jsonvalueas_f64"></a>
<a id="jsonvalueas_i64"></a>
<a id="jsonvalueas_string"></a>
<a id="jsonvalueas_u64"></a>
<a id="jsonvaluebegin"></a>
<a id="jsonvaluecontains"></a>
<a id="jsonvalueend"></a>
<a id="jsonvalueget"></a>
<a id="jsonvalueis_array"></a>
<a id="jsonvalueis_bool"></a>
<a id="jsonvalueis_integer"></a>
<a id="jsonvalueis_null"></a>
<a id="jsonvalueis_number"></a>
<a id="jsonvalueis_object"></a>
<a id="jsonvalueis_string"></a>
<a id="jsonvalueis_valid"></a>
<a id="jsonvaluemembers"></a>
<a id="jsonvaluemnode"></a>
<a id="jsonvaluemowner"></a>
<a id="jsonvalueobject_iterator"></a>
<a id="jsonvalueobject_iteratordifference_type"></a>
<a id="jsonvalueobject_iteratoriterator_category"></a>
<a id="jsonvalueobject_iteratormkey"></a>
<a id="jsonvalueobject_iteratormowner"></a>
<a id="jsonvalueobject_iteratormremaining"></a>
<a id="jsonvalueobject_iteratorobject_iterator"></a>
<a id="jsonvalueobject_iteratoroperator"></a>
<a id="jsonvalueobject_iteratoroperator_1"></a>
<a id="jsonvalueobject_iteratoroperator_2"></a>
<a id="jsonvalueobject_iteratorpointer"></a>
<a id="jsonvalueobject_iteratorreference"></a>
<a id="jsonvalueobject_iteratorvalue"></a>
<a id="jsonvalueobject_iteratorvalue_type"></a>
<a id="jsonvalueobject_range"></a>
<a id="jsonvalueobject_rangebegin"></a>
<a id="jsonvalueobject_rangeend"></a>
<a id="jsonvalueobject_rangembegin"></a>
<a id="jsonvalueobject_rangeobject_range"></a>
<a id="jsonvalueobject_rangevalue"></a>
<a id="jsonvalueoperator"></a>
<a id="jsonvalueoperator_1"></a>
<a id="jsonvalueowner"></a>
<a id="jsonvaluesize"></a>
<a id="jsonvalueto"></a>
<a id="jsonvaluetype"></a>
<a id="jsonvaluevalue"></a>
<a id="jsonvaluevalue_access"></a>
<a id="member"></a>
<a id="memberitem"></a>
<a id="memberkey"></a>
<a id="other-declarations"></a>
<a id="std"></a>
<a id="stdformat"></a>
<a id="stdformatterconjurecorejsonjson_type"></a>
<a id="stdformatterconjurecorejsonjson_typeformat"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="to_string"></a>
<a id="value"></a>
<a id="value_access"></a>
<a id="value_accessmake"></a>
<a id="value_accessnode"></a>
<a id="valuearray_iterator"></a>
<a id="valuearray_iteratorarray_iterator"></a>
<a id="valuearray_iteratordifference_type"></a>
<a id="valuearray_iteratoriterator_category"></a>
<a id="valuearray_iteratormnode"></a>
<a id="valuearray_iteratormowner"></a>
<a id="valuearray_iteratormremaining"></a>
<a id="valuearray_iteratoroperator"></a>
<a id="valuearray_iteratoroperator_1"></a>
<a id="valuearray_iteratoroperator_2"></a>
<a id="valuearray_iteratorpointer"></a>
<a id="valuearray_iteratorreference"></a>
<a id="valuearray_iteratorvalue"></a>
<a id="valuearray_iteratorvalue_type"></a>
<a id="valueas_bool"></a>
<a id="valueas_f32"></a>
<a id="valueas_f64"></a>
<a id="valueas_i64"></a>
<a id="valueas_string"></a>
<a id="valueas_u64"></a>
<a id="valuebegin"></a>
<a id="valuecontains"></a>
<a id="valueend"></a>
<a id="valueget"></a>
<a id="valueis_array"></a>
<a id="valueis_bool"></a>
<a id="valueis_integer"></a>
<a id="valueis_null"></a>
<a id="valueis_number"></a>
<a id="valueis_object"></a>
<a id="valueis_string"></a>
<a id="valueis_valid"></a>
<a id="valuemembers"></a>
<a id="valuemnode"></a>
<a id="valuemowner"></a>
<a id="valueobject_iterator"></a>
<a id="valueobject_iteratordifference_type"></a>
<a id="valueobject_iteratoriterator_category"></a>
<a id="valueobject_iteratormkey"></a>
<a id="valueobject_iteratormowner"></a>
<a id="valueobject_iteratormremaining"></a>
<a id="valueobject_iteratorobject_iterator"></a>
<a id="valueobject_iteratoroperator"></a>
<a id="valueobject_iteratoroperator_1"></a>
<a id="valueobject_iteratoroperator_2"></a>
<a id="valueobject_iteratorpointer"></a>
<a id="valueobject_iteratorreference"></a>
<a id="valueobject_iteratorvalue"></a>
<a id="valueobject_iteratorvalue_type"></a>
<a id="valueobject_range"></a>
<a id="valueobject_rangebegin"></a>
<a id="valueobject_rangeend"></a>
<a id="valueobject_rangembegin"></a>
<a id="valueobject_rangeobject_range"></a>
<a id="valueobject_rangevalue"></a>
<a id="valueoperator"></a>
<a id="valueoperator_1"></a>
<a id="valueowner"></a>
<a id="valuesize"></a>
<a id="valueto"></a>
<a id="valuetype"></a>
<a id="valuevalue"></a>
<a id="valuevalue_access"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->

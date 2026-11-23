---
kind: class
---

[Engine](../../../../engine.md) → [Core](../../../core.md) → [value.h](../value.md)

# json::value
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::core::json` |
| Include File Path | `engine/include/conjure/core/json/value.h` |
| Source File Path | `engine/src/core/json/value.cpp` |
| API ↗ | [JSON](../../../../../api/core/json.md) |

## Syntax

```cpp
class CONJURE_API value
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature"><span class="k">constexpr</span> value(
    <span class="k">void</span> *node,
    <span class="k">const</span> <a href="../document/json-document.html">document</a> *owner) <span class="k">noexcept</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature"><span class="k">constexpr</span> value() <span class="k">noexcept</span> = <span class="k">default</span>;</code></td><td>An invalid value: not part of any document, every accessor returns its fallback.</td></tr>
</tbody>
</table>



## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mNode</code></td><td><code class="signature"><span class="k">void</span> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mOwner</code></td><td><code class="signature"><span class="k">const</span> <a href="../document/json-document.html">document</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `as_bool` {#function-as-bool}

<div class="function-declaration" id="function-as-bool-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> as_bool(
    <span class="k">bool</span> fallback = false) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The boolean, or <code>fallback</code> when this is not a bool.</p>

#### `as_f32` {#function-as-f32}

<div class="function-declaration" id="function-as-f32-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-f32">f32</a> as_f32(
    <a href="../../types.html#symbol-f32">f32</a> fallback = <span class="mi">0.0</span>f) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Any number as <code><a href="../../types.html#symbol-f32">f32</a></code> (rounded), or <code>fallback</code> when this is not a number or its magnitude exceeds the <code><a href="../../types.html#symbol-f32">f32</a></code> range (the cast would be undefined behaviour).</p>

#### `as_f64` {#function-as-f64}

<div class="function-declaration" id="function-as-f64-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-f64">f64</a> as_f64(
    <a href="../../types.html#symbol-f64">f64</a> fallback = <span class="mi">0.0</span>) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Any number as <code><a href="../../types.html#symbol-f64">f64</a></code> (large integers round), or <code>fallback</code> when this is not a number.</p>

#### `as_i64` {#function-as-i64}

<div class="function-declaration" id="function-as-i64-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-i64">i64</a> as_i64(
    <a href="../../types.html#symbol-i64">i64</a> fallback = <span class="mi">0</span>) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The number as <code><a href="../../types.html#symbol-i64">i64</a></code>. Integers outside the <code><a href="../../types.html#symbol-i64">i64</a></code> range and non-integral reals yield <code>fallback</code>; integral reals convert.</p>

#### `as_string` {#function-as-string}

<div class="function-declaration" id="function-as-string-1"><pre><code>[[<span class="na">nodiscard</span>]] std::string_view as_string(
    std::string_view fallback = {}) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The string bytes (UTF-8, may contain embedded zeros), or <code>fallback</code> when this is not a string. The view lives as long as the document.</p>

#### `as_u64` {#function-as-u64}

<div class="function-declaration" id="function-as-u64-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-u64">u64</a> as_u64(
    <a href="../../types.html#symbol-u64">u64</a> fallback = <span class="mi">0</span>) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The number as <code><a href="../../types.html#symbol-u64">u64</a></code>. Negative values and non-integral reals yield <code>fallback</code>.</p>

#### `begin` {#function-begin}

<div class="function-declaration" id="function-begin-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="json-value-array-iterator.html">array_iterator</a> begin() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Iteration over array elements: <code>for (value item : array)</code>. Empty for non-arrays.</p>

#### `contains` {#function-contains}

<div class="function-declaration" id="function-contains-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> contains(
    std::string_view key) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when this is an object with a member named <code>key</code>.</p>

#### `end` {#function-end}

<div class="function-declaration" id="function-end-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="json-value-array-iterator.html">array_iterator</a> end() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `get` {#function-get}

<div class="function-declaration" id="function-get-1"><pre><code>[[<span class="na">nodiscard</span>]] std::optional&lt;<a href="json-value.html">value</a>&gt; get(
    std::string_view key) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The member named <code>key</code>, or nullopt when missing or not an object.</p>

#### `is_array` {#function-is-array}

<div class="function-declaration" id="function-is-array-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_array() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `is_bool` {#function-is-bool}

<div class="function-declaration" id="function-is-bool-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_bool() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `is_integer` {#function-is-integer}

<div class="function-declaration" id="function-is-integer-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_integer() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True for a number written without fraction or exponent (fits <code><a href="../../types.html#symbol-i64">i64</a></code> or <code><a href="../../types.html#symbol-u64">u64</a></code>). <code>3.0</code> is a number but not an integer.</p>

#### `is_null` {#function-is-null}

<div class="function-declaration" id="function-is-null-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_null() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True for a JSON <code>null</code> and for an invalid value. Use <code><a href="#function-is-valid-1">is_valid</a>()</code> to tell them apart.</p>

#### `is_number` {#function-is-number}

<div class="function-declaration" id="function-is-number-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_number() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `is_object` {#function-is-object}

<div class="function-declaration" id="function-is-object-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_object() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `is_string` {#function-is-string}

<div class="function-declaration" id="function-is-string-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_string() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `is_valid` {#function-is-valid}

<div class="function-declaration" id="function-is-valid-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_valid() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>False for a default-constructed value or the result of a failed lookup.</p>

#### `members` {#function-members}

<div class="function-declaration" id="function-members-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="json-value-object-range.html">object_range</a> members() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Iteration over object members in document order. Empty for non-objects.</p>

#### `owner` {#function-owner}

<div class="function-declaration" id="function-owner-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../document/json-document.html">document</a> *owner() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The document this view points into, or <code>nullptr</code> when invalid. Informational only; stale once the document has been moved.</p>

#### `size` {#function-size}

<div class="function-declaration" id="function-size-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-usize">usize</a> size() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Number of elements (array) or members (object); 0 for anything else.</p>

#### `to` {#function-to}

<div class="function-declaration" id="function-to-1"><pre><code><span class="k">template</span> &lt;<a href="../value.html#symbol-json-json-convertible">json_convertible</a> T&gt; [[<span class="na">nodiscard</span>]] <a href="../../result/result.html">result</a>&lt;T&gt; to() <span class="k">const</span>;</code></pre></div>

<p>Converts through <code><a href="json-json-converter-bool.html">json_converter</a>&lt;T&gt;</code>; see that template for the error codes.</p>

#### `type` {#function-type}

<div class="function-declaration" id="function-type-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../value.html#symbol-json-json-type">json_type</a> type() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The kind of this value. Invalid values report <code>NULL_VALUE</code>.</p>

## Operator Overloads

### Public

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="json-value.html">value</a> &amp;other) <span class="k">const</span>;</code></pre></div>

<p>Structural equality, defined as &quot;same canonical text&quot;: objects compare regardless of member order (duplicate keys pair up in order); integers compare mathematically; reals compare bit for bit (so -0.0 differs from 0.0); an integer never equals a real. Two invalid values are equal; an invalid value never equals a valid one. Walks both trees with an explicit stack (one small allocation), so any nesting depth is safe.</p>

#### `operator[]` {#operator-5b--5d}

<div class="function-declaration" id="operator-5b--5d-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="json-value.html">value</a> <span class="k">operator</span>[](
    <a href="../../types.html#symbol-usize">usize</a> index) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Element <code>index</code> of an array, or an invalid value when out of range or not an array.</p>

<div class="function-declaration" id="operator-5b--5d-2"><pre><code>[[<span class="na">nodiscard</span>]] <a href="json-value.html">value</a> <span class="k">operator</span>[](
    std::string_view key) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The first member named <code>key</code> of an object, or an invalid value when missing or not an object. Never asserts.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->

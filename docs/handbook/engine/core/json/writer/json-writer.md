---
kind: class
---

[Engine](../../../../engine.md) → [Core](../../../core.md) → [writer.h](../writer.md)

# json::writer
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::core::json` |
| Include File Path | `engine/include/conjure/core/json/writer.h` |
| Source File Path | `engine/src/core/json/writer.cpp` |
| API ↗ | [JSON](../../../../../api/core/json.md) |

## Syntax

```cpp
class CONJURE_API writer
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">writer();</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">writer(
    <span class="k">const</span> <a href="json-writer.html">writer</a> &amp;) = <span class="k">default</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">writer(
    <a href="json-writer.html">writer</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~writer();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mBuffer</code></td><td><code class="signature">std::string</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mRootWritten</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mStack</code></td><td><code class="signature">std::vector&lt;<a href="json-writer-frame.html">frame</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `begin_array` {#function-begin-array}

<div class="function-declaration" id="function-begin-array-1"><pre><code><span class="k">void</span> begin_array();</code></pre></div>

<p>Opens an array as the root, an array element or the value of the pending key.</p>

#### `begin_object` {#function-begin-object}

<div class="function-declaration" id="function-begin-object-1"><pre><code><span class="k">void</span> begin_object();</code></pre></div>

<p>Opens an object as the root, an array element or the value of the pending key.</p>

#### `depth` {#function-depth}

<div class="function-declaration" id="function-depth-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-usize">usize</a> depth() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Number of open containers.</p>

#### `end_array` {#function-end-array}

<div class="function-declaration" id="function-end-array-1"><pre><code><span class="k">void</span> end_array();</code></pre></div>

<p>Closes the innermost array. Asserts when the innermost scope is not an array.</p>

#### `end_object` {#function-end-object}

<div class="function-declaration" id="function-end-object-1"><pre><code><span class="k">void</span> end_object();</code></pre></div>

<p>Closes the innermost object. Asserts when the innermost scope is not an object or a key is pending.</p>

#### `finish` {#function-finish}

<div class="function-declaration" id="function-finish-1"><pre><code>[[<span class="na">nodiscard</span>]] std::string finish();</code></pre></div>

<p>Returns the text and resets the writer. Asserts when containers are still open or no root was written (the partial text is still returned).</p>

#### `is_complete` {#function-is-complete}

<div class="function-declaration" id="function-is-complete-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_complete() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when exactly one root value has been written and every container is closed.</p>

#### `key` {#function-key}

<div class="function-declaration" id="function-key-1"><pre><code><span class="k">void</span> key(
    std::string_view name);</code></pre></div>

<p>Writes a member key. Asserts outside an object or when a key is already pending.</p>

#### `reset` {#function-reset}

<div class="function-declaration" id="function-reset-1"><pre><code><span class="k">void</span> reset();</code></pre></div>

<p>Discards everything written so far.</p>

#### `text` {#function-text}

<div class="function-declaration" id="function-text-1"><pre><code>[[<span class="na">nodiscard</span>]] std::string_view text() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The text written so far (possibly incomplete).</p>

#### `value` {#function-value}

<div class="function-declaration" id="function-value-1"><pre><code><span class="k">void</span> value(
    <span class="k">bool</span> item);</code></pre></div>

<p>Writes a scalar as the root, an array element or the value of the pending key. Non-finite reals assert and are written as <code>null</code>; a <code>null</code> <code>C</code> string writes <code>null</code>, as in the document API. Strings must be valid UTF-8 (asserted in Debug, written as-is otherwise). Integral types without an overload of their own widen to <code><a href="../../types.html#symbol-i64">i64</a></code> or <code><a href="../../types.html#symbol-u64">u64</a></code>.</p>

<div class="function-declaration" id="function-value-2"><pre><code><span class="k">void</span> value(
    <a href="../../types.html#symbol-i32">i32</a> item);</code></pre></div>

<div class="function-declaration" id="function-value-3"><pre><code><span class="k">void</span> value(
    <a href="../../types.html#symbol-u32">u32</a> item);</code></pre></div>

<div class="function-declaration" id="function-value-4"><pre><code><span class="k">void</span> value(
    <a href="../../types.html#symbol-i64">i64</a> item);</code></pre></div>

<div class="function-declaration" id="function-value-5"><pre><code><span class="k">void</span> value(
    <a href="../../types.html#symbol-u64">u64</a> item);</code></pre></div>

<div class="function-declaration" id="function-value-6"><pre><code><span class="k">void</span> value(
    <a href="../../types.html#symbol-f32">f32</a> item);</code></pre></div>

<div class="function-declaration" id="function-value-7"><pre><code><span class="k">void</span> value(
    <a href="../../types.html#symbol-f64">f64</a> item);</code></pre></div>

<div class="function-declaration" id="function-value-8"><pre><code><span class="k">void</span> value(
    std::string_view item);</code></pre></div>

<div class="function-declaration" id="function-value-9"><pre><code><span class="k">void</span> value(
    <span class="k">const</span> <span class="k">char</span> *item);</code></pre></div>

<div class="function-declaration" id="function-value-10"><pre><code><span class="k">void</span> value(
    std::nullptr_t);</code></pre></div>

<div class="function-declaration" id="function-value-11"><pre><code><span class="k">template</span> &lt;std::integral T&gt; <span class="k">requires</span> (!std::same_as&lt;T, <span class="k">bool</span>&gt; &amp;&amp; !std::same_as&lt;T, <a href="../../types.html#symbol-i32">i32</a>&gt; &amp;&amp; !std::same_as&lt;T, <a href="../../types.html#symbol-u32">u32</a>&gt; &amp;&amp; !std::same_as&lt;T, <a href="../../types.html#symbol-i64">i64</a>&gt; &amp;&amp; !std::same_as&lt;T, <a href="../../types.html#symbol-u64">u64</a>&gt;) <span class="k">void</span> value(
    T item);</code></pre></div>

#### `write_raw_json` {#function-write-raw-json}

<div class="function-declaration" id="function-write-raw-json-1"><pre><code><span class="k">void</span> write_raw_json(
    std::string_view rawJson);</code></pre></div>

<p>Inserts pre-serialised JSON verbatim where a value is expected. Debug builds parse the text and assert when it is not a single valid JSON value.</p>

### Private

#### `append_string` {#function-append-string}

<div class="function-declaration" id="function-append-string-1"><pre><code><span class="k">void</span> append_string(
    std::string_view text);</code></pre></div>

#### `begin_value` {#function-begin-value}

<div class="function-declaration" id="function-begin-value-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> begin_value();</code></pre></div>

## Operator Overloads

### Public

#### `operator=` {#operator-3d}

<div class="function-declaration" id="operator-3d-1"><pre><code><a href="json-writer.html">writer</a> &amp;<span class="k">operator</span>=(
    <span class="k">const</span> <a href="json-writer.html">writer</a> &amp;) = <span class="k">default</span>;</code></pre></div>

<div class="function-declaration" id="operator-3d-2"><pre><code><a href="json-writer.html">writer</a> &amp;<span class="k">operator</span>=(
    <a href="json-writer.html">writer</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

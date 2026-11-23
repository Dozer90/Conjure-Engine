---
kind: class
---

[Engine](../../../../engine.md) → [Core](../../../core.md) → [document.h](../document.md)

# json::init_value
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::core::json` |
| Include File Path | `engine/include/conjure/core/json/document.h` |
| Source File Path | `engine/src/core/json/document.cpp` |
| API ↗ | [JSON](../../../../../api/core/json.md) |

## Syntax

```cpp
class CONJURE_API init_value
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">init_value(
    std::nullptr_t) <span class="k">noexcept</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">init_value(
    <span class="k">bool</span> item) <span class="k">noexcept</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">init_value(
    <a href="../../types.html#symbol-i32">i32</a> item) <span class="k">noexcept</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">init_value(
    <a href="../../types.html#symbol-u32">u32</a> item) <span class="k">noexcept</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">init_value(
    <a href="../../types.html#symbol-i64">i64</a> item) <span class="k">noexcept</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">init_value(
    <a href="../../types.html#symbol-u64">u64</a> item) <span class="k">noexcept</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">init_value(
    <a href="../../types.html#symbol-f32">f32</a> item) <span class="k">noexcept</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">init_value(
    <a href="../../types.html#symbol-f64">f64</a> item) <span class="k">noexcept</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">init_value(
    <span class="k">const</span> <span class="k">char</span> *item);</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">init_value(
    std::string_view item);</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">init_value(
    <span class="k">const</span> std::string &amp;item);</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">init_value(
    std::string &amp;&amp;item) <span class="k">noexcept</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">init_value(
    <span class="k">const</span> <a href="json-document.html">document</a> &amp;item);</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">init_value(
    <a href="json-document.html">document</a> &amp;&amp;item) <span class="k">noexcept</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">init_value(
    <span class="k">const</span> <a href="../value/json-value.html">value</a> &amp;item) <span class="k">noexcept</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature"><span class="k">template</span> &lt;<a href="../document.html#symbol-json-detail-other-integer">detail::other_integer</a> T&gt; init_value(
    T item) <span class="k">noexcept</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">init_value(
    <span class="k">const</span> <a href="json-init-value.html">init_value</a> &amp;) = <span class="k">default</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">init_value(
    <a href="json-init-value.html">init_value</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~init_value();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mStorage</code></td><td><code class="signature"><a href="../document.html#symbol-json-init-value-storage">storage</a></code></td><td>—</td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `to_document` {#function-to-document}

<div class="function-declaration" id="function-to-document-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="json-document.html">document</a> to_document() <span class="k">const</span>;</code></pre></div>

<p>Builds a one-value document from the stored alternative.</p>

### Private

#### `array` {#function-array}

<div class="function-declaration" id="function-array-1"><pre><code><span class="k">friend</span> <span class="na">CONJURE_API</span> <a href="json-document.html">document</a> array(
    std::initializer_list&lt;<a href="json-init-value.html">init_value</a>&gt; items);</code></pre></div>

#### `make_node` {#function-make-node}

<div class="function-declaration" id="function-make-node-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">void</span> *make_node(
    <span class="k">void</span> *target) <span class="k">const</span>;</code></pre></div>

#### `object` {#function-object}

<div class="function-declaration" id="function-object-1"><pre><code><span class="k">friend</span> <span class="na">CONJURE_API</span> <a href="json-document.html">document</a> object(
    std::initializer_list&lt;<a href="json-init-member.html">init_member</a>&gt; members);</code></pre></div>

## Operator Overloads

### Public

#### `operator=` {#operator-3d}

<div class="function-declaration" id="operator-3d-1"><pre><code><a href="json-init-value.html">init_value</a> &amp;<span class="k">operator</span>=(
    <span class="k">const</span> <a href="json-init-value.html">init_value</a> &amp;) = <span class="k">default</span>;</code></pre></div>

<div class="function-declaration" id="operator-3d-2"><pre><code><a href="json-init-value.html">init_value</a> &amp;<span class="k">operator</span>=(
    <a href="json-init-value.html">init_value</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

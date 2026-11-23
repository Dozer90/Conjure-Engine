---
kind: class
---

[Engine](../../../engine.md) → [Core](../../core.md) → [result.h](../result.md)

# result
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::core` |
| Include File Path | `engine/include/conjure/core/result.h` |
| Source File Path | N/A |
| API ↗ | [Errors and results](../../../../api/core/errors-and-results.md) |

## Syntax

```cpp
template <typename ValueType> class [[nodiscard]] result
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">result(
    <span class="k">const</span> ValueType &amp;value);</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">result(
    ValueType &amp;&amp;value) <span class="k">noexcept</span>(std::is_nothrow_move_constructible_v&lt;ValueType&gt;);</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">result(
    <a href="../error/error.html">error</a> failure) <span class="k">noexcept</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">result(
    <span class="k">const</span> <a href="result.html">result</a> &amp;) = <span class="k">default</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">result(
    <a href="result.html">result</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~result() = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mStorage</code></td><td><code class="signature">std::<wbr>variant&lt;<wbr>Value<wbr>Type,<wbr> <a href="../error/error.html">error</a>&gt;</code></td><td>—</td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `get_error` {#function-get-error}

<div class="function-declaration" id="function-get-error-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../error/error.html">error</a> &amp;get_error() <span class="k">const</span> &amp; <span class="k">noexcept</span>;</code></pre></div>

<p>The held error. Calling this on a value <code><a href="result.html">result</a></code> is a fatal programmer error.</p>

<div class="function-declaration" id="function-get-error-2"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../error/error.html">error</a> &amp;&amp;get_error() &amp;&amp; <span class="k">noexcept</span>;</code></pre></div>

#### `has_value` {#function-has-value}

<div class="function-declaration" id="function-has-value-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> has_value() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when this <code><a href="result.html">result</a></code> holds a value rather than an error.</p>

#### `value` {#function-value}

<div class="function-declaration" id="function-value-1"><pre><code>[[<span class="na">nodiscard</span>]] ValueType &amp;value() &amp; <span class="k">noexcept</span>;</code></pre></div>

<p>The held value. Calling this on an error <code><a href="result.html">result</a></code> is a fatal programmer error.</p>

<div class="function-declaration" id="function-value-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> ValueType &amp;value() <span class="k">const</span> &amp; <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-value-3"><pre><code>[[<span class="na">nodiscard</span>]] ValueType &amp;&amp;value() &amp;&amp; <span class="k">noexcept</span>;</code></pre></div>

#### `value_or` {#function-value-or}

<div class="function-declaration" id="function-value-or-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> FallbackType&gt; [[<span class="na">nodiscard</span>]] ValueType value_or(
    FallbackType &amp;&amp;fallback) <span class="k">const</span> &amp;;</code></pre></div>

<p>The value, or <code>fallback</code> when this <code><a href="result.html">result</a></code> holds an error.</p>

<div class="function-declaration" id="function-value-or-2"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> FallbackType&gt; [[<span class="na">nodiscard</span>]] ValueType value_or(
    FallbackType &amp;&amp;fallback) &amp;&amp;;</code></pre></div>

## Operator Overloads

### Public

#### `operator bool` {#operator-bool}

<div class="function-declaration" id="operator-bool-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">explicit</span> <span class="k">operator</span> <span class="k">bool</span>() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `operator*` {#operator-2a}

<div class="function-declaration" id="operator-2a-1"><pre><code>[[<span class="na">nodiscard</span>]] ValueType &amp;<span class="k">operator</span>*() &amp; <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="operator-2a-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> ValueType &amp;<span class="k">operator</span>*() <span class="k">const</span> &amp; <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="operator-2a-3"><pre><code>[[<span class="na">nodiscard</span>]] ValueType &amp;&amp;<span class="k">operator</span>*() &amp;&amp; <span class="k">noexcept</span>;</code></pre></div>

#### `operator->` {#operator-2d--3e}

<div class="function-declaration" id="operator-2d--3e-1"><pre><code>[[<span class="na">nodiscard</span>]] ValueType *<span class="k">operator</span>-&gt;() <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="operator-2d--3e-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> ValueType *<span class="k">operator</span>-&gt;() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `operator=` {#operator-3d}

<div class="function-declaration" id="operator-3d-1"><pre><code><a href="result.html">result</a> &amp;<span class="k">operator</span>=(
    <span class="k">const</span> <a href="result.html">result</a> &amp;) = <span class="k">default</span>;</code></pre></div>

<div class="function-declaration" id="operator-3d-2"><pre><code><a href="result.html">result</a> &amp;<span class="k">operator</span>=(
    <a href="result.html">result</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

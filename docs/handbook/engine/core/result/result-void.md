---
kind: class
---

[Engine](../../../engine.md) → [Core](../../core.md) → [result.h](../result.md)

# result<void>
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::core` |
| Include File Path | `engine/include/conjure/core/result.h` |
| Source File Path | N/A |
| API ↗ | [Errors and results](../../../../api/core/errors-and-results.md) |

## Syntax

```cpp
template <> class [[nodiscard]] result<void>
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">result() <span class="k">noexcept</span> = <span class="k">default</span>;</code></td><td></td></tr>
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
<tr><td>private</td><td><code class="signature">mError</code></td><td><code class="signature">std::optional&lt;<a href="../error/error.html">error</a>&gt;</code></td><td><code class="signature">std::nullopt</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `get_error` {#function-get-error}

<div class="function-declaration" id="function-get-error-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../error/error.html">error</a> &amp;get_error() <span class="k">const</span> &amp; <span class="k">noexcept</span>;</code></pre></div>

<p>The held error. Calling this on a success <code><a href="result.html">result</a></code> is a fatal programmer error.</p>

<div class="function-declaration" id="function-get-error-2"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../error/error.html">error</a> &amp;&amp;get_error() &amp;&amp; <span class="k">noexcept</span>;</code></pre></div>

#### `has_value` {#function-has-value}

<div class="function-declaration" id="function-has-value-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> has_value() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `success` {#function-success}

<div class="function-declaration" id="function-success-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="result.html">result</a> success() <span class="k">noexcept</span>;</code></pre></div>

<p><code>A</code> successful void <code><a href="result.html">result</a></code>. Equivalent to <code><a href="result.html">result</a>&lt;void&gt;</code>{}.</p>

## Operator Overloads

### Public

#### `operator bool` {#operator-bool}

<div class="function-declaration" id="operator-bool-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">explicit</span> <span class="k">operator</span> <span class="k">bool</span>() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `operator=` {#operator-3d}

<div class="function-declaration" id="operator-3d-1"><pre><code><a href="result.html">result</a> &amp;<span class="k">operator</span>=(
    <span class="k">const</span> <a href="result.html">result</a> &amp;) = <span class="k">default</span>;</code></pre></div>

<div class="function-declaration" id="operator-3d-2"><pre><code><a href="result.html">result</a> &amp;<span class="k">operator</span>=(
    <a href="result.html">result</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

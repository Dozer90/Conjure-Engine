---
kind: class
---

[Engine](../../../engine.md) → [Core](../../core.md) → [error.h](../error.md)

# error
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::core` |
| Include File Path | `engine/include/conjure/core/error.h` |
| Source File Path | `engine/src/core/foundation/error.cpp` |
| API ↗ | [Errors and results](../../../../api/core/errors-and-results.md) |

## Syntax

```cpp
class CONJURE_API error
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">error();</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature"><span class="k">explicit</span> error(
    <a href="../error.html#symbol-error-code">error_code</a> code);</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">error(
    <a href="../error.html#symbol-error-code">error_code</a> code,
    std::string message);</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">error(
    <a href="../types.html#symbol-u32">u32</a> category,
    <a href="../types.html#symbol-u32">u32</a> code,
    <span class="k">const</span> <span class="k">char</span> *categoryName,
    <a href="../error.html#symbol-error-code-name-fn">error_code_name_fn</a> codeName,
    std::string message);</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">error(
    <span class="k">const</span> <a href="error.html">error</a> &amp;) = <span class="k">default</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">error(
    <a href="error.html">error</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~error();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mCategory</code></td><td><code class="signature"><a href="../types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Category<wbr>Name</code></td><td><code class="signature"><span class="k">const</span> <span class="k">char</span> *</code></td><td><code class="signature"><span class="s">""</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mCode</code></td><td><code class="signature"><a href="../types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mCodeName</code></td><td><code class="signature"><a href="../error.html#symbol-error-code-name-fn">error_code_name_fn</a></code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mMessage</code></td><td><code class="signature">std::string</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `category` {#function-category}

<div class="function-declaration" id="function-category-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../types.html#symbol-u32">u32</a> category() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>FNV-1a 32 hash of the category name. <code>A</code> default-constructed <code><a href="error.html">error</a></code> carries the core category (<code><a href="error-category-traits-error-code.html">error_category_traits</a>&lt;<a href="../error.html#symbol-error-code">error_code</a>&gt;</code>::<code>ID</code>) with code <code>NONE</code>; test for &quot;no <code><a href="error.html">error</a></code>&quot; with <code><a href="#function-is-1">is</a>(error_code::NONE)</code>, not with <code><a href="#function-category-1">category</a>()</code> == 0.</p>

#### `category_name` {#function-category-name}

<div class="function-declaration" id="function-category-name-1"><pre><code>[[<span class="na">nodiscard</span>]] std::string_view category_name() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The category&#x27;s registered name (&quot;core&quot;, &quot;ecs&quot;, ...).</p>

#### `code` {#function-code}

<div class="function-declaration" id="function-code-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../types.html#symbol-u32">u32</a> code() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The raw code within the category.</p>

#### `code_name` {#function-code-name}

<div class="function-declaration" id="function-code-name-1"><pre><code>[[<span class="na">nodiscard</span>]] std::string_view code_name() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The enumerator name of the code within its category, or &quot;&quot; when unknown.</p>

#### `from` {#function-from}

<div class="function-declaration" id="function-from-1"><pre><code><span class="k">template</span> &lt;<a href="../error.html#symbol-error-enum">error_enum</a> EnumType&gt; [[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="error.html">error</a> from(
    EnumType code,
    std::string message = {});</code></pre></div>

<p>Builds an <code><a href="error.html">error</a></code> from a module <code><a href="error.html">error</a></code> enumerator.</p>

#### `generic_code` {#function-generic-code}

<div class="function-declaration" id="function-generic-code-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../error.html#symbol-error-code">error_code</a> generic_code() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The generic code when this <code><a href="error.html">error</a></code> belongs to the core category, otherwise <code>NONE</code>.</p>

#### `is` {#function-is}

<div class="function-declaration" id="function-is-1"><pre><code><span class="k">template</span> &lt;<a href="../error.html#symbol-error-enum">error_enum</a> EnumType&gt; [[<span class="na">nodiscard</span>]] <span class="k">bool</span> is(
    EnumType code) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when this <code><a href="error.html">error</a></code> carries exactly this code of this category.</p>

#### `is_category` {#function-is-category}

<div class="function-declaration" id="function-is-category-1"><pre><code><span class="k">template</span> &lt;<a href="../error.html#symbol-error-enum">error_enum</a> EnumType&gt; [[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_category() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when this <code><a href="error.html">error</a></code> belongs to the category bound to the given enumeration.</p>

#### `message` {#function-message}

<div class="function-declaration" id="function-message-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> std::string &amp;message() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Human readable detail; may be empty.</p>

#### `to_string` {#function-to-string}

<div class="function-declaration" id="function-to-string-1"><pre><code>[[<span class="na">nodiscard</span>]] std::string to_string() <span class="k">const</span>;</code></pre></div>

<p>&quot;<code>category.CODE</code>: message&quot;, or &quot;<code>category.CODE</code>&quot; when there is no message.</p>

## Operator Overloads

### Public

#### `operator=` {#operator-3d}

<div class="function-declaration" id="operator-3d-1"><pre><code><a href="error.html">error</a> &amp;<span class="k">operator</span>=(
    <span class="k">const</span> <a href="error.html">error</a> &amp;) = <span class="k">default</span>;</code></pre></div>

<div class="function-declaration" id="operator-3d-2"><pre><code><a href="error.html">error</a> &amp;<span class="k">operator</span>=(
    <a href="error.html">error</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="error.html">error</a> &amp;other) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

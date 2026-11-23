---
kind: class
---

[Engine](../../../../engine.md) → [Core](../../../core.md) → [stable_vector.h](../stable-vector.md)

# stable_vector::basic_iterator
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::core` |
| Include File Path | `engine/include/conjure/core/containers/stable_vector.h` |
| Source File Path | N/A |
| API ↗ | [Containers](../../../../../api/core/containers.md) |

## Syntax

```cpp
template <bool IsConst> class basic_iterator
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">basic_iterator() <span class="k">noexcept</span> = <span class="k">default</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">basic_iterator(
    <a href="../stable-vector.html#symbol-stable-vector-basic-iterator-owner-pointer">owner_pointer</a> owner,
    <a href="../../types.html#symbol-usize">usize</a> index) <span class="k">noexcept</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature"><span class="k">template</span> &lt;<span class="k">bool</span> OtherConst&gt; <span class="k">requires</span> (IsConst &amp;&amp; !OtherConst) basic_iterator(
    <span class="k">const</span> <a href="stable-vector-basic-iterator.html">basic_iterator</a>&lt;OtherConst&gt; &amp;other) <span class="k">noexcept</span>;</code></td><td>Mutable iterators convert to const ones.</td></tr>
</tbody>
</table>



## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mIndex</code></td><td><code class="signature"><a href="../../types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mOwner</code></td><td><code class="signature"><a href="../stable-vector.html#symbol-stable-vector-basic-iterator-owner-pointer">owner_pointer</a></code></td><td><code class="signature">nullptr</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `index` {#function-index}

<div class="function-declaration" id="function-index-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-usize">usize</a> index() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `owner` {#function-owner}

<div class="function-declaration" id="function-owner-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../stable-vector.html#symbol-stable-vector-basic-iterator-owner-pointer">owner_pointer</a> owner() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

## Operator Overloads

### Public

#### `operator*` {#operator-2a}

<div class="function-declaration" id="operator-2a-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../stable-vector.html#symbol-stable-vector-basic-iterator-reference">reference</a> <span class="k">operator</span>*() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `operator+` {#operator-2b}

<div class="function-declaration" id="operator-2b-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">friend</span> <a href="stable-vector-basic-iterator.html">basic_iterator</a> <span class="k">operator</span>+(
    <a href="stable-vector-basic-iterator.html">basic_iterator</a> left,
    <a href="../stable-vector.html#symbol-stable-vector-basic-iterator-difference-type">difference_type</a> offset);</code></pre></div>

<div class="function-declaration" id="operator-2b-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">friend</span> <a href="stable-vector-basic-iterator.html">basic_iterator</a> <span class="k">operator</span>+(
    <a href="../stable-vector.html#symbol-stable-vector-basic-iterator-difference-type">difference_type</a> offset,
    <a href="stable-vector-basic-iterator.html">basic_iterator</a> right);</code></pre></div>

#### `operator++` {#operator-2b--2b}

<div class="function-declaration" id="operator-2b--2b-1"><pre><code><a href="stable-vector-basic-iterator.html">basic_iterator</a> &amp;<span class="k">operator</span>++() <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="operator-2b--2b-2"><pre><code><a href="stable-vector-basic-iterator.html">basic_iterator</a> <span class="k">operator</span>++(
    <span class="k">int</span>) <span class="k">noexcept</span>;</code></pre></div>

#### `operator+=` {#operator-2b--3d}

<div class="function-declaration" id="operator-2b--3d-1"><pre><code><a href="stable-vector-basic-iterator.html">basic_iterator</a> &amp;<span class="k">operator</span>+=(
    <a href="../stable-vector.html#symbol-stable-vector-basic-iterator-difference-type">difference_type</a> offset) <span class="k">noexcept</span>;</code></pre></div>

#### `operator-` {#operator-2d}

<div class="function-declaration" id="operator-2d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">friend</span> <a href="stable-vector-basic-iterator.html">basic_iterator</a> <span class="k">operator</span>-(
    <a href="stable-vector-basic-iterator.html">basic_iterator</a> left,
    <a href="../stable-vector.html#symbol-stable-vector-basic-iterator-difference-type">difference_type</a> offset);</code></pre></div>

<div class="function-declaration" id="operator-2d-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">friend</span> <a href="../stable-vector.html#symbol-stable-vector-basic-iterator-difference-type">difference_type</a> <span class="k">operator</span>-(
    <span class="k">const</span> <a href="stable-vector-basic-iterator.html">basic_iterator</a> &amp;left,
    <span class="k">const</span> <a href="stable-vector-basic-iterator.html">basic_iterator</a> &amp;right) <span class="k">noexcept</span>;</code></pre></div>

#### `operator--` {#operator-2d--2d}

<div class="function-declaration" id="operator-2d--2d-1"><pre><code><a href="stable-vector-basic-iterator.html">basic_iterator</a> &amp;<span class="k">operator</span>--() <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="operator-2d--2d-2"><pre><code><a href="stable-vector-basic-iterator.html">basic_iterator</a> <span class="k">operator</span>--(
    <span class="k">int</span>) <span class="k">noexcept</span>;</code></pre></div>

#### `operator-=` {#operator-2d--3d}

<div class="function-declaration" id="operator-2d--3d-1"><pre><code><a href="stable-vector-basic-iterator.html">basic_iterator</a> &amp;<span class="k">operator</span>-=(
    <a href="../stable-vector.html#symbol-stable-vector-basic-iterator-difference-type">difference_type</a> offset) <span class="k">noexcept</span>;</code></pre></div>

#### `operator->` {#operator-2d--3e}

<div class="function-declaration" id="operator-2d--3e-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../stable-vector.html#symbol-stable-vector-basic-iterator-pointer">pointer</a> <span class="k">operator</span>-&gt;() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `operator<=>` {#operator-3c--3d--3e}

<div class="function-declaration" id="operator-3c--3d--3e-1"><pre><code>[[<span class="na">nodiscard</span>]] std::strong_ordering <span class="k">operator</span>&lt;=&gt;(
    <span class="k">const</span> <a href="stable-vector-basic-iterator.html">basic_iterator</a> &amp;other) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="stable-vector-basic-iterator.html">basic_iterator</a> &amp;other) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `operator[]` {#operator-5b--5d}

<div class="function-declaration" id="operator-5b--5d-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../stable-vector.html#symbol-stable-vector-basic-iterator-reference">reference</a> <span class="k">operator</span>[](
    <a href="../stable-vector.html#symbol-stable-vector-basic-iterator-difference-type">difference_type</a> offset) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->

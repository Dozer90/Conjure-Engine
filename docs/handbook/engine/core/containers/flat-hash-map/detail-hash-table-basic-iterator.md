---
kind: class
---

[Engine](../../../../engine.md) → [Core](../../../core.md) → [flat_hash_map.h](../flat-hash-map.md)

# detail::hash_table::basic_iterator
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::core` |
| Include File Path | `engine/include/conjure/core/containers/flat_hash_map.h` |
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
    <a href="../flat-hash-map.html#symbol-detail-hash-table-basic-iterator-pointer">pointer</a> values,
    <span class="k">const</span> <a href="../../types.html#symbol-u32">u32</a> *meta,
    <a href="../../types.html#symbol-usize">usize</a> index,
    <a href="../../types.html#symbol-usize">usize</a> anchor,
    <a href="../../types.html#symbol-usize">usize</a> mask) <span class="k">noexcept</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature"><span class="k">template</span> &lt;<span class="k">bool</span> OtherConst&gt; <span class="k">requires</span> (IsConst &amp;&amp; !OtherConst) basic_iterator(
    <span class="k">const</span> <a href="detail-hash-table-basic-iterator.html">basic_iterator</a>&lt;OtherConst&gt; &amp;other) <span class="k">noexcept</span>;</code></td><td>Mutable iterators convert to const ones.</td></tr>
</tbody>
</table>



## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mAnchor</code></td><td><code class="signature"><a href="../../types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mIndex</code></td><td><code class="signature"><a href="../../types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mMask</code></td><td><code class="signature"><a href="../../types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mMeta</code></td><td><code class="signature"><span class="k">const</span> <a href="../../types.html#symbol-u32">u32</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mValues</code></td><td><code class="signature"><a href="../flat-hash-map.html#symbol-detail-hash-table-basic-iterator-pointer">pointer</a></code></td><td><code class="signature">nullptr</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Private

#### `skip_empty` {#function-skip-empty}

<div class="function-declaration" id="function-skip-empty-1"><pre><code><span class="k">void</span> skip_empty() <span class="k">noexcept</span>;</code></pre></div>

## Operator Overloads

### Public

#### `operator*` {#operator-2a}

<div class="function-declaration" id="operator-2a-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../flat-hash-map.html#symbol-detail-hash-table-basic-iterator-reference">reference</a> <span class="k">operator</span>*() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `operator++` {#operator-2b--2b}

<div class="function-declaration" id="operator-2b--2b-1"><pre><code><a href="detail-hash-table-basic-iterator.html">basic_iterator</a> &amp;<span class="k">operator</span>++() <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="operator-2b--2b-2"><pre><code><a href="detail-hash-table-basic-iterator.html">basic_iterator</a> <span class="k">operator</span>++(
    <span class="k">int</span>) <span class="k">noexcept</span>;</code></pre></div>

#### `operator->` {#operator-2d--3e}

<div class="function-declaration" id="operator-2d--3e-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../flat-hash-map.html#symbol-detail-hash-table-basic-iterator-pointer">pointer</a> <span class="k">operator</span>-&gt;() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="detail-hash-table-basic-iterator.html">basic_iterator</a> &amp;other) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->

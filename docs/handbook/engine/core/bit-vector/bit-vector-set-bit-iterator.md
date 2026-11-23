---
kind: class
---

[Engine](../../../engine.md) → [Core](../../core.md) → [bit_vector.h](../bit-vector.md)

# bit_vector::set_bit_iterator
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::core` |
| Include File Path | `engine/include/conjure/core/bit_vector.h` |
| Source File Path | `engine/src/core/strings/bit_vector.cpp` |
| API ↗ | [Strings and identifiers](../../../../api/core/strings-and-identifiers.md) |

## Syntax

```cpp
class set_bit_iterator
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">constexpr</span> set_bit_iterator() <span class="k">noexcept</span> = <span class="k">default</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature"><span class="k">constexpr</span> set_bit_iterator(
    <span class="k">const</span> <a href="bit-vector.html">bit_vector</a> *owner,
    <a href="../types.html#symbol-usize">usize</a> index) <span class="k">noexcept</span>;</code></td><td></td></tr>
</tbody>
</table>



## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mIndex</code></td><td><code class="signature"><a href="../types.html#symbol-usize">usize</a></code></td><td><code class="signature">NPOS</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mOwner</code></td><td><code class="signature"><span class="k">const</span> <a href="bit-vector.html">bit_vector</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
</tbody>
</table>



## Operator Overloads

### Public

#### `operator*` {#operator-2a}

<div class="function-declaration" id="operator-2a-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../types.html#symbol-usize">usize</a> <span class="k">operator</span>*() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `operator++` {#operator-2b--2b}

<div class="function-declaration" id="operator-2b--2b-1"><pre><code><a href="bit-vector-set-bit-iterator.html">set_bit_iterator</a> &amp;<span class="k">operator</span>++() <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="operator-2b--2b-2"><pre><code><a href="bit-vector-set-bit-iterator.html">set_bit_iterator</a> <span class="k">operator</span>++(
    <span class="k">int</span>) <span class="k">noexcept</span>;</code></pre></div>

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="bit-vector-set-bit-iterator.html">set_bit_iterator</a> &amp;other) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->

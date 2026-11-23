---
kind: class
---

[Engine](../../../engine.md) → [Core](../../core.md) → [bit_vector.h](../bit-vector.md)

# bit_vector::set_bit_range
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::core` |
| Include File Path | `engine/include/conjure/core/bit_vector.h` |
| Source File Path | `engine/src/core/strings/bit_vector.cpp` |
| API ↗ | [Strings and identifiers](../../../../api/core/strings-and-identifiers.md) |

## Syntax

```cpp
class set_bit_range
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">constexpr</span> <span class="k">explicit</span> set_bit_range(
    <span class="k">const</span> <a href="bit-vector.html">bit_vector</a> *owner) <span class="k">noexcept</span>;</code></td><td></td></tr>
</tbody>
</table>



## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mOwner</code></td><td><code class="signature"><span class="k">const</span> <a href="bit-vector.html">bit_vector</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `begin` {#function-begin}

<div class="function-declaration" id="function-begin-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="bit-vector-set-bit-iterator.html">set_bit_iterator</a> begin() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `end` {#function-end}

<div class="function-declaration" id="function-end-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="bit-vector-set-bit-iterator.html">set_bit_iterator</a> end() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->

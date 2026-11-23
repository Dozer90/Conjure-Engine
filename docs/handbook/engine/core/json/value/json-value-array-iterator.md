---
kind: class
---

[Engine](../../../../engine.md) → [Core](../../../core.md) → [value.h](../value.md)

# json::value::array_iterator
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::core::json` |
| Include File Path | `engine/include/conjure/core/json/value.h` |
| Source File Path | `engine/src/core/json/value.cpp` |
| API ↗ | [JSON](../../../../../api/core/json.md) |

## Syntax

```cpp
class CONJURE_API array_iterator
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature"><span class="k">constexpr</span> array_iterator(
    <span class="k">void</span> *node,
    <a href="../../types.html#symbol-usize">usize</a> remaining,
    <span class="k">const</span> <a href="../document/json-document.html">document</a> *owner) <span class="k">noexcept</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature"><span class="k">constexpr</span> array_iterator() <span class="k">noexcept</span> = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>



## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mNode</code></td><td><code class="signature"><span class="k">void</span> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mOwner</code></td><td><code class="signature"><span class="k">const</span> <a href="../document/json-document.html">document</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mRemaining</code></td><td><code class="signature"><a href="../../types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
</tbody>
</table>



## Operator Overloads

### Public

#### `operator*` {#operator-2a}

<div class="function-declaration" id="operator-2a-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="json-value.html">value</a> <span class="k">operator</span>*() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `operator++` {#operator-2b--2b}

<div class="function-declaration" id="operator-2b--2b-1"><pre><code><a href="json-value-array-iterator.html">array_iterator</a> &amp;<span class="k">operator</span>++() <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="operator-2b--2b-2"><pre><code><a href="json-value-array-iterator.html">array_iterator</a> <span class="k">operator</span>++(
    <span class="k">int</span>) <span class="k">noexcept</span>;</code></pre></div>

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="json-value-array-iterator.html">array_iterator</a> &amp;other) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->

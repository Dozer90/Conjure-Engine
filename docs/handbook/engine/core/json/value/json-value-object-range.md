---
kind: class
---

[Engine](../../../../engine.md) → [Core](../../../core.md) → [value.h](../value.md)

# json::value::object_range
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::core::json` |
| Include File Path | `engine/include/conjure/core/json/value.h` |
| Source File Path | `engine/src/core/json/value.cpp` |
| API ↗ | [JSON](../../../../../api/core/json.md) |

## Syntax

```cpp
class object_range
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature"><span class="k">constexpr</span> <span class="k">explicit</span> object_range(
    <a href="json-value-object-iterator.html">object_iterator</a> first) <span class="k">noexcept</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature"><span class="k">constexpr</span> object_range() <span class="k">noexcept</span> = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>



## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mBegin</code></td><td><code class="signature"><a href="json-value-object-iterator.html">object_iterator</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `begin` {#function-begin}

<div class="function-declaration" id="function-begin-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="json-value-object-iterator.html">object_iterator</a> begin() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `end` {#function-end}

<div class="function-declaration" id="function-end-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="json-value-object-iterator.html">object_iterator</a> end() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->

---
kind: struct
---

[Engine](../../../../engine.md) → [Core](../../../core.md) → [type_id.h](../type-id.md)

# reflect::type_id
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::core::reflect` |
| Include File Path | `engine/include/conjure/core/reflect/type_id.h` |
| Source File Path | N/A |
| API ↗ | [Reflection](../../../../../api/core/reflection.md) |

## Syntax

```cpp
struct type_id
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">value</code></td><td><code class="signature"><a href="../../types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `from_name` {#function-from-name}

<div class="function-declaration" id="function-from-name-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <span class="k">constexpr</span> <a href="reflect-type-id.html">type_id</a> from_name(
    std::string_view name) <span class="k">noexcept</span>;</code></pre></div>

<p>The id a type registered under <code>name</code> has.</p>

#### `is_null` {#function-is-null}

<div class="function-declaration" id="function-is-null-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> is_null() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True for the <code>null</code> id.</p>

#### `null` {#function-null}

<div class="function-declaration" id="function-null-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <span class="k">constexpr</span> <a href="reflect-type-id.html">type_id</a> null() <span class="k">noexcept</span>;</code></pre></div>

<p>The <code>null</code> id. Equivalent to <code><a href="reflect-type-id.html">type_id</a></code>{}.</p>

## Operator Overloads

### Public

#### `operator<=>` {#operator-3c--3d--3e}

<div class="function-declaration" id="operator-3c--3d--3e-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">auto</span> <span class="k">operator</span>&lt;=&gt;(
    <span class="k">const</span> <a href="reflect-type-id.html">type_id</a> &amp;) <span class="k">const</span> <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="reflect-type-id.html">type_id</a> &amp;) <span class="k">const</span> <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->

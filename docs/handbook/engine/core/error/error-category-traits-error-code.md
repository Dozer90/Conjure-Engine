---
kind: struct
---

[Engine](../../../engine.md) → [Core](../../core.md) → [error.h](../error.md)

# error_category_traits<error_code>
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::core` |
| Include File Path | `engine/include/conjure/core/error.h` |
| Source File Path | `engine/src/core/foundation/error.cpp` |
| API ↗ | [Errors and results](../../../../api/core/errors-and-results.md) |

## Syntax

```cpp
template <> struct error_category_traits<error_code>
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">ID</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../types.html#symbol-u32">u32</a></code></td><td><code class="signature">hash::<wbr>fnv1a32(NAME)</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">NAME</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code></td><td><code class="signature"><span class="s">"core"</span></code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `code_name` {#function-code-name}

<div class="function-declaration" id="function-code-name-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> std::string_view code_name(
    <a href="../types.html#symbol-u32">u32</a> code) <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->

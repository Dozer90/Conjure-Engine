---
kind: struct
---

[Engine](../../../engine.md) → [Core](../../core.md) → [string_id.h](../string-id.md)

# std::hash<conjure::core::string_id>
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `std` |
| Include File Path | `engine/include/conjure/core/string_id.h` |
| Source File Path | `engine/src/core/strings/string_id.cpp` |
| API ↗ | [Containers](../../../../api/core/containers.md), [Strings and identifiers](../../../../api/core/strings-and-identifiers.md) |

## Syntax

```cpp
template <> struct std::hash<conjure::core::string_id>
```











## Operator Overloads

### Public

#### `operator()` {#operator-28--29}

<div class="function-declaration" id="operator-28--29-1"><pre><code>[[<span class="na">nodiscard</span>]] std::size_t <span class="k">operator</span>()(
    <span class="k">const</span> <a href="string-id.html">conjure::core::string_id</a> &amp;value) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<a id="variables"></a>
<!-- doc-legacy-anchors:end -->

---
kind: struct
---

[Engine](../../../engine.md) → [Core](../../core.md) → [uuid.h](../uuid.md)

# std::hash<conjure::core::uuid>
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `std` |
| Include File Path | `engine/include/conjure/core/uuid.h` |
| Source File Path | `engine/src/core/foundation/uuid.cpp` |
| API ↗ | [Data types](../../../../api/core/data-types.md) |

## Syntax

```cpp
template <> struct std::hash<conjure::core::uuid>
```











## Operator Overloads

### Public

#### `operator()` {#operator-28--29}

<div class="function-declaration" id="operator-28--29-1"><pre><code>[[<span class="na">nodiscard</span>]] std::size_t <span class="k">operator</span>()(
    <span class="k">const</span> <a href="uuid.html">conjure::core::uuid</a> &amp;value) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<a id="variables"></a>
<!-- doc-legacy-anchors:end -->

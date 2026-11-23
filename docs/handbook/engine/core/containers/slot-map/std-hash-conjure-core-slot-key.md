---
kind: struct
---

[Engine](../../../../engine.md) → [Core](../../../core.md) → [slot_map.h](../slot-map.md)

# std::hash<conjure::core::slot_key>
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `std` |
| Include File Path | `engine/include/conjure/core/containers/slot_map.h` |
| Source File Path | N/A |
| API ↗ | [Containers](../../../../../api/core/containers.md) |

## Syntax

```cpp
template <> struct std::hash<conjure::core::slot_key>
```











## Operator Overloads

### Public

#### `operator()` {#operator-28--29}

<div class="function-declaration" id="operator-28--29-1"><pre><code>[[<span class="na">nodiscard</span>]] std::size_t <span class="k">operator</span>()(
    <span class="k">const</span> <a href="slot-key.html">conjure::core::slot_key</a> &amp;key) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<a id="variables"></a>
<!-- doc-legacy-anchors:end -->

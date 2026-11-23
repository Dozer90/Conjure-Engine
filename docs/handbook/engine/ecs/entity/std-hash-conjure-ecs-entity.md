---
kind: struct
---

[Engine](../../../engine.md) → [ECS](../../ecs.md) → [entity.h](../entity.md)

# std::hash<conjure::ecs::entity>
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `std` |
| Include File Path | `engine/include/conjure/ecs/entity.h` |
| Source File Path | N/A |
| API ↗ | [Entities and components](../../../../api/ecs/entities-and-components.md) |

## Syntax

```cpp
template <> struct std::hash<conjure::ecs::entity>
```











## Operator Overloads

### Public

#### `operator()` {#operator-28--29}

<div class="function-declaration" id="operator-28--29-1"><pre><code>[[<span class="na">nodiscard</span>]] std::size_t <span class="k">operator</span>()(
    <span class="k">const</span> <a href="entity.html">conjure::ecs::entity</a> &amp;value) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<a id="variables"></a>
<!-- doc-legacy-anchors:end -->

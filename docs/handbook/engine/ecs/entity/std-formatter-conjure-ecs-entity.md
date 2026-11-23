---
kind: struct
---

[Engine](../../../engine.md) → [ECS](../../ecs.md) → [entity.h](../entity.md)

# std::formatter<conjure::ecs::entity>
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `std` |
| Include File Path | `engine/include/conjure/ecs/entity.h` |
| Source File Path | N/A |
| API ↗ | [Entities and components](../../../../api/ecs/entities-and-components.md) |

## Syntax

```cpp
template <> struct std::formatter<conjure::ecs::entity> : std::formatter<std::string_view>
```

## Inheritance Hierarchy


`std::formatter<std::string_view>` → `std::formatter<conjure::ecs::entity>`







## Functions

### Public

#### `format` {#function-format}

<div class="function-declaration" id="function-format-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> FormatContext&gt; <span class="k">auto</span> format(
    <span class="k">const</span> <a href="entity.html">conjure::ecs::entity</a> &amp;value,
    FormatContext &amp;context) <span class="k">const</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="variables"></a>
<!-- doc-legacy-anchors:end -->

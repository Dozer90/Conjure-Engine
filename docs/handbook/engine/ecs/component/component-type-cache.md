---
kind: struct
---

[Engine](../../../engine.md) → [ECS](../../ecs.md) → [component.h](../component.md)

# component_type_cache
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::core::json` |
| Include File Path | `engine/include/conjure/ecs/component.h` |
| Source File Path | N/A |
| API ↗ | [Entities and components](../../../../api/ecs/entities-and-components.md) |

## Syntax

```cpp
template <typename ComponentType> struct component_type_cache
```









## Functions

### Public

#### `slot` {#function-slot}

<div class="function-declaration" id="function-slot-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> std::atomic&lt;<a href="../../core/types.html#symbol-u64">u64</a>&gt; &amp;slot() <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="variables"></a>
<!-- doc-legacy-anchors:end -->

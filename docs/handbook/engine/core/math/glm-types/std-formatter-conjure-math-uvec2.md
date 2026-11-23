---
kind: struct
---

[Engine](../../../../engine.md) → [Core](../../../core.md) → [glm_types.h](../glm-types.md)

# std::formatter<conjure::math::uvec2>
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `std` |
| Include File Path | `engine/include/conjure/core/math/glm_types.h` |
| Source File Path | N/A |
| API ↗ | [Math](../../../../../api/core/math.md) |

## Syntax

```cpp
template <> struct std::formatter<conjure::math::uvec2> : std::formatter<conjure::u32>
```

## Inheritance Hierarchy


`std::formatter<conjure::u32>` → `std::formatter<conjure::math::uvec2>`







## Functions

### Public

#### `format` {#function-format}

<div class="function-declaration" id="function-format-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> FormatContext&gt; <span class="k">auto</span> format(
    <span class="k">const</span> <a href="../glm-types.html#symbol-uvec2">conjure::math::uvec2</a> &amp;value,
    FormatContext &amp;context) <span class="k">const</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="variables"></a>
<!-- doc-legacy-anchors:end -->

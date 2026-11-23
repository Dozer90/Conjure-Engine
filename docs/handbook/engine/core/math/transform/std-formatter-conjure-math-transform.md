---
kind: struct
---

[Engine](../../../../engine.md) → [Core](../../../core.md) → [transform.h](../transform.md)

# std::formatter<conjure::math::transform>
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `std` |
| Include File Path | `engine/include/conjure/core/math/transform.h` |
| Source File Path | `engine/src/core/math/transform.cpp` |
| API ↗ | [Math](../../../../../api/core/math.md) |

## Syntax

```cpp
template <> struct std::formatter<conjure::math::transform> : std::formatter<float>
```

## Inheritance Hierarchy


`std::formatter<float>` → `std::formatter<conjure::math::transform>`







## Functions

### Public

#### `format` {#function-format}

<div class="function-declaration" id="function-format-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> FormatContext&gt; <span class="k">auto</span> format(
    <span class="k">const</span> <a href="transform.html">conjure::math::transform</a> &amp;value,
    FormatContext &amp;context) <span class="k">const</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="variables"></a>
<!-- doc-legacy-anchors:end -->

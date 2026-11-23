---
kind: struct
---

[Engine](../../../engine.md) → [Core](../../core.md) → [uuid.h](../uuid.md)

# std::formatter<conjure::core::uuid>
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `std` |
| Include File Path | `engine/include/conjure/core/uuid.h` |
| Source File Path | `engine/src/core/foundation/uuid.cpp` |
| API ↗ | [Data types](../../../../api/core/data-types.md) |

## Syntax

```cpp
template <> struct std::formatter<conjure::core::uuid> : std::formatter<std::string>
```

## Inheritance Hierarchy


`std::formatter<std::string>` → `std::formatter<conjure::core::uuid>`







## Functions

### Public

#### `format` {#function-format}

<div class="function-declaration" id="function-format-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> FormatContext&gt; <span class="k">auto</span> format(
    <span class="k">const</span> <a href="uuid.html">conjure::core::uuid</a> &amp;value,
    FormatContext &amp;context) <span class="k">const</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="variables"></a>
<!-- doc-legacy-anchors:end -->

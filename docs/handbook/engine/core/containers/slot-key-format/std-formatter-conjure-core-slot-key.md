---
kind: struct
---

[Engine](../../../../engine.md) → [Core](../../../core.md) → [slot_key_format.h](../slot-key-format.md)

# std::formatter<conjure::core::slot_key>
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `std` |
| Include File Path | `engine/include/conjure/core/containers/slot_key_format.h` |
| Source File Path | N/A |
| API ↗ | [Containers](../../../../../api/core/containers.md) |

## Syntax

```cpp
template <> struct std::formatter<conjure::core::slot_key> : std::formatter<std::string>
```

## Inheritance Hierarchy


`std::formatter<std::string>` → `std::formatter<conjure::core::slot_key>`







## Functions

### Public

#### `format` {#function-format}

<div class="function-declaration" id="function-format-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> FormatContext&gt; <span class="k">auto</span> format(
    <span class="k">const</span> <a href="../slot-map/slot-key.html">conjure::core::slot_key</a> &amp;key,
    FormatContext &amp;context) <span class="k">const</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="variables"></a>
<!-- doc-legacy-anchors:end -->

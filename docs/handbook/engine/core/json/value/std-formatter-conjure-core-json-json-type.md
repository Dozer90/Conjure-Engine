---
kind: struct
---

[Engine](../../../../engine.md) → [Core](../../../core.md) → [value.h](../value.md)

# std::formatter<conjure::core::json::json_type>
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `std` |
| Include File Path | `engine/include/conjure/core/json/value.h` |
| Source File Path | `engine/src/core/json/value.cpp` |
| API ↗ | [JSON](../../../../../api/core/json.md) |

## Syntax

```cpp
template <> struct std::formatter<conjure::core::json::json_type> : std::formatter<std::string_view>
```

## Inheritance Hierarchy


`std::formatter<std::string_view>` → `std::formatter<conjure::core::json::json_type>`







## Functions

### Public

#### `format` {#function-format}

<div class="function-declaration" id="function-format-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> FormatContext&gt; <span class="k">auto</span> format(
    <a href="../value.html#symbol-json-json-type">conjure::core::json::json_type</a> type,
    FormatContext &amp;context) <span class="k">const</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="variables"></a>
<!-- doc-legacy-anchors:end -->

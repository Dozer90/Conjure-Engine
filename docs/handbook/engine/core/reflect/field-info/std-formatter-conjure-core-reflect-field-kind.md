---
kind: struct
---

[Engine](../../../../engine.md) → [Core](../../../core.md) → [field_info.h](../field-info.md)

# std::formatter<conjure::core::reflect::field_kind>
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `std` |
| Include File Path | `engine/include/conjure/core/reflect/field_info.h` |
| Source File Path | `engine/src/core/reflect/field_info.cpp` |
| API ↗ | [Reflection](../../../../../api/core/reflection.md) |

## Syntax

```cpp
template <> struct std::formatter<conjure::core::reflect::field_kind> : std::formatter<std::string_view>
```

## Inheritance Hierarchy


`std::formatter<std::string_view>` → `std::formatter<conjure::core::reflect::field_kind>`







## Functions

### Public

#### `format` {#function-format}

<div class="function-declaration" id="function-format-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> FormatContext&gt; <span class="k">auto</span> format(
    <a href="../field-info.html#symbol-reflect-field-kind">conjure::core::reflect::field_kind</a> kind,
    FormatContext &amp;context) <span class="k">const</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="variables"></a>
<!-- doc-legacy-anchors:end -->

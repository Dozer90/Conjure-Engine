---
kind: struct
---

[Engine](../../../../engine.md) → [Core](../../../core.md) → [type_id.h](../type-id.md)

# std::formatter<conjure::core::reflect::type_id>
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `std` |
| Include File Path | `engine/include/conjure/core/reflect/type_id.h` |
| Source File Path | N/A |
| API ↗ | [Reflection](../../../../../api/core/reflection.md) |

## Syntax

```cpp
template <> struct std::formatter<conjure::core::reflect::type_id> : std::formatter<std::string_view>
```

## Inheritance Hierarchy


`std::formatter<std::string_view>` → `std::formatter<conjure::core::reflect::type_id>`







## Functions

### Public

#### `format` {#function-format}

<div class="function-declaration" id="function-format-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> FormatContext&gt; <span class="k">auto</span> format(
    <span class="k">const</span> <a href="reflect-type-id.html">conjure::core::reflect::type_id</a> &amp;id,
    FormatContext &amp;context) <span class="k">const</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="variables"></a>
<!-- doc-legacy-anchors:end -->

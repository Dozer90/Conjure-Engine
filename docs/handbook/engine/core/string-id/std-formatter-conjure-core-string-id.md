---
kind: struct
---

[Engine](../../../engine.md) → [Core](../../core.md) → [string_id.h](../string-id.md)

# std::formatter<conjure::core::string_id>
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `std` |
| Include File Path | `engine/include/conjure/core/string_id.h` |
| Source File Path | `engine/src/core/strings/string_id.cpp` |
| API ↗ | [Containers](../../../../api/core/containers.md), [Strings and identifiers](../../../../api/core/strings-and-identifiers.md) |

## Syntax

```cpp
template <> struct std::formatter<conjure::core::string_id> : std::formatter<std::string>
```

## Inheritance Hierarchy


`std::formatter<std::string>` → `std::formatter<conjure::core::string_id>`







## Functions

### Public

#### `format` {#function-format}

<div class="function-declaration" id="function-format-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> FormatContext&gt; <span class="k">auto</span> format(
    <span class="k">const</span> <a href="string-id.html">conjure::core::string_id</a> &amp;value,
    FormatContext &amp;context) <span class="k">const</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="variables"></a>
<!-- doc-legacy-anchors:end -->

---
kind: struct
---

[Engine](../../../engine.md) → [Core](../../core.md) → [error.h](../error.md)

# std::formatter<conjure::core::error_code>
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `std` |
| Include File Path | `engine/include/conjure/core/error.h` |
| Source File Path | `engine/src/core/foundation/error.cpp` |
| API ↗ | [Errors and results](../../../../api/core/errors-and-results.md) |

## Syntax

```cpp
template <> struct std::formatter<conjure::core::error_code> : std::formatter<std::string_view>
```

## Inheritance Hierarchy


`std::formatter<std::string_view>` → `std::formatter<conjure::core::error_code>`







## Functions

### Public

#### `format` {#function-format}

<div class="function-declaration" id="function-format-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> FormatContext&gt; <span class="k">auto</span> format(
    <a href="../error.html#symbol-error-code">conjure::core::error_code</a> code,
    FormatContext &amp;context) <span class="k">const</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="variables"></a>
<!-- doc-legacy-anchors:end -->

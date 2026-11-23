---
kind: struct
---

[Engine](../../../engine.md) → [Modules](../../modules.md) → [module_types.h](../module-types.md)

# std::formatter<conjure::modules::module_state>
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `std` |
| Include File Path | `engine/include/conjure/modules/module_types.h` |
| Source File Path | `engine/src/modules/module_types.cpp` |
| API ↗ | [Writing a module](../../../../api/modules/writing-a-module.md) |

## Syntax

```cpp
template <> struct std::formatter<conjure::modules::module_state> : std::formatter<std::string_view>
```

## Inheritance Hierarchy


`std::formatter<std::string_view>` → `std::formatter<conjure::modules::module_state>`







## Functions

### Public

#### `format` {#function-format}

<div class="function-declaration" id="function-format-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> FormatContext&gt; <span class="k">auto</span> format(
    <a href="../module-types.html#symbol-module-state">conjure::modules::module_state</a> value,
    FormatContext &amp;context) <span class="k">const</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="variables"></a>
<!-- doc-legacy-anchors:end -->

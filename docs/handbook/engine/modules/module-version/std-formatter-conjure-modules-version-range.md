---
kind: struct
---

[Engine](../../../engine.md) → [Modules](../../modules.md) → [module_version.h](../module-version.md)

# std::formatter<conjure::modules::version_range>
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `std` |
| Include File Path | `engine/include/conjure/modules/module_version.h` |
| Source File Path | `engine/src/modules/module_version.cpp` |
| API ↗ | [Writing a module](../../../../api/modules/writing-a-module.md) |

## Syntax

```cpp
template <> struct std::formatter<conjure::modules::version_range> : std::formatter<std::string>
```

## Inheritance Hierarchy


`std::formatter<std::string>` → `std::formatter<conjure::modules::version_range>`







## Functions

### Public

#### `format` {#function-format}

<div class="function-declaration" id="function-format-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> FormatContext&gt; <span class="k">auto</span> format(
    <span class="k">const</span> <a href="version-range.html">conjure::modules::version_range</a> &amp;value,
    FormatContext &amp;context) <span class="k">const</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="variables"></a>
<!-- doc-legacy-anchors:end -->

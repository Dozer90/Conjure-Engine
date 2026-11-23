---
kind: struct
---

[Engine](../../../../engine.md) → [Core](../../../core.md) → [json_conversion.h](../json-conversion.md)

# json::json_converter<math::transform>
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::core::json` |
| Include File Path | `engine/include/conjure/core/reflect/json_conversion.h` |
| Source File Path | `engine/src/core/reflect/json_conversion.cpp` |
| API ↗ | [Reflection](../../../../../api/core/reflection.md) |

## Syntax

```cpp
template <> struct CONJURE_API json_converter<math::transform>
```









## Functions

### Public

#### `from_json` {#function-from-json}

<div class="function-declaration" id="function-from-json-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../result/result.html">result</a>&lt;<a href="../../math/transform/transform.html">math::transform</a>&gt; from_json(
    <span class="k">const</span> <a href="../../json/value/json-value.html">value</a> &amp;source);</code></pre></div>

<p>Reads {&quot;position&quot;, &quot;rotation&quot;, &quot;scale&quot;}; absent members keep the identity.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="variables"></a>
<!-- doc-legacy-anchors:end -->

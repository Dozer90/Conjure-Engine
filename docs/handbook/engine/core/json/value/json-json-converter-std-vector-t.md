---
kind: struct
---

[Engine](../../../../engine.md) → [Core](../../../core.md) → [value.h](../value.md)

# json::json_converter<std::vector<T>>
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::core::json` |
| Include File Path | `engine/include/conjure/core/json/value.h` |
| Source File Path | `engine/src/core/json/value.cpp` |
| API ↗ | [JSON](../../../../../api/core/json.md) |

## Syntax

```cpp
template <json_convertible T> struct json_converter<std::vector<T>>
```









## Functions

### Public

#### `from_json` {#function-from-json}

<div class="function-declaration" id="function-from-json-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../result/result.html">result</a>&lt;std::vector&lt;T&gt;&gt; from_json(
    <span class="k">const</span> <a href="json-value.html">value</a> &amp;source);</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="variables"></a>
<!-- doc-legacy-anchors:end -->

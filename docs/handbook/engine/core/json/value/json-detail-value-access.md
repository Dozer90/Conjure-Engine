---
kind: struct
---

[Engine](../../../../engine.md) → [Core](../../../core.md) → [value.h](../value.md)

# json::detail::value_access
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::core::json` |
| Include File Path | `engine/include/conjure/core/json/value.h` |
| Source File Path | `engine/src/core/json/value.cpp` |
| API ↗ | [JSON](../../../../../api/core/json.md) |

## Syntax

```cpp
struct value_access
```









## Functions

### Public

#### `make` {#function-make}

<div class="function-declaration" id="function-make-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="json-value.html">value</a> make(
    <span class="k">void</span> *node,
    <span class="k">const</span> <a href="../document/json-document.html">document</a> *owner) <span class="k">noexcept</span>;</code></pre></div>

#### `node` {#function-node}

<div class="function-declaration" id="function-node-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <span class="k">void</span> *node(
    <span class="k">const</span> <a href="json-value.html">value</a> &amp;source) <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="variables"></a>
<!-- doc-legacy-anchors:end -->

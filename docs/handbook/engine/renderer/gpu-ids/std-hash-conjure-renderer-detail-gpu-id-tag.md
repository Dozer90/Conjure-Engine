---
kind: struct
---

[Engine](../../../engine.md) → [Renderer](../../renderer.md) → [gpu_ids.h](../gpu-ids.md)

# std::hash<conjure::renderer::detail::gpu_id<Tag>>
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `std` |
| Include File Path | `engine/include/conjure/renderer/gpu_ids.h` |
| Source File Path | N/A |
| API ↗ | [Rendering](../../../../api/renderer/index.md), [Rendering a frame](../../../../api/renderer/rendering-a-frame.md) |

## Syntax

```cpp
template <typename Tag> struct std::hash<conjure::renderer::detail::gpu_id<Tag>>
```











## Operator Overloads

### Public

#### `operator()` {#operator-28--29}

<div class="function-declaration" id="operator-28--29-1"><pre><code>[[<span class="na">nodiscard</span>]] std::size_t <span class="k">operator</span>()(
    <span class="k">const</span> <a href="detail-gpu-id.html">conjure::renderer::detail::gpu_id</a>&lt;Tag&gt; &amp;id) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<a id="variables"></a>
<!-- doc-legacy-anchors:end -->

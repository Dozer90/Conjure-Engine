---
kind: struct
---

[Engine](../../../engine.md) → [Scene](../../scene.md) → [components.h](../components.md)

# detail::depth_tag_name
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::scene` |
| Include File Path | `engine/include/conjure/scene/components.h` |
| Source File Path | `engine/src/scene/components.cpp` |
| API ↗ | [Scenes, hierarchy and prefabs](../../../../api/scene/index.md), [Scenes and the hierarchy](../../../../api/scene/scenes-and-hierarchy.md) |

## Syntax

```cpp
template <u8 Depth> struct depth_tag_name
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">CHARACTERS</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> std::<wbr>array&lt;<wbr><span class="k">char</span>,<wbr> DEPTH_<wbr>TAG_<wbr>NAME_<wbr>LENGTH&gt;</code></td><td><code class="signature">make_<wbr>depth_<wbr>tag_<wbr>name(Depth)</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">VALUE</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code></td><td><code class="signature">std::<wbr>string_<wbr>view(CHARACTERS.data(),<wbr> CHARACTERS.size())</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->

---
kind: struct
---

[Engine](../../../engine.md) → [Scene](../../scene.md) → [scene.h](../scene.md)

# instantiate_options
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::scene` |
| Include File Path | `engine/include/conjure/scene/scene.h` |
| Source File Path | `engine/src/scene/scene.cpp` |
| API ↗ | [Scenes, hierarchy and prefabs](../../../../api/scene/index.md), [Scenes and the hierarchy](../../../../api/scene/scenes-and-hierarchy.md) |

## Syntax

```cpp
struct instantiate_options
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">bindAssets</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">true</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">guids</code></td><td><code class="signature"><a href="../scene.html#symbol-guid-policy">guid_policy</a></code></td><td><code class="signature">guid_<wbr>policy::<wbr>REGENERATE_<wbr>ALL</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">index</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature">APPEND_CHILD</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">owner</code></td><td><code class="signature"><a href="scene-id.html">scene_id</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">parent</code></td><td><code class="signature"><a href="../../ecs/entity/entity.html">ecs::entity</a></code></td><td><code class="signature">ecs::NULL_ENTITY</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">rootName</code></td><td><code class="signature">std::<wbr>optional&lt;<wbr><a href="../../core/string-id/string-id.html">core::<wbr>string_<wbr>id</a>&gt;</code></td><td><code class="signature">std::nullopt</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">root<wbr>Transform</code></td><td><code class="signature">std::<wbr>optional&lt;<wbr><a href="../components/local-transform.html">local_<wbr>transform</a>&gt;</code></td><td><code class="signature">std::nullopt</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->

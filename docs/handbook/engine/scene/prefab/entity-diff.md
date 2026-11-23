---
kind: struct
---

[Engine](../../../engine.md) → [Scene](../../scene.md) → [prefab.h](../prefab.md)

# entity_diff
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::scene` |
| Include File Path | `engine/include/conjure/scene/prefab.h` |
| Source File Path | `engine/src/scene/prefab.cpp` |
| API ↗ | [Scenes, hierarchy and prefabs](../../../../api/scene/index.md), [Scene files, prefabs and snapshots](../../../../api/scene/scene-files-and-prefabs.md) |

## Syntax

```cpp
struct entity_diff
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">assetGuid</code></td><td><code class="signature"><a href="../../core/uuid/uuid.html">core::uuid</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">components</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="../../core/string-id/string-id.html">core::<wbr>string_<wbr>id</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">instance<wbr>Entity</code></td><td><code class="signature"><a href="../../ecs/entity/entity.html">ecs::entity</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">kind</code></td><td><code class="signature"><a href="../prefab.html#symbol-entity-diff-kind">entity_diff_kind</a></code></td><td><code class="signature">entity_<wbr>diff_<wbr>kind::<wbr>MODIFIED</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->

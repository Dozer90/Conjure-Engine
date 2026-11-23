---
kind: struct
---

[Engine](../../../engine.md) → [Scene](../../scene.md) → [prefab.h](../prefab.md)

# prefab_asset
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::scene` |
| Include File Path | `engine/include/conjure/scene/prefab.h` |
| Source File Path | `engine/src/scene/prefab.cpp` |
| API ↗ | [Scenes, hierarchy and prefabs](../../../../api/scene/index.md), [Scene files, prefabs and snapshots](../../../../api/scene/scene-files-and-prefabs.md) |

## Syntax

```cpp
struct prefab_asset
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">archive</code></td><td><code class="signature">std::<wbr>shared_<wbr>ptr&lt;<wbr><span class="k">const</span> <a href="../../ecs/entity-archive/entity-archive.html">ecs::<wbr>entity_<wbr>archive</a>&gt;</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">dependencies</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="../../assets/asset-types.html#symbol-asset-id">assets::<wbr>asset_<wbr>id</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">id</code></td><td><code class="signature"><a href="../../assets/asset-types.html#symbol-asset-id">assets::asset_id</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">name</code></td><td><code class="signature">std::string</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">rootGuid</code></td><td><code class="signature"><a href="../../core/uuid/uuid.html">core::uuid</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->

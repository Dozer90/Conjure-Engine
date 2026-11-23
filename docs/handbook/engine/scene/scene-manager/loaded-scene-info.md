---
kind: struct
---

[Engine](../../../engine.md) → [Scene](../../scene.md) → [scene_manager.h](../scene-manager.md)

# loaded_scene_info
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::scene` |
| Include File Path | `engine/include/conjure/scene/scene_manager.h` |
| Source File Path | `engine/src/scene/scene_manager.cpp` |
| API ↗ | [Scenes, hierarchy and prefabs](../../../../api/scene/index.md), [Scene files, prefabs and snapshots](../../../../api/scene/scene-files-and-prefabs.md) |

## Syntax

```cpp
struct loaded_scene_info
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">asset</code></td><td><code class="signature"><a href="../../assets/asset-types.html#symbol-asset-id">assets::asset_id</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">id</code></td><td><code class="signature"><a href="../scene/scene-id.html">scene_id</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">name</code></td><td><code class="signature">std::string</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">path</code></td><td><code class="signature">std::string</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">roots</code></td><td><code class="signature">std::vector&lt;<a href="../../ecs/entity/entity.html">ecs::entity</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->

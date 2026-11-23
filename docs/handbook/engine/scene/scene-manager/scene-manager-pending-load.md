---
kind: struct
---

[Engine](../../../engine.md) → [Scene](../../scene.md) → [scene_manager.h](../scene-manager.md)

# scene_manager::pending_load
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::scene` |
| Include File Path | `engine/include/conjure/scene/scene_manager.h` |
| Source File Path | `engine/src/scene/scene_manager.cpp` |
| API ↗ | [Scenes, hierarchy and prefabs](../../../../api/scene/index.md), [Scene files, prefabs and snapshots](../../../../api/scene/scene-files-and-prefabs.md) |

## Syntax

```cpp
struct pending_load
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">asset</code></td><td><code class="signature"><a href="../../assets/asset-types.html#symbol-asset-id">assets::asset_id</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">handle</code></td><td><code class="signature"><a href="../../assets/asset-ref/asset-handle.html">assets::asset_handle</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">id</code></td><td><code class="signature"><a href="load-request-id.html">load_request_id</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">mode</code></td><td><code class="signature"><a href="../scene-manager.html#symbol-load-mode">load_mode</a></code></td><td><code class="signature">load_<wbr>mode::<wbr>ADDITIVE</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->

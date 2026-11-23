---
kind: struct
---

[Engine](../../../engine.md) → [Scene](../../scene.md) → [prefab.h](../prefab.md)

# prefab_builder::node
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::scene` |
| Include File Path | `engine/include/conjure/scene/prefab.h` |
| Source File Path | `engine/src/scene/prefab.cpp` |
| API ↗ | [Scenes, hierarchy and prefabs](../../../../api/scene/index.md), [Scene files, prefabs and snapshots](../../../../api/scene/scene-files-and-prefabs.md) |

## Syntax

```cpp
struct node
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">components</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="prefab-builder-node-component.html">node_<wbr>component</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">name</code></td><td><code class="signature"><a href="../../core/string-id/string-id.html">core::string_id</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">parentIndex</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature">NO_PARENT</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">tags</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr>ecs::<wbr>component_<wbr>id&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">transform</code></td><td><code class="signature"><a href="../components/local-transform.html">local_transform</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->

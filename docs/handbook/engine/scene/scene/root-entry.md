---
kind: struct
---

[Engine](../../../engine.md) → [Scene](../../scene.md) → [scene.h](../scene.md)

# root_entry
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::scene` |
| Include File Path | `engine/include/conjure/scene/scene.h` |
| Source File Path | `engine/src/scene/scene.cpp` |
| API ↗ | [Scenes, hierarchy and prefabs](../../../../api/scene/index.md), [Scenes and the hierarchy](../../../../api/scene/scenes-and-hierarchy.md) |

## Syntax

```cpp
struct root_entry
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">entity</code></td><td><code class="signature"><a href="../../ecs/entity/entity.html">ecs::entity</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">owner</code></td><td><code class="signature"><a href="scene-id.html">scene_id</a></code></td><td><code class="signature">SCENE_<wbr>ID_<wbr>AUTHORED</code></td><td></td></tr>
</tbody>
</table>



## Operator Overloads

### Public

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="root-entry.html">root_entry</a> &amp;) <span class="k">const</span> <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->

---
kind: struct
---

[Engine](../../../engine.md) → [Physics](../../physics.md) → [queries.h](../queries.md)

# query_filter
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::physics` |
| Include File Path | `engine/include/conjure/physics/queries.h` |
| Source File Path | N/A |
| API ↗ | [Physics](../../../../api/physics/index.md), [The physics world and queries](../../../../api/physics/physics-world-and-queries.md) |

## Syntax

```cpp
struct query_filter
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">ignore</code></td><td><code class="signature"><a href="../../ecs/entity/entity.html">ecs::entity</a></code></td><td><code class="signature">ecs::NULL_ENTITY</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">include<wbr>Static</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">true</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">include<wbr>Triggers</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">layers</code></td><td><code class="signature"><a href="../physics-layers/physics-layer-mask.html">physics_layer_mask</a></code></td><td><code class="signature">physics_<wbr>layer_<wbr>mask::<wbr>all()</code></td><td></td></tr>
</tbody>
</table>



## Operator Overloads

### Public

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="query-filter.html">query_filter</a> &amp;) <span class="k">const</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->

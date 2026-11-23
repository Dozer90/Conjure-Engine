---
kind: struct
---

[Engine](../../../engine.md) → [Scene](../../scene.md) → [transform_system.h](../transform-system.md)

# transform_system::run_counters
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::scene` |
| Include File Path | `engine/include/conjure/scene/transform_system.h` |
| Source File Path | `engine/src/scene/transform_system.cpp` |
| API ↗ | [Scenes, hierarchy and prefabs](../../../../api/scene/index.md), [Scenes and the hierarchy](../../../../api/scene/scenes-and-hierarchy.md) |

## Syntax

```cpp
struct run_counters
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">chunks<wbr>Skipped</code></td><td><code class="signature">std::atomic&lt;<a href="../../core/types.html#symbol-u32">u32</a>&gt;</code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">chunks<wbr>Visited</code></td><td><code class="signature">std::atomic&lt;<a href="../../core/types.html#symbol-u32">u32</a>&gt;</code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">entities<wbr>Visited</code></td><td><code class="signature">std::atomic&lt;<a href="../../core/types.html#symbol-u32">u32</a>&gt;</code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">render<wbr>Recomputed</code></td><td><code class="signature">std::atomic&lt;<a href="../../core/types.html#symbol-u32">u32</a>&gt;</code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">world<wbr>Recomputed</code></td><td><code class="signature">std::atomic&lt;<a href="../../core/types.html#symbol-u32">u32</a>&gt;</code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->

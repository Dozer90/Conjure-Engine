---
kind: struct
---

[Editor](../../../editor.md) → [User Interface](../../user-interface.md) → [picker.h](../picker.md)

# pick_context
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::core::jobs` |
| Include File Path | `engine/include/conjure/editor/picker.h` |
| Source File Path | `engine/src/editor/core/picker.cpp` |
| API ↗ | [Headless mode and the command model](../../../../api/editor/headless-and-commands.md), [The editor](../../../../api/editor/index.md) |

## Syntax

```cpp
struct pick_context
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">jobs</code></td><td><code class="signature"><a href="../../../engine/core/jobs/job-system/jobs-job-system.html">core::jobs::job_system</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">maxDistance</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">1.0</span>e6f</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">physics</code></td><td><code class="signature"><a href="../../../engine/physics/physics-world/physics-world.html">physics::physics_world</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">scene</code></td><td><code class="signature"><span class="k">const</span> scene::scene *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->

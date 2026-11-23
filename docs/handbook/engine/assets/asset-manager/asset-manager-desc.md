---
kind: struct
---

[Engine](../../../engine.md) → [Assets](../../assets.md) → [asset_manager.h](../asset-manager.md)

# asset_manager_desc
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::assets` |
| Include File Path | `engine/include/conjure/assets/asset_manager.h` |
| Source File Path | `engine/src/assets/asset_manager.cpp` |
| API ↗ | [Cooking, caches and packs](../../../../api/assets/cooking-and-packs.md), [Database, importers and hot reload](../../../../api/assets/database-and-import.md), [Assets](../../../../api/assets/index.md), [Referencing and loading assets](../../../../api/assets/referencing-and-loading.md) |

## Syntax

```cpp
struct asset_manager_desc
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">blocking<wbr>Timeout<wbr>Seconds</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f64">f64</a></code></td><td><code class="signature"><span class="mi">30.0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">finalise<wbr>Budget<wbr>Seconds</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f64">f64</a></code></td><td><code class="signature"><span class="mi">0.002</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">jobs</code></td><td><code class="signature"><a href="../../core/jobs/job-system/jobs-job-system.html">core::jobs::job_system</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">max<wbr>Jobs<wbr>In<wbr>Flight</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature">DEFAULT_<wbr>MAX_<wbr>JOBS_<wbr>IN_<wbr>FLIGHT</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">source</code></td><td><code class="signature">std::<wbr>unique_<wbr>ptr&lt;<wbr><a href="../asset-source/asset-source.html">asset_<wbr>source</a>&gt;</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->

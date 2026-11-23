---
kind: struct
---

[Engine](../../../engine.md) → [Assets](../../assets.md) → [asset_loader.h](../asset-loader.md)

# finalise_context
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::assets` |
| Include File Path | `engine/include/conjure/assets/asset_loader.h` |
| Source File Path | `engine/src/assets/asset_loader.cpp` |
| API ↗ | [Database, importers and hot reload](../../../../api/assets/database-and-import.md), [Assets](../../../../api/assets/index.md) |

## Syntax

```cpp
struct finalise_context
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">dependencies</code></td><td><code class="signature">std::<wbr>span&lt;<wbr><span class="k">const</span> <a href="../asset-ref/asset-handle.html">asset_<wbr>handle</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">frameIndex</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">id</code></td><td><code class="signature"><a href="../asset-types.html#symbol-asset-id">asset_id</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">isReload</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->

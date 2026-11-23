---
kind: struct
---

[Engine](../../../engine.md) → [Assets](../../assets.md) → [asset_manager.h](../asset-manager.md)

# asset_event
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::assets` |
| Include File Path | `engine/include/conjure/assets/asset_manager.h` |
| Source File Path | `engine/src/assets/asset_manager.cpp` |
| API ↗ | [Cooking, caches and packs](../../../../api/assets/cooking-and-packs.md), [Database, importers and hot reload](../../../../api/assets/database-and-import.md), [Assets](../../../../api/assets/index.md), [Referencing and loading assets](../../../../api/assets/referencing-and-loading.md) |

## Syntax

```cpp
struct asset_event
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">error</code></td><td><code class="signature"><span class="k">const</span> <a href="../../core/error/error.html">core::error</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">id</code></td><td><code class="signature"><a href="../asset-types.html#symbol-asset-id">asset_id</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">kind</code></td><td><code class="signature"><a href="../asset-manager.html#symbol-asset-event-kind">asset_event_kind</a></code></td><td><code class="signature">asset_<wbr>event_<wbr>kind::<wbr>LOADED</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">type</code></td><td><code class="signature"><a href="../asset-types.html#symbol-asset-type-id">asset_type_id</a></code></td><td><code class="signature">asset_<wbr>type_<wbr>id::<wbr>INVALID</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">version</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->

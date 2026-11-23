---
kind: struct
---

[Engine](../../../engine.md) → [Assets](../../assets.md) → [asset_importer.h](../asset-importer.md)

# imported_asset
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::assets` |
| Include File Path | `engine/include/conjure/assets/asset_importer.h` |
| Source File Path | `engine/src/assets/asset_importer.cpp` |
| API ↗ | [Database, importers and hot reload](../../../../api/assets/database-and-import.md), [Assets](../../../../api/assets/index.md) |

## Syntax

```cpp
struct imported_asset
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">dependencies</code></td><td><code class="signature">std::vector&lt;<a href="../asset-types.html#symbol-asset-id">asset_id</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">displayName</code></td><td><code class="signature">std::string</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">payload</code></td><td><code class="signature">std::vector&lt;std::byte&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">subPath</code></td><td><code class="signature">std::string</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">type</code></td><td><code class="signature"><a href="../asset-types.html#symbol-asset-type-id">asset_type_id</a></code></td><td><code class="signature">asset_<wbr>type_<wbr>id::<wbr>INVALID</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->

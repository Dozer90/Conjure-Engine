---
kind: struct
---

[Engine](../../../engine.md) → [Assets](../../assets.md) → [asset_extension_desc.h](../asset-extension-desc.md)

# importer_extension_desc
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::assets` |
| Include File Path | `engine/include/conjure/assets/asset_extension_desc.h` |
| Source File Path | `engine/src/assets/asset_extension_desc.cpp` |
| API ↗ | [Cooking, caches and packs](../../../../api/assets/cooking-and-packs.md) |

## Syntax

```cpp
struct importer_extension_desc
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">create</code></td><td><code class="signature">std::<wbr>unique_<wbr>ptr&lt;<wbr><a href="../asset-importer/asset-importer.html">asset_<wbr>importer</a>&gt; (*)()</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">EXTENSION_<wbr>POINT</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../core/string-id/string-id.html">core::<wbr>string_<wbr>id</a></code></td><td><code class="signature">{<span class="s">"assets.importer"</span>}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">id</code></td><td><code class="signature"><span class="k">const</span> <span class="k">char</span> *</code></td><td><code class="signature"><span class="s">""</span></code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->

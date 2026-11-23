---
kind: struct
---

[Engine](../../../engine.md) → [Renderer](../../renderer.md) → [gltf_importer.h](../gltf-importer.md)

# gltf_import_result
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::renderer` |
| Include File Path | `engine/include/conjure/renderer/gltf_importer.h` |
| Source File Path | `engine/src/renderer/import/gltf_importer.cpp` |
| API ↗ | [Rendering](../../../../api/renderer/index.md), [Renderer assets](../../../../api/renderer/renderer-assets.md) |

## Syntax

```cpp
struct gltf_import_result
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">materials</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="../material-asset/material-asset.html">material_<wbr>asset</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">material<wbr>Textures</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="../gltf-importer.html#symbol-material-texture-indices">material_<wbr>texture_<wbr>indices</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">meshes</code></td><td><code class="signature">std::vector&lt;<a href="../mesh-asset/mesh-asset.html">mesh_asset</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">model</code></td><td><code class="signature"><a href="../model-asset/model-asset.html">model_asset</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">textures</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="../texture-asset/texture-asset.html">texture_<wbr>asset</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">warnings</code></td><td><code class="signature">std::vector&lt;std::string&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->

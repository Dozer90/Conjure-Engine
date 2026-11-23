---
kind: class
---

[Engine](../../../engine.md) → [Renderer](../../renderer.md) → [material_asset.h](../material-asset.md)

# material_importer
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::renderer` |
| Include File Path | `engine/include/conjure/renderer/material_asset.h` |
| Source File Path | `engine/src/renderer/assets/material_asset.cpp` |
| API ↗ | [Rendering](../../../../api/renderer/index.md), [Renderer assets](../../../../api/renderer/renderer-assets.md) |

## Syntax

```cpp
class CONJURE_API material_importer final : public assets::asset_importer
```

## Inheritance Hierarchy


[`assets::asset_importer`](../../assets/asset-importer/asset-importer.md) → `material_importer`

## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">material_importer();</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~material_importer() <span class="k">override</span>;</code></td><td></td></tr>
</tbody>
</table>



## Functions

### Public

#### `extensions` {#function-extensions}

<div class="function-declaration" id="function-extensions-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> std::string_view&gt; extensions() <span class="k">const</span> <span class="k">noexcept</span> <span class="k">override</span>;</code></pre></div>

#### `id` {#function-id}

<div class="function-declaration" id="function-id-1"><pre><code>[[<span class="na">nodiscard</span>]] std::string_view id() <span class="k">const</span> <span class="k">noexcept</span> <span class="k">override</span>;</code></pre></div>

#### `import` {#function-import}

<div class="function-declaration" id="function-import-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="../../assets/asset-importer/import-output.html">assets::import_output</a>&gt; import(
    <a href="../../assets/asset-importer/import-context.html">assets::import_context</a> &amp;context) <span class="k">const</span> <span class="k">override</span>;</code></pre></div>

#### `version` {#function-version}

<div class="function-declaration" id="function-version-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> version() <span class="k">const</span> <span class="k">noexcept</span> <span class="k">override</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="variables"></a>
<!-- doc-legacy-anchors:end -->

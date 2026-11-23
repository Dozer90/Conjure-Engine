---
kind: class
---

[Engine](../../../engine.md) → [Renderer](../../renderer.md) → [texture_asset.h](../texture-asset.md)

# texture_loader
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::renderer` |
| Include File Path | `engine/include/conjure/renderer/texture_asset.h` |
| Source File Path | `engine/src/renderer/assets/texture_asset.cpp` |
| API ↗ | [Rendering](../../../../api/renderer/index.md), [JPEG XL images](../../../../api/renderer/jpeg-xl.md), [Renderer assets](../../../../api/renderer/renderer-assets.md) |

## Syntax

```cpp
class CONJURE_API texture_loader final : public assets::typed_asset_loader<texture_asset, texture_asset>
```

## Inheritance Hierarchy


`assets::typed_asset_loader<texture_asset, texture_asset>` → `texture_loader`

## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">texture_loader();</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~texture_loader() <span class="k">override</span>;</code></td><td></td></tr>
</tbody>
</table>



## Functions

### Protected

#### `create_fallback_typed` {#function-create-fallback-typed}

<div class="function-declaration" id="function-create-fallback-typed-1"><pre><code>[[<span class="na">nodiscard</span>]] std::optional&lt;<a href="texture-asset.html">texture_asset</a>&gt; create_fallback_typed() <span class="k">const</span> <span class="k">override</span>;</code></pre></div>

#### `finalise_typed` {#function-finalise-typed}

<div class="function-declaration" id="function-finalise-typed-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="texture-asset.html">texture_asset</a>&gt; finalise_typed(
    <span class="k">const</span> <a href="../../assets/asset-loader/finalise-context.html">assets::finalise_context</a> &amp;context,
    <a href="texture-asset.html">texture_asset</a> &amp;&amp;intermediate) <span class="k">const</span> <span class="k">override</span>;</code></pre></div>

#### `load_cpu_typed` {#function-load-cpu-typed}

<div class="function-declaration" id="function-load-cpu-typed-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="texture-asset.html">texture_asset</a>&gt; load_cpu_typed(
    <span class="k">const</span> <a href="../../assets/asset-loader/load-context.html">assets::load_context</a> &amp;context,
    std::span&lt;<span class="k">const</span> std::byte&gt; payloadBytes) <span class="k">const</span> <span class="k">override</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="variables"></a>
<!-- doc-legacy-anchors:end -->

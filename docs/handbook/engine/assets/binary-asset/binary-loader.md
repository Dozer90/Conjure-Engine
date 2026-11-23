---
kind: class
---

[Engine](../../../engine.md) → [Assets](../../assets.md) → [binary_asset.h](../binary-asset.md)

# binary_loader
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::assets` |
| Include File Path | `engine/include/conjure/assets/binary_asset.h` |
| Source File Path | `engine/src/assets/binary_asset.cpp` |
| API ↗ | [Cooking, caches and packs](../../../../api/assets/cooking-and-packs.md), [Database, importers and hot reload](../../../../api/assets/database-and-import.md), [Assets](../../../../api/assets/index.md), [Referencing and loading assets](../../../../api/assets/referencing-and-loading.md) |

## Syntax

```cpp
class CONJURE_API binary_loader final : public typed_asset_loader<binary_asset, binary_asset>
```

## Inheritance Hierarchy


`typed_asset_loader<binary_asset, binary_asset>` → `binary_loader`

## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">binary_loader();</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~binary_loader() <span class="k">override</span>;</code></td><td></td></tr>
</tbody>
</table>



## Functions

### Protected

#### `create_fallback_typed` {#function-create-fallback-typed}

<div class="function-declaration" id="function-create-fallback-typed-1"><pre><code>[[<span class="na">nodiscard</span>]] std::optional&lt;<a href="binary-asset.html">binary_asset</a>&gt; create_fallback_typed() <span class="k">const</span> <span class="k">override</span>;</code></pre></div>

#### `finalise_typed` {#function-finalise-typed}

<div class="function-declaration" id="function-finalise-typed-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="binary-asset.html">binary_asset</a>&gt; finalise_typed(
    <span class="k">const</span> <a href="../asset-loader/finalise-context.html">finalise_context</a> &amp;context,
    <a href="binary-asset.html">binary_asset</a> &amp;&amp;intermediate) <span class="k">const</span> <span class="k">override</span>;</code></pre></div>

#### `load_cpu_typed` {#function-load-cpu-typed}

<div class="function-declaration" id="function-load-cpu-typed-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="binary-asset.html">binary_asset</a>&gt; load_cpu_typed(
    <span class="k">const</span> <a href="../asset-loader/load-context.html">load_context</a> &amp;context,
    std::span&lt;<span class="k">const</span> std::byte&gt; payloadBytes) <span class="k">const</span> <span class="k">override</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="variables"></a>
<!-- doc-legacy-anchors:end -->

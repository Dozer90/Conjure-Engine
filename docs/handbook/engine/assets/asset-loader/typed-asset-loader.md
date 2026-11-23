---
kind: class
---

[Engine](../../../engine.md) → [Assets](../../assets.md) → [asset_loader.h](../asset-loader.md)

# typed_asset_loader
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::assets` |
| Include File Path | `engine/include/conjure/assets/asset_loader.h` |
| Source File Path | `engine/src/assets/asset_loader.cpp` |
| API ↗ | [Database, importers and hot reload](../../../../api/assets/database-and-import.md), [Assets](../../../../api/assets/index.md) |

## Syntax

```cpp
template <declared_asset_type AssetType, typename Intermediate> class typed_asset_loader : public asset_loader
```

## Inheritance Hierarchy


[`asset_loader`](asset-loader.md) → `typed_asset_loader`

## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">typed_asset_loader() = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~typed_asset_loader() <span class="k">override</span> = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>



## Functions

### Public

#### `create_fallback` {#function-create-fallback}

<div class="function-declaration" id="function-create-fallback-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="../asset-payload.html#symbol-asset-payload">asset_payload</a>&gt; create_fallback() <span class="k">const</span> <span class="k">final</span>;</code></pre></div>

#### `finalise` {#function-finalise}

<div class="function-declaration" id="function-finalise-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="../asset-payload.html#symbol-asset-payload">asset_payload</a>&gt; finalise(
    <span class="k">const</span> <a href="finalise-context.html">finalise_context</a> &amp;context,
    <a href="../asset-payload.html#symbol-asset-payload">asset_payload</a> &amp;&amp;cpuOutput) <span class="k">const</span> <span class="k">final</span>;</code></pre></div>

#### `load_cpu` {#function-load-cpu}

<div class="function-declaration" id="function-load-cpu-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="../asset-payload.html#symbol-asset-payload">asset_payload</a>&gt; load_cpu(
    <span class="k">const</span> <a href="load-context.html">load_context</a> &amp;context,
    std::span&lt;<span class="k">const</span> std::byte&gt; payloadBytes) <span class="k">const</span> <span class="k">final</span>;</code></pre></div>

#### `type` {#function-type}

<div class="function-declaration" id="function-type-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../asset-types.html#symbol-asset-type-id">asset_type_id</a> type() <span class="k">const</span> <span class="k">noexcept</span> <span class="k">final</span>;</code></pre></div>

### Protected

#### `create_fallback_typed` {#function-create-fallback-typed}

<div class="function-declaration" id="function-create-fallback-typed-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> std::optional&lt;AssetType&gt; create_fallback_typed() <span class="k">const</span>;</code></pre></div>

<p>Main thread, once per type. nullopt (the default) means the type has no fallback.</p>

#### `finalise_typed` {#function-finalise-typed}

<div class="function-declaration" id="function-finalise-typed-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../../core/result/result.html">core::result</a>&lt;AssetType&gt; finalise_typed(
    <span class="k">const</span> <a href="finalise-context.html">finalise_context</a> &amp;context,
    Intermediate &amp;&amp;intermediate) <span class="k">const</span> = <span class="mi">0</span>;</code></pre></div>

<p>Main thread. Produces the published value from the intermediate.</p>

#### `load_cpu_typed` {#function-load-cpu-typed}

<div class="function-declaration" id="function-load-cpu-typed-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../../core/result/result.html">core::result</a>&lt;Intermediate&gt; load_cpu_typed(
    <span class="k">const</span> <a href="load-context.html">load_context</a> &amp;context,
    std::span&lt;<span class="k">const</span> std::byte&gt; payloadBytes) <span class="k">const</span> = <span class="mi">0</span>;</code></pre></div>

<p>Worker thread. Decodes the bytes into an owning intermediate.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="variables"></a>
<!-- doc-legacy-anchors:end -->

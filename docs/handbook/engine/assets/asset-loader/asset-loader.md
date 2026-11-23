---
kind: class
---

[Engine](../../../engine.md) → [Assets](../../assets.md) → [asset_loader.h](../asset-loader.md)

# asset_loader
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::assets` |
| Include File Path | `engine/include/conjure/assets/asset_loader.h` |
| Source File Path | `engine/src/assets/asset_loader.cpp` |
| API ↗ | [Database, importers and hot reload](../../../../api/assets/database-and-import.md), [Assets](../../../../api/assets/index.md) |

## Syntax

```cpp
class CONJURE_API asset_loader
```

## Derived Classes


<details><summary>Classes that use this as a base</summary>

<ul>

<li><a href="typed-asset-loader.html">typed_asset_loader</a></li>

</ul>

</details>

## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">asset_loader();</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">virtual</span> ~asset_loader();</code></td><td></td></tr>
</tbody>
</table>



## Functions

### Public

#### `create_fallback` {#function-create-fallback}

<div class="function-declaration" id="function-create-fallback-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../../core/result/result.html">core::result</a>&lt;<a href="../asset-payload.html#symbol-asset-payload">asset_payload</a>&gt; create_fallback() <span class="k">const</span>;</code></pre></div>

<p>Main thread, once per type, lazily (or at registration with <code>PINNED_FALLBACK</code>). The default returns a <code>null</code> payload, meaning &quot;no fallback&quot;.</p>

#### `finalise` {#function-finalise}

<div class="function-declaration" id="function-finalise-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../../core/result/result.html">core::result</a>&lt;<a href="../asset-payload.html#symbol-asset-payload">asset_payload</a>&gt; finalise(
    <span class="k">const</span> <a href="finalise-context.html">finalise_context</a> &amp;context,
    <a href="../asset-payload.html#symbol-asset-payload">asset_payload</a> &amp;&amp;cpuOutput) <span class="k">const</span> = <span class="mi">0</span>;</code></pre></div>

<p>Main thread. Turns the intermediate into the published payload.</p>

#### `load_cpu` {#function-load-cpu}

<div class="function-declaration" id="function-load-cpu-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../../core/result/result.html">core::result</a>&lt;<a href="../asset-payload.html#symbol-asset-payload">asset_payload</a>&gt; load_cpu(
    <span class="k">const</span> <a href="load-context.html">load_context</a> &amp;context,
    std::span&lt;<span class="k">const</span> std::byte&gt; payloadBytes) <span class="k">const</span> = <span class="mi">0</span>;</code></pre></div>

<p>Worker thread. Decodes the payload bytes into an intermediate that owns its data; the bytes are valid only for the duration of the call.</p>

#### `type` {#function-type}

<div class="function-declaration" id="function-type-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../asset-types.html#symbol-asset-type-id">asset_type_id</a> type() <span class="k">const</span> <span class="k">noexcept</span> = <span class="mi">0</span>;</code></pre></div>

<p>The type this loader produces.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="variables"></a>
<!-- doc-legacy-anchors:end -->

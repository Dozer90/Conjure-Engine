---
kind: class
---

[Engine](../../../engine.md) → [Audio](../../audio.md) → [clip_asset.h](../clip-asset.md)

# audio_clip_loader
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::audio` |
| Include File Path | `engine/include/conjure/audio/clip_asset.h` |
| Source File Path | `engine/src/audio/clip_asset.cpp` |
| API ↗ | [Clips and decoding](../../../../api/audio/clips-and-decoding.md) |

## Syntax

```cpp
class CONJURE_API audio_clip_loader final : public assets::typed_asset_loader<audio_clip, audio_clip>
```

## Inheritance Hierarchy


`assets::typed_asset_loader<audio_clip, audio_clip>` → `audio_clip_loader`

## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">audio_clip_loader();</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~audio_clip_loader() <span class="k">override</span>;</code></td><td></td></tr>
</tbody>
</table>



## Functions

### Protected

#### `create_fallback_typed` {#function-create-fallback-typed}

<div class="function-declaration" id="function-create-fallback-typed-1"><pre><code>[[<span class="na">nodiscard</span>]] std::optional&lt;<a href="../clip/audio-clip.html">audio_clip</a>&gt; create_fallback_typed() <span class="k">const</span> <span class="k">override</span>;</code></pre></div>

<p><code>A</code> 0.1 s silent mono 48 kHz clip (the &quot;silent clip&quot; fallback of).</p>

#### `finalise_typed` {#function-finalise-typed}

<div class="function-declaration" id="function-finalise-typed-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="../clip/audio-clip.html">audio_clip</a>&gt; finalise_typed(
    <span class="k">const</span> <a href="../../assets/asset-loader/finalise-context.html">assets::finalise_context</a> &amp;context,
    <a href="../clip/audio-clip.html">audio_clip</a> &amp;&amp;intermediate) <span class="k">const</span> <span class="k">override</span>;</code></pre></div>

#### `load_cpu_typed` {#function-load-cpu-typed}

<div class="function-declaration" id="function-load-cpu-typed-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="../clip/audio-clip.html">audio_clip</a>&gt; load_cpu_typed(
    <span class="k">const</span> <a href="../../assets/asset-loader/load-context.html">assets::load_context</a> &amp;context,
    std::span&lt;<span class="k">const</span> std::byte&gt; payloadBytes) <span class="k">const</span> <span class="k">override</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="variables"></a>
<!-- doc-legacy-anchors:end -->

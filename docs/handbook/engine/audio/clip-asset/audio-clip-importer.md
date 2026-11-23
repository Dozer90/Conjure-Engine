---
kind: class
---

[Engine](../../../engine.md) → [Audio](../../audio.md) → [clip_asset.h](../clip-asset.md)

# audio_clip_importer
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::audio` |
| Include File Path | `engine/include/conjure/audio/clip_asset.h` |
| Source File Path | `engine/src/audio/clip_asset.cpp` |
| API ↗ | [Clips and decoding](../../../../api/audio/clips-and-decoding.md) |

## Syntax

```cpp
class CONJURE_API audio_clip_importer final : public assets::asset_importer
```

## Inheritance Hierarchy


[`assets::asset_importer`](../../assets/asset-importer/asset-importer.md) → `audio_clip_importer`

## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">audio_clip_importer();</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~audio_clip_importer() <span class="k">override</span>;</code></td><td></td></tr>
</tbody>
</table>



## Functions

### Public

#### `default_settings` {#function-default-settings}

<div class="function-declaration" id="function-default-settings-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/json/document/json-document.html">core::json::document</a> default_settings() <span class="k">const</span> <span class="k">override</span>;</code></pre></div>

<p><code><a href="clip-import-settings.html">clip_import_settings</a></code>{} as JSON.</p>

#### `extensions` {#function-extensions}

<div class="function-declaration" id="function-extensions-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> std::string_view&gt; extensions() <span class="k">const</span> <span class="k">noexcept</span> <span class="k">override</span>;</code></pre></div>

<p><code>CLIP_EXTENSIONS</code>.</p>

#### `id` {#function-id}

<div class="function-declaration" id="function-id-1"><pre><code>[[<span class="na">nodiscard</span>]] std::string_view id() <span class="k">const</span> <span class="k">noexcept</span> <span class="k">override</span>;</code></pre></div>

<p>&quot;<code>conjure.audio.clip</code>&quot;.</p>

#### `import` {#function-import}

<div class="function-declaration" id="function-import-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="../../assets/asset-importer/import-output.html">assets::import_output</a>&gt; import(
    <a href="../../assets/asset-importer/import-context.html">assets::import_context</a> &amp;context) <span class="k">const</span> <span class="k">override</span>;</code></pre></div>

<p>Reads the settings, cooks the source bytes and returns the primary asset. Decode or settings failures come back as <code>IMPORT_FAILED</code> with the audio error&#x27;s message. <code>A</code> clip cooked as <code>STREAMED</code> while <code>forceMono</code>, <code>sampleRate</code> or normalize is set adds an import warning, since those settings apply to <code>PCM</code> storage only.</p>

#### `version` {#function-version}

<div class="function-declaration" id="function-version-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> version() <span class="k">const</span> <span class="k">noexcept</span> <span class="k">override</span>;</code></pre></div>

<p><code>CLIP_IMPORTER_VERSION</code>.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="variables"></a>
<!-- doc-legacy-anchors:end -->

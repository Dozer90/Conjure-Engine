---
kind: header
---

[Engine](../../engine.md) → [Audio](../audio.md)

# clip_asset.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/audio/clip_asset.h` |
| API ↗ | [Clips and decoding](../../../api/audio/clips-and-decoding.md) |

<p>The <code>conjure.audio.clip</code> cooked blob, import settings, importer and loader. The pure functions <code>cook_clip</code> and <code>load_clip_blob</code> hold the whole import and load logic; <code><a href="clip-asset/audio-clip-importer.html">audio_clip_importer</a></code> and <code><a href="clip-asset/audio-clip-loader.html">audio_clip_loader</a></code> are the thin assets-module adapters over them. The loader needs no <code><a href="audio-engine/audio-engine.html">audio_engine</a></code>: the clip is a self-contained CPU payload.</p>

## Structs

| File | Description |
|---|---|
| [`clip_blob_header`](clip-asset/clip-blob-header.md) | 32-<code><a href="../core/types.html#symbol-byte">byte</a></code> little-endian header of a cooked clip, written verbatim and followed by <code>dataBytes</code> of interleaved <code><a href="../core/types.html#symbol-f32">f32</a></code> samples (<code>PCM</code>) or the original encoded file (<code>STREAMED</code>). |
| [`clip_import_settings`](clip-asset/clip-import-settings.md) | Importer settings, mirrored by the JSON object { &quot;storage&quot;: &quot;auto&quot;, &quot;<code>streamThresholdSeconds</code>&quot;: 10.0, &quot;<code>forceMono</code>&quot;: <code>false</code>, &quot;<code>sampleRate</code>&quot;: 0, &quot;normalize&quot;: <code>false</code> }. <code>forceMono</code>, <code>sampleRate</code> and normalize apply to <code>PCM</code> storage only; <code>STREAMED</code> keeps the file, and the importer reports a warning when one of them is set on a clip it streams. |

## Classes

| File | Description |
|---|---|
| [`audio_clip_importer`](clip-asset/audio-clip-importer.md) | The <code>conjure.audio.clip</code> importer: .wav .ogg .mp3 .flac -&gt; one cooked clip blob (<code>cook_clip</code>). Deterministic and stateless; runs on a worker thread. |
| [`audio_clip_loader`](clip-asset/audio-clip-loader.md) | The <code>conjure.audio.clip</code> loader: blob -&gt; <code><a href="clip/audio-clip.html">audio_clip</a></code> on the load job; finalise returns the clip unchanged; the fallback is the silent clip. |

## Enumerations

### `clip_storage_setting` {#symbol-clip-storage-setting}

```cpp
enum class clip_storage_setting : u8 {
    AUTO,
    PCM,
    STREAMED
};
```

The storage choice of an import: <code>AUTO</code> picks <code>STREAMED</code> above <code>streamThresholdSeconds</code>.

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-CLIP_BLOB_VERSION"></a><code>CLIP_BLOB_VERSION</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">1</span></code> | Blob format version written into <code>clip_blob_header::version</code>. |
| <a id="variable-CLIP_EXTENSIONS"></a><code>CLIP_EXTENSIONS</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>array&lt;<wbr>std::<wbr>string_<wbr>view,<wbr> <span class="mi">4</span>&gt;</code> | <code class="signature">{<span class="s">".wav"</span>,<wbr> <span class="s">".ogg"</span>,<wbr> <span class="s">".mp3"</span>,<wbr> <span class="s">".flac"</span>}</code> |  |
| <a id="variable-CLIP_IMPORTER_ID"></a><code>CLIP_IMPORTER_ID</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code> | <code class="signature"><span class="s">"conjure.audio.clip"</span></code> | Importer identity and the extensions it claims. |
| <a id="variable-CLIP_IMPORTER_VERSION"></a><code>CLIP_IMPORTER_VERSION</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">1</span></code> |  |

## Functions

### Public

#### `cook_clip` {#function-cook-clip}

<div class="function-declaration" id="function-cook-clip-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;std::vector&lt;<a href="../core/types.html#symbol-byte">byte</a>&gt;&gt; cook_clip(
    std::span&lt;<span class="k">const</span> <a href="../core/types.html#symbol-byte">byte</a>&gt; encoded,
    <span class="k">const</span> <a href="clip-asset/clip-import-settings.html">clip_import_settings</a> &amp;settings);</code></pre></div>

<p>Imports a source file into a cooked blob (header + payload). <code>PCM</code> storage decodes with <code><a href="decoder/decode-options.html">decode_options</a></code> built from the settings; <code>STREAMED</code> storage copies the file <code><a href="../core/types.html#symbol-byte">byte</a></code> for <code><a href="../core/types.html#symbol-byte">byte</a></code> and records the probed format. Deterministic for the same input and build. Errors are those of <code>probe()</code> / <code>decode_to_pcm()</code>.</p>

#### `is_clip_extension` {#function-is-clip-extension}

<div class="function-declaration" id="function-is-clip-extension-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <span class="k">bool</span> is_clip_extension(
    std::string_view extension) <span class="k">noexcept</span>;</code></pre></div>

<p>True when the lower-case extension (&quot;.wav&quot;) is one the importer claims.</p>

#### `load_clip_blob` {#function-load-clip-blob}

<div class="function-declaration" id="function-load-clip-blob-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<a href="clip/audio-clip.html">audio_clip</a>&gt; load_clip_blob(
    std::span&lt;<span class="k">const</span> <a href="../core/types.html#symbol-byte">byte</a>&gt; blob);</code></pre></div>

<p>Turns a blob into a self-contained clip: <code>PCM</code> samples are copied into a fresh <code><a href="clip/pcm-buffer.html">pcm_buffer</a></code>, <code>STREAMED</code> bytes are copied and opened with the header&#x27;s <code>frameCount</code> (no full decode) to check that their format matches the header. Errors: <code>INVALID_CLIP</code> for a bad header or a streamed payload whose format disagrees with it, <code>DECODE_FAILED</code> / <code>UNSUPPORTED_FORMAT</code> when streamed bytes do not decode.</p>

#### `read_clip_blob_header` {#function-read-clip-blob-header}

<div class="function-declaration" id="function-read-clip-blob-header-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<a href="clip-asset/clip-blob-header.html">clip_blob_header</a>&gt; read_clip_blob_header(
    std::span&lt;<span class="k">const</span> <a href="../core/types.html#symbol-byte">byte</a>&gt; blob);</code></pre></div>

<p>Validates and returns the header of a blob: magic, version, storage and encoding values, channels 1 or 2, sample rate in range, <code>dataBytes</code> equal to the payload size and (<code>PCM</code>) equal to <code>frameCount</code> * channels * 4. Fails with <code>INVALID_CLIP</code> naming the mismatch.</p>

#### `register_asset_types` {#function-register-asset-types}

<div class="function-declaration" id="function-register-asset-types-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; register_asset_types(
    <a href="../assets/asset-type-registry/asset-type-registry.html">assets::asset_type_registry</a> &amp;types,
    <a href="../assets/asset-importer/importer-registry.html">assets::importer_registry</a> &amp;importers);</code></pre></div>

<p>Registers <code>conjure.audio.clip</code> (with its loader) and the clip importer. Main thread, from <code>audio_subsystem::startup</code>. Errors are those of the two registries (<code>TYPE_DUPLICATE</code>, ...).</p>

#### `to_string` {#function-to-string}

<div class="function-declaration" id="function-to-string-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="clip-asset.html#symbol-clip-storage-setting">clip_storage_setting</a> value) <span class="k">noexcept</span>;</code></pre></div>

<p>Lower-case JSON spelling (&quot;auto&quot;, &quot;pcm&quot;, &quot;streamed&quot;).</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="audio_clip_importer"></a>
<a id="audio_clip_importeraudio_clip_importer"></a>
<a id="audio_clip_importeraudio_clip_importer_1"></a>
<a id="audio_clip_importerdefault_settings"></a>
<a id="audio_clip_importerextensions"></a>
<a id="audio_clip_importerid"></a>
<a id="audio_clip_importerimport"></a>
<a id="audio_clip_importerversion"></a>
<a id="audio_clip_loader"></a>
<a id="audio_clip_loaderaudio_clip_loader"></a>
<a id="audio_clip_loaderaudio_clip_loader_1"></a>
<a id="audio_clip_loadercreate_fallback_typed"></a>
<a id="audio_clip_loaderfinalise_typed"></a>
<a id="audio_clip_loaderload_cpu_typed"></a>
<a id="clip_blob_header"></a>
<a id="clip_blob_headerchannels"></a>
<a id="clip_blob_headerdatabytes"></a>
<a id="clip_blob_headerencoding"></a>
<a id="clip_blob_headerexpected_magic"></a>
<a id="clip_blob_headerframecount"></a>
<a id="clip_blob_headermagic"></a>
<a id="clip_blob_headeroperator"></a>
<a id="clip_blob_headerreserved"></a>
<a id="clip_blob_headersamplerate"></a>
<a id="clip_blob_headerstorage"></a>
<a id="clip_blob_headerversion"></a>
<a id="clip_blob_version"></a>
<a id="clip_extensions"></a>
<a id="clip_import_settings"></a>
<a id="clip_import_settingsforcemono"></a>
<a id="clip_import_settingsfrom_json"></a>
<a id="clip_import_settingsnormalize"></a>
<a id="clip_import_settingsoperator"></a>
<a id="clip_import_settingssamplerate"></a>
<a id="clip_import_settingsstorage"></a>
<a id="clip_import_settingsstreamthresholdseconds"></a>
<a id="clip_import_settingsto_document"></a>
<a id="clip_import_settingsto_json"></a>
<a id="clip_importer_id"></a>
<a id="clip_importer_version"></a>
<a id="clip_storage_setting"></a>
<a id="clip_storage_settingauto"></a>
<a id="clip_storage_settingpcm"></a>
<a id="clip_storage_settingstreamed"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="cook_clip"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="is_clip_extension"></a>
<a id="load_clip_blob"></a>
<a id="other-declarations"></a>
<a id="read_clip_blob_header"></a>
<a id="register_asset_types"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="to_string"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->

---
kind: header
---

[Engine](../../engine.md) → [Audio](../audio.md)

# clip.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/audio/clip.h` |
| API ↗ | [Clips and decoding](../../../api/audio/clips-and-decoding.md) |

<p>Clips. <code>A</code> <code><a href="clip/pcm-buffer.html">pcm_buffer</a></code> is immutable once built and shared between the clip asset and every voice playing it through <code>std::shared_ptr</code>, so a voice keeps playing the old samples after a hot reload swaps the clip. <code><a href="clip/audio-clip.html">audio_clip</a></code> is the payload of the <code>conjure.audio.clip</code> asset type declared at the end of this header.</p>

## Structs

| File | Description |
|---|---|
| [`audio_clip`](clip/audio-clip.md) | The <code>conjure.audio.clip</code> payload: info plus one of two shared buffers (<code>PCM</code> samples, or the original encoded file for <code>STREAMED</code> storage). |
| [`clip_info`](clip/clip-info.md) | What is known about a clip without touching its samples. |

## Classes

| File | Description |
|---|---|
| [`pcm_buffer`](clip/pcm-buffer.md) | Interleaved <code><a href="../core/types.html#symbol-f32">f32</a></code> frames. Immutable after construction; shared by the clip and its voices. |

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-CLIP_ASSET_TYPE_NAME"></a><code>CLIP_ASSET_TYPE_NAME</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code> | <code class="signature"><span class="s">"conjure.audio.clip"</span></code> | Registered name of the clip asset type. |

## Macros

### `CONJURE_DECLARE_ASSET_TYPE` {#symbol-conjure-declare-asset-type}

```cpp
CONJURE_DECLARE_ASSET_TYPE(conjure::audio::audio_clip, "conjure.audio.clip")
```

## Functions

### Public

#### `make_fallback_clip` {#function-make-fallback-clip}

<div class="function-declaration" id="function-make-fallback-clip-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="clip/audio-clip.html">audio_clip</a> make_fallback_clip();</code></pre></div>

<p>The silent fallback clip: 0.1 s of mono 48 kHz zeros.</p>

#### `make_pcm_clip` {#function-make-pcm-clip}

<div class="function-declaration" id="function-make-pcm-clip-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="clip/audio-clip.html">audio_clip</a> make_pcm_clip(
    std::shared_ptr&lt;<span class="k">const</span> <a href="clip/pcm-buffer.html">pcm_buffer</a>&gt; pcm,
    <a href="audio-types.html#symbol-clip-encoding">clip_encoding</a> encoding = clip_encoding::WAV);</code></pre></div>

<p>Builds a clip around a <code>PCM</code> buffer, filling info from the buffer (encoding <code>WAV</code>, storage <code>PCM</code>).</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="audio_clip"></a>
<a id="audio_clipduration_seconds"></a>
<a id="audio_clipencoded"></a>
<a id="audio_clipinfo"></a>
<a id="audio_clipis_streamed"></a>
<a id="audio_clipis_valid"></a>
<a id="audio_clippcm"></a>
<a id="clip_asset_type_name"></a>
<a id="clip_info"></a>
<a id="clip_infoduration_seconds"></a>
<a id="clip_infoencoding"></a>
<a id="clip_infoformat"></a>
<a id="clip_infoframecount"></a>
<a id="clip_infooperator"></a>
<a id="clip_infostorage"></a>
<a id="conjure_declare_asset_type"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="make_fallback_clip"></a>
<a id="make_pcm_clip"></a>
<a id="other-declarations"></a>
<a id="pcm_buffer"></a>
<a id="pcm_bufferduration_seconds"></a>
<a id="pcm_bufferformat"></a>
<a id="pcm_bufferframe_count"></a>
<a id="pcm_buffermformat"></a>
<a id="pcm_buffermframecount"></a>
<a id="pcm_buffermsamples"></a>
<a id="pcm_bufferoperator"></a>
<a id="pcm_bufferpcm_buffer"></a>
<a id="pcm_bufferpcm_buffer_1"></a>
<a id="pcm_buffersamples"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->

---
kind: header
---

[Engine](../../engine.md) → [Audio](../audio.md)

# audio_types.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/audio/audio_types.h` |
| API ↗ | [Playing sounds](../../../api/audio/playing-sounds.md) |

<p>Formats, identifiers and constants shared by every audio header. The mixer renders interleaved <code><a href="../core/types.html#symbol-f32">f32</a></code> stereo at <code>audio_settings::sampleRate</code>; every other format is converted by the backend (miniaudio) or at import (decoder).</p>

## Structs

| File | Description |
|---|---|
| [`audio_format`](audio-types/audio-format.md) | Sample rate, channel count and sample format of a buffer or of the mixer output. |

## Enumerations

### `attenuation_model` {#symbol-attenuation-model}

```cpp
enum class attenuation_model : u8 {
    NONE,
    INVERSE,
    LINEAR,
    EXPONENTIAL
};
```

Distance attenuation curve of a spatial voice.

### `audio_backend_kind` {#symbol-audio-backend-kind}

```cpp
enum class audio_backend_kind : u8 {
    AUTO,
    MINIAUDIO,
    NULL_BACKEND
};
```

Which backend <code><a href="audio-engine/audio-engine.html">audio_engine</a></code> talks to. <code>AUTO</code> picks <code>MINIAUDIO</code> when it is compiled in and falls back to <code>NULL_BACKEND</code> when no device can be opened.

### `clip_encoding` {#symbol-clip-encoding}

```cpp
enum class clip_encoding : u8 {
    UNKNOWN,
    WAV,
    OGG_VORBIS,
    MP3,
    FLAC
};
```

Container / codec of the source file a clip was imported from.

### `clip_storage` {#symbol-clip-storage}

```cpp
enum class clip_storage : u8 {
    PCM,
    STREAMED
};
```

How a clip keeps its samples: decoded at import (<code>PCM</code>) or decoded incrementally while playing from the compressed bytes kept in memory (<code>STREAMED</code>).

### `device_state` {#symbol-device-state}

```cpp
enum class device_state : u8 {
    STOPPED,
    STARTING,
    RUNNING,
    LOST
};
```

Device state published by a backend (atomic, readable from any thread).

### `sample_format` {#symbol-sample-format}

```cpp
enum class sample_format : u8 {
    F32
};
```

Sample format of <code><a href="clip/pcm-buffer.html">pcm_buffer</a></code> and of the mixer. <code>F32</code> is the only one in v1.

### `voice_state` {#symbol-voice-state}

```cpp
enum class voice_state : u8 {
    FREE,
    PRIMING,
    PLAYING,
    PAUSED,
    STOPPING,
    FINISHED,
    STOPPED
};
```

Lifecycle of a voice. <code>PRIMING</code> covers a streamed voice waiting for its first chunk and a voice waiting for a stolen mixer slot; <code>FINISHED</code> and <code>STOPPED</code> are terminal.

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-COMMAND_RING_CAPACITY"></a><code>COMMAND_RING_CAPACITY</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">8192</span></code> |  |
| <a id="variable-DEFAULT_RAMP_SECONDS"></a><code>DEFAULT_RAMP_SECONDS</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-f32">f32</a></code> | <code class="signature"><span class="mi">0.005</span>f</code> |  |
| <a id="variable-DEFAULT_SPEED_OF_SOUND"></a><code>DEFAULT_SPEED_OF_SOUND</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-f32">f32</a></code> | <code class="signature"><span class="mi">343.0</span>f</code> |  |
| <a id="variable-EVENT_RING_CAPACITY"></a><code>EVENT_RING_CAPACITY</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">4096</span></code> |  |
| <a id="variable-MAX_GROUP_DEPTH"></a><code>MAX_GROUP_DEPTH</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">8</span></code> |  |
| <a id="variable-MAX_GROUPS"></a><code>MAX_GROUPS</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">32</span></code> |  |
| <a id="variable-MAX_SAMPLE_RATE"></a><code>MAX_SAMPLE_RATE</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">192000</span></code> |  |
| <a id="variable-MAX_STREAMING_VOICES"></a><code>MAX_STREAMING_VOICES</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">32</span></code> |  |
| <a id="variable-MAX_VOICES"></a><code>MAX_VOICES</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">256</span></code> |  |
| <a id="variable-MIN_SAMPLE_RATE"></a><code>MIN_SAMPLE_RATE</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">8000</span></code> |  |
| <a id="variable-MIX_BLOCK_FRAMES"></a><code>MIX_BLOCK_FRAMES</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">256</span></code> |  |
| <a id="variable-MIX_CHANNELS"></a><code>MIX_CHANNELS</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">2</span></code> |  |
| <a id="variable-STEAL_FADE_SECONDS"></a><code>STEAL_FADE_SECONDS</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-f32">f32</a></code> | <code class="signature"><span class="mi">0.002</span>f</code> |  |
| <a id="variable-STREAM_CHUNK_FRAMES"></a><code>STREAM_CHUNK_FRAMES</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">4096</span></code> |  |
| <a id="variable-STREAM_RING_FRAMES"></a><code>STREAM_RING_FRAMES</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">16384</span></code> |  |

## Macros

### `CONJURE_AUDIO_ENUM_FORMATTER` {#symbol-conjure-audio-enum-formatter}

```cpp
CONJURE_AUDIO_ENUM_FORMATTER(conjure::audio::sample_format)
```

## Functions

### Public

#### `is_active_state` {#function-is-active-state}

<div class="function-declaration" id="function-is-active-state-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> is_active_state(
    <a href="audio-types.html#symbol-voice-state">voice_state</a> state) <span class="k">noexcept</span>;</code></pre></div>

<p>True for <code>PRIMING</code>, <code>PLAYING</code> and <code>PAUSED</code>: the states in which <code>audio_engine::is_playing()</code> holds.</p>

#### `is_terminal_state` {#function-is-terminal-state}

<div class="function-declaration" id="function-is-terminal-state-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> is_terminal_state(
    <a href="audio-types.html#symbol-voice-state">voice_state</a> state) <span class="k">noexcept</span>;</code></pre></div>

<p>True for <code>FINISHED</code> and <code>STOPPED</code>: the voice has been acknowledged as ended by the mixer.</p>

#### `to_string` {#function-to-string}

<div class="function-declaration" id="function-to-string-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="audio-types.html#symbol-sample-format">sample_format</a> value) <span class="k">noexcept</span>;</code></pre></div>

<p>Enumerator names (&quot;<code>F32</code>&quot;, &quot;<code>PCM</code>&quot;, &quot;<code>PLAYING</code>&quot;, ...); &quot;<code>UNKNOWN</code>&quot; for out-of-range values.</p>

<div class="function-declaration" id="function-to-string-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="audio-types.html#symbol-clip-storage">clip_storage</a> value) <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-to-string-3"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="audio-types.html#symbol-clip-encoding">clip_encoding</a> value) <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-to-string-4"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="audio-types.html#symbol-audio-backend-kind">audio_backend_kind</a> value) <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-to-string-5"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="audio-types.html#symbol-device-state">device_state</a> value) <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-to-string-6"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="audio-types.html#symbol-voice-state">voice_state</a> value) <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-to-string-7"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="audio-types.html#symbol-attenuation-model">attenuation_model</a> value) <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="attenuation_model"></a>
<a id="attenuation_modelexponential"></a>
<a id="attenuation_modelinverse"></a>
<a id="attenuation_modellinear"></a>
<a id="attenuation_modelnone"></a>
<a id="audio_backend_kind"></a>
<a id="audio_backend_kindauto"></a>
<a id="audio_backend_kindminiaudio"></a>
<a id="audio_backend_kindnull_backend"></a>
<a id="audio_format"></a>
<a id="audio_formatchannels"></a>
<a id="audio_formatformat"></a>
<a id="audio_formatoperator"></a>
<a id="audio_formatsamplerate"></a>
<a id="classes"></a>
<a id="clip_encoding"></a>
<a id="clip_encodingflac"></a>
<a id="clip_encodingmp3"></a>
<a id="clip_encodingogg_vorbis"></a>
<a id="clip_encodingunknown"></a>
<a id="clip_encodingwav"></a>
<a id="clip_storage"></a>
<a id="clip_storagepcm"></a>
<a id="clip_storagestreamed"></a>
<a id="command_ring_capacity"></a>
<a id="conjure_audio_enum_formatter"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="default_ramp_seconds"></a>
<a id="default_speed_of_sound"></a>
<a id="dependencies"></a>
<a id="device_state"></a>
<a id="device_statelost"></a>
<a id="device_staterunning"></a>
<a id="device_statestarting"></a>
<a id="device_statestopped"></a>
<a id="direct-header-dependencies"></a>
<a id="event_ring_capacity"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="is_active_state"></a>
<a id="is_terminal_state"></a>
<a id="max_group_depth"></a>
<a id="max_groups"></a>
<a id="max_sample_rate"></a>
<a id="max_streaming_voices"></a>
<a id="max_voices"></a>
<a id="min_sample_rate"></a>
<a id="mix_block_frames"></a>
<a id="mix_channels"></a>
<a id="other-declarations"></a>
<a id="sample_format"></a>
<a id="sample_formatf32"></a>
<a id="steal_fade_seconds"></a>
<a id="stream_chunk_frames"></a>
<a id="stream_ring_frames"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="to_string"></a>
<a id="variables-and-properties"></a>
<a id="voice_state"></a>
<a id="voice_statefinished"></a>
<a id="voice_statefree"></a>
<a id="voice_statepaused"></a>
<a id="voice_stateplaying"></a>
<a id="voice_statepriming"></a>
<a id="voice_statestopped"></a>
<a id="voice_statestopping"></a>
<!-- doc-legacy-anchors:end -->

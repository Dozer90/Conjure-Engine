---
kind: header
---

[Engine](../../engine.md) → [Audio](../audio.md)

# decoder.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/audio/decoder.h` |
| API ↗ | [Clips and decoding](../../../api/audio/clips-and-decoding.md) |

<p>Decoding. Everything here goes through miniaudio&#x27;s decoders (<code>WAV</code>, <code>MP3</code>, <code>FLAC</code> built in, Ogg Vorbis through <code>stb_vorbis</code> compiled into the miniaudio translation unit) and is backend independent: the <code>null</code> backend decodes exactly like the real one. Safe to call from any thread; a <code><a href="decoder/streaming-decoder.html">streaming_decoder</a></code> instance is used by one thread at a time.</p>

## Structs

| File | Description |
|---|---|
| [`decode_options`](decoder/decode-options.md) | Options of <code>decode_to_pcm</code>: downmix to mono, resample (0 keeps the source rate) and scale the peak to 1. |

## Classes

| File | Description |
|---|---|
| [`streaming_decoder`](decoder/streaming-decoder.md) | Incremental decoder over caller-owned encoded bytes, used by streamed voices. |

## Forward Declarations

### `streaming_decoder::state` {#symbol-streaming-decoder-state}

```cpp
struct state;
```

## Functions

### Public

#### `decode_to_pcm` {#function-decode-to-pcm}

<div class="function-declaration" id="function-decode-to-pcm-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<a href="clip/pcm-buffer.html">pcm_buffer</a>&gt; decode_to_pcm(
    std::span&lt;<span class="k">const</span> <a href="../core/types.html#symbol-byte">byte</a>&gt; encoded,
    <span class="k">const</span> <a href="decoder/decode-options.html">decode_options</a> &amp;options);</code></pre></div>

<p>Decodes a whole file to interleaved <code><a href="../core/types.html#symbol-f32">f32</a></code> at its native channel count and rate, then applies the options in order: <code>forceMono</code>, <code>sampleRate</code> (miniaudio linear resampler), normalize. Same errors as <code><a href="#function-probe-1">probe</a>()</code>.</p>

#### `probe` {#function-probe}

<div class="function-declaration" id="function-probe-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<a href="clip/clip-info.html">clip_info</a>&gt; probe(
    std::span&lt;<span class="k">const</span> <a href="../core/types.html#symbol-byte">byte</a>&gt; encoded);</code></pre></div>

<p>Opens a decoder to read format and length. Errors: <code>INVALID_CLIP</code> (empty), <code>UNSUPPORTED_FORMAT</code> (unknown magic), <code>DECODE_FAILED</code> (miniaudio refused the data; the message names its result).</p>

#### `sniff_encoding` {#function-sniff-encoding}

<div class="function-declaration" id="function-sniff-encoding-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="audio-types.html#symbol-clip-encoding">clip_encoding</a> sniff_encoding(
    std::span&lt;<span class="k">const</span> <a href="../core/types.html#symbol-byte">byte</a>&gt; encoded) <span class="k">noexcept</span>;</code></pre></div>

<p>Sniffs the container magic (RIFF/WAVE, OggS, ID3 / <code>MP3</code> sync, fLaC). <code>UNKNOWN</code> when none matches.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="decode_options"></a>
<a id="decode_optionsforcemono"></a>
<a id="decode_optionsnormalize"></a>
<a id="decode_optionsoperator"></a>
<a id="decode_optionssamplerate"></a>
<a id="decode_to_pcm"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="probe"></a>
<a id="sniff_encoding"></a>
<a id="streaming_decoder"></a>
<a id="streaming_decoderclose"></a>
<a id="streaming_decoderinfo"></a>
<a id="streaming_decoderis_open"></a>
<a id="streaming_decoderminfo"></a>
<a id="streaming_decodermposition"></a>
<a id="streaming_decodermstate"></a>
<a id="streaming_decoderopen"></a>
<a id="streaming_decoderoperator"></a>
<a id="streaming_decoderposition"></a>
<a id="streaming_decoderread"></a>
<a id="streaming_decoderseek"></a>
<a id="streaming_decoderstate"></a>
<a id="streaming_decoderstreaming_decoder"></a>
<a id="streaming_decoderstreaming_decoder_1"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->

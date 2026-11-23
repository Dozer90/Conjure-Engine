---
kind: header
---

[Engine](../../engine.md) → [Audio](../audio.md)

# voice.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/audio/voice.h` |
| API ↗ | [Playing sounds](../../../api/audio/playing-sounds.md) |

<p>Voice identity and parameters.</p>

## Structs

| File | Description |
|---|---|
| [`play_params`](voice/play-params.md) | Everything <code>play()</code> needs. Values outside the documented ranges are clamped at the API boundary; NaN becomes the default. |
| [`voice_id`](voice/voice-id.md) | Handle of a voice: the slot key of the engine&#x27;s voice table. 8 bytes, trivially copyable, mirrored by the <code>C</code># AudioSource struct. <code>A</code> stale id is rejected by every engine call. |
| [`voice_info`](voice/voice-info.md) | Main-thread mirror of a voice, refreshed by <code>audio_engine::update()</code> and <code>end_frame()</code>. |

## Functions

### Public

#### `clamp_pan` {#function-clamp-pan}

<div class="function-declaration" id="function-clamp-pan-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/types.html#symbol-f32">f32</a> clamp_pan(
    <a href="../core/types.html#symbol-f32">f32</a> pan) <span class="k">noexcept</span>;</code></pre></div>

#### `clamp_pitch` {#function-clamp-pitch}

<div class="function-declaration" id="function-clamp-pitch-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/types.html#symbol-f32">f32</a> clamp_pitch(
    <a href="../core/types.html#symbol-f32">f32</a> pitch) <span class="k">noexcept</span>;</code></pre></div>

#### `clamp_play_params` {#function-clamp-play-params}

<div class="function-declaration" id="function-clamp-play-params-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="voice/play-params.html">play_params</a> clamp_play_params(
    <span class="k">const</span> <a href="voice/play-params.html">play_params</a> &amp;params) <span class="k">noexcept</span>;</code></pre></div>

<p>Clamp rules of applied to every field: volume [0, 2], pitch [0.25, 4], pan [-1, 1], <code>minDistance</code> &gt;= 0.01, <code>maxDistance</code> &gt;= <code>minDistance</code>, rolloff [0, 10], <code>startSeconds</code> &gt;= 0; NaN becomes the field&#x27;s default.</p>

#### `clamp_volume` {#function-clamp-volume}

<div class="function-declaration" id="function-clamp-volume-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/types.html#symbol-f32">f32</a> clamp_volume(
    <a href="../core/types.html#symbol-f32">f32</a> volume) <span class="k">noexcept</span>;</code></pre></div>

<p>Individual clamp helpers shared by <code><a href="voice/play-params.html">play_params</a></code> and the engine setters.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="clamp_pan"></a>
<a id="clamp_pitch"></a>
<a id="clamp_play_params"></a>
<a id="clamp_volume"></a>
<a id="classes"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="play_params"></a>
<a id="play_paramsattenuation"></a>
<a id="play_paramsgroup"></a>
<a id="play_paramsloop"></a>
<a id="play_paramsmaxdistance"></a>
<a id="play_paramsmindistance"></a>
<a id="play_paramsoperator"></a>
<a id="play_paramspan"></a>
<a id="play_paramspitch"></a>
<a id="play_paramspriority"></a>
<a id="play_paramsretain"></a>
<a id="play_paramsrolloff"></a>
<a id="play_paramsspatial"></a>
<a id="play_paramsstartpaused"></a>
<a id="play_paramsstartseconds"></a>
<a id="play_paramsvolume"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<a id="voice_id"></a>
<a id="voice_idfrom_u64"></a>
<a id="voice_idis_valid"></a>
<a id="voice_idkey"></a>
<a id="voice_idnull"></a>
<a id="voice_idoperator"></a>
<a id="voice_idoperator_1"></a>
<a id="voice_idto_u64"></a>
<a id="voice_info"></a>
<a id="voice_infoclipframes"></a>
<a id="voice_infoclipversion"></a>
<a id="voice_infocursorframes"></a>
<a id="voice_infoeffectivegain"></a>
<a id="voice_infogroup"></a>
<a id="voice_infoloop"></a>
<a id="voice_infooperator"></a>
<a id="voice_infopitch"></a>
<a id="voice_inforetain"></a>
<a id="voice_infospatial"></a>
<a id="voice_infostate"></a>
<a id="voice_infostreamed"></a>
<a id="voice_infovolume"></a>
<!-- doc-legacy-anchors:end -->

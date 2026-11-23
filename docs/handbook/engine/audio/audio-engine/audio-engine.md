---
kind: class
---

[Engine](../../../engine.md) → [Audio](../../audio.md) → [audio_engine.h](../audio-engine.md)

# audio_engine
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::audio` |
| Include File Path | `engine/include/conjure/audio/audio_engine.h` |
| Source File Path | `engine/src/audio/audio_engine.cpp` |
| API ↗ | [Clips and decoding](../../../../api/audio/clips-and-decoding.md), [Audio](../../../../api/audio/index.md), [Playing sounds](../../../../api/audio/playing-sounds.md) |

## Syntax

```cpp
class CONJURE_API audio_engine
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">explicit</span> audio_engine(
    <a href="audio-settings.html">audio_settings</a> settings);</code></td><td>Never fails: an invalid setting or a backend that cannot start leaves the engine on the <code>null</code> backend with <code>last_backend_error()</code> set.</td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~audio_engine();</code></td><td>Stops every voice, stops the backend and drains the acknowledgements.</td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mState</code></td><td><code class="signature">std::unique_ptr&lt;<a href="../../core/event/event-state.html">state</a>&gt;</code></td><td><code class="signature">nullptr</code></td><td>Private implementation state owning the mixer, command/event rings, voice and group tables, and backend.</td></tr>
</tbody>
</table>

## Functions

### Public

#### `active_voice_count` {#function-active-voice-count}

<div class="function-declaration" id="function-active-voice-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> active_voice_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Voices in <code>PRIMING</code>, <code>PLAYING</code>, <code>PAUSED</code> or <code>STOPPING</code>.</p>

#### `apply_params` {#function-apply-params}

<div class="function-declaration" id="function-apply-params-1"><pre><code><span class="k">void</span> apply_params(
    <a href="../voice/voice-id.html">voice_id</a> voice,
    <span class="k">const</span> <a href="../voice/play-params.html">play_params</a> &amp;params);</code></pre></div>

<p>Applies volume, pitch, pan, loop, spatial flag, priority, group and attenuation of <code>params</code> at once (what <code>apply_source</code> forwards to). An invalid group leaves the voice&#x27;s group unchanged.</p>

#### `apply_source` {#function-apply-source}

<div class="function-declaration" id="function-apply-source-1"><pre><code><span class="k">void</span> apply_source(
    <a href="../voice/voice-id.html">voice_id</a> voice,
    <span class="k">const</span> <a href="../components/audio-source.html">audio_source</a> &amp;source,
    <a href="../mix-groups/group-id.html">group_id</a> group);</code></pre></div>

<p>Applies every authored field of an <code><a href="../components/audio-source.html">audio_source</a></code> component to its voice at once (<code>engine.audio_sync</code> calls it for <code>changed&lt;<a href="../components/audio-source.html">audio_source</a>&gt;</code> chunks): <code><a href="#function-apply-params-1">apply_params</a>(voice, play_params_from(source, group))</code> with <code>group</code> the resolution of <code>source.group</code> through <code><a href="#function-find-group-1">find_group</a>()</code>. No-op on a stale voice id.</p>

#### `backend_kind` {#function-backend-kind}

<div class="function-declaration" id="function-backend-kind-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../audio-types.html#symbol-audio-backend-kind">audio_backend_kind</a> backend_kind() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The backend in use (never <code>AUTO</code>).</p>

#### `create_group` {#function-create-group}

<div class="function-declaration" id="function-create-group-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="../mix-groups/group-id.html">group_id</a>&gt; create_group(
    <span class="k">const</span> <a href="../mix-groups/group-desc.html">group_desc</a> &amp;desc);</code></pre></div>

<p>Creates a group under <code>desc.parent</code> (or <code>desc.parentName</code>). Its own paused flag starts clear, but a group created under a paused ancestor is frozen from the start. Errors: <code>GROUP_LIMIT_REACHED</code>, <code>DUPLICATE_GROUP</code> (name taken or <code>null</code>), <code>INVALID_GROUP</code> (unknown parent id or name), <code>GROUP_CYCLE</code> (depth would exceed <code>MAX_GROUP_DEPTH</code>).</p>

#### `end_frame` {#function-end-frame}

<div class="function-declaration" id="function-end-frame-1"><pre><code><span class="k">void</span> end_frame();</code></pre></div>

<p>Erases every acknowledged one-shot and publishes the ids that ended this frame through <code><a href="#function-finished-voices-1">finished_voices</a>()</code>. Main thread.</p>

#### `enumerate_devices` {#function-enumerate-devices}

<div class="function-declaration" id="function-enumerate-devices-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;std::vector&lt;<a href="../audio-device/audio-device-info.html">audio_device_info</a>&gt;&gt; enumerate_devices() <span class="k">const</span>;</code></pre></div>

<p>Playback devices of the current backend.</p>

#### `find_group` {#function-find-group}

<div class="function-declaration" id="function-find-group-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../mix-groups/group-id.html">group_id</a> find_group(
    <a href="../../core/string-id/string-id.html">core::string_id</a> name) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The group named <code>name</code>; <code>MASTER_GROUP</code> for the <code>null</code> name; the <code>null</code> id when unknown.</p>

#### `finished_voices` {#function-finished-voices}

<div class="function-declaration" id="function-finished-voices-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> <a href="../voice/voice-id.html">voice_id</a>&gt; finished_voices() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Voices that reached <code>FINISHED</code> or <code>STOPPED</code> during the last frame, each reported once. Valid until the next <code><a href="#function-end-frame-1">end_frame</a>()</code>.</p>

#### `group` {#function-group}

<div class="function-declaration" id="function-group-1"><pre><code>[[<span class="na">nodiscard</span>]] std::optional&lt;<a href="../mix-groups/group-info.html">group_info</a>&gt; group(
    <a href="../mix-groups/group-id.html">group_id</a> group) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Name, parent, gain, effective gain, own paused flag and effective paused flag (own or any ancestor&#x27;s) of a live group; nullopt for a stale id. Main thread.</p>

#### `groups` {#function-groups}

<div class="function-declaration" id="function-groups-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> <a href="../mix-groups/group-info.html">group_info</a>&gt; groups() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Every live group, master first, in slot order. Valid until the next group change.</p>

#### `is_paused` {#function-is-paused}

<div class="function-declaration" id="function-is-paused-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_paused() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True after <code><a href="#function-set-paused-1">set_paused</a>(true)</code>. Main thread.</p>

#### `is_playing` {#function-is-playing}

<div class="function-declaration" id="function-is-playing-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_playing(
    <a href="../voice/voice-id.html">voice_id</a> voice) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True for <code>PRIMING</code>, <code>PLAYING</code> or <code>PAUSED</code>. Any thread.</p>

#### `is_valid` {#function-is-valid}

<div class="function-declaration" id="function-is-valid-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_valid(
    <a href="../voice/voice-id.html">voice_id</a> voice) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True while the id addresses a voice the engine still holds (any state but <code>FREE</code>). Any thread.</p>

#### `last_backend_error` {#function-last-backend-error}

<div class="function-declaration" id="function-last-backend-error-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../../core/error/error.html">core::error</a> *last_backend_error() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Why the requested backend was replaced by <code>null</code>, or <code>nullptr</code> when it was not.</p>

#### `listener` {#function-listener}

<div class="function-declaration" id="function-listener-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../spatialisation/listener-state.html">listener_state</a> &amp;listener() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The listener as last set (after clamping). Main thread.</p>

#### `null_backend_instance` {#function-null-backend-instance}

<div class="function-declaration" id="function-null-backend-instance-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../null-backend/null-backend.html">null_backend</a> *null_backend_instance() <span class="k">noexcept</span>;</code></pre></div>

<p>The <code>null</code> backend when it is the one in use, <code>nullptr</code> on miniaudio.</p>

#### `output_format` {#function-output-format}

<div class="function-declaration" id="function-output-format-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../audio-types/audio-format.html">audio_format</a> output_format() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The format the mixer renders at.</p>

#### `pause` {#function-pause}

<div class="function-declaration" id="function-pause-1"><pre><code><span class="k">void</span> pause(
    <a href="../voice/voice-id.html">voice_id</a> voice);</code></pre></div>

<p>Holds the cursor (the voice stays <code><a href="#function-is-playing-1">is_playing</a>()</code>); no-op on an invalid id or a voice that is not <code>PLAYING</code>. <code>A</code> <code>PRIMING</code> voice starts paused.</p>

#### `play` {#function-play}

<div class="function-declaration" id="function-play-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="../voice/voice-id.html">voice_id</a>&gt; play(
    <span class="k">const</span> <a href="../../assets/asset-ref/asset-ref.html">assets::asset_ref</a>&lt;<a href="../clip/audio-clip.html">audio_clip</a>&gt; &amp;clip,
    <span class="k">const</span> <a href="../voice/play-params.html">play_params</a> &amp;params = {});</code></pre></div>

<p>Starts a voice on a loaded clip asset (<code>CLIP_NOT_LOADED</code> while <code>get()</code> is <code>null</code>), recording the ref&#x27;s version for reload detection. Other errors as <code><a href="#function-play-2">play</a>(const <a href="../clip/audio-clip.html">audio_clip</a> &amp;)</code>.</p>

<div class="function-declaration" id="function-play-2"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="../voice/voice-id.html">voice_id</a>&gt; play(
    <span class="k">const</span> <a href="../clip/audio-clip.html">audio_clip</a> &amp;clip,
    <span class="k">const</span> <a href="../voice/play-params.html">play_params</a> &amp;params = {},
    <a href="../../core/types.html#symbol-u32">u32</a> clipVersion = <span class="mi">0</span>);</code></pre></div>

<p>Starts a voice on a clip payload, sharing its <code>PCM</code> or encoded bytes. <code>clipVersion</code> is recorded in <code><a href="../voice/voice-info.html">voice_info</a></code> for reload detection. Errors: <code>INVALID_CLIP</code> (clip without its buffer), <code>INVALID_GROUP</code> (<code>params.group</code> stale or unknown), <code>STREAM_SLOT_LIMIT</code>, <code>VOICE_LIMIT_REACHED</code>, <code>DECODE_FAILED</code> (streamed clip that does not open; its stream slot is released again).</p>

<div class="function-declaration" id="function-play-3"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="../voice/voice-id.html">voice_id</a>&gt; play(
    std::shared_ptr&lt;<span class="k">const</span> <a href="../clip/pcm-buffer.html">pcm_buffer</a>&gt; pcm,
    <span class="k">const</span> <a href="../voice/play-params.html">play_params</a> &amp;params = {});</code></pre></div>

<p>Starts a voice on raw <code>PCM</code> (tests, tools: no asset manager). <code>INVALID_CLIP</code> for a <code>null</code> or empty buffer.</p>

#### `release` {#function-release}

<div class="function-declaration" id="function-release-1"><pre><code><span class="k">void</span> release(
    <a href="../voice/voice-id.html">voice_id</a> voice);</code></pre></div>

<p>Gives a retained voice back: stops it if it is still playing and erases it once the mixer acknowledged. One-shot voices are released by <code><a href="#function-end-frame-1">end_frame</a>()</code> instead (calling release on one is harmless).</p>

#### `remove_group` {#function-remove-group}

<div class="function-declaration" id="function-remove-group-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; remove_group(
    <a href="../mix-groups/group-id.html">group_id</a> group);</code></pre></div>

<p>Reparents children and moves voices to the parent before retiring the id. <code>A</code> pause the group held goes with it: its children and voices follow the parent&#x27;s effective paused flag from then on. <code>INVALID_GROUP</code> for <code>MASTER_GROUP</code> or a stale id.</p>

#### `resume` {#function-resume}

<div class="function-declaration" id="function-resume-1"><pre><code><span class="k">void</span> resume(
    <a href="../voice/voice-id.html">voice_id</a> voice);</code></pre></div>

<p>Continues a paused voice from its cursor; no-op on an invalid id or a voice that is not <code>PAUSED</code>. During a pending streamed seek the resume takes effect once it lands. On a voice still <code>PRIMING</code> it cancels startPaused or an earlier <code><a href="#function-pause-1">pause</a>()</code>: the voice starts playing.</p>

#### `seek` {#function-seek}

<div class="function-declaration" id="function-seek-1"><pre><code><span class="k">void</span> seek(
    <a href="../voice/voice-id.html">voice_id</a> voice,
    <a href="../../core/types.html#symbol-f64">f64</a> seconds);</code></pre></div>

<p>Moves the cursor to <code>seconds</code> (clamped to the clip length). <code>A</code> <code>PCM</code> voice jumps at the next block; a streamed voice pauses, refills its ring from the new position and resumes a few frames later (a later seek before that lands retargets it). When the decoder cannot reach the position, the voice continues from where its decoder stands and <code><a href="#function-voice-1">voice</a>()</code> reports that frame. No-op on a stale or ended voice.</p>

#### `select_device` {#function-select-device}

<div class="function-declaration" id="function-select-device-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; select_device(
    std::string_view deviceName);</code></pre></div>

<p>Restarts the backend on the named device (empty = default). Voices are frozen during the restart and continue afterwards. On failure the previous device is kept. An unknown non-empty name falls back to the default device with a <code>WARN</code> and counts as success. If neither the new nor the previous device starts, the engine continues on the <code>null</code> backend with <code><a href="#function-last-backend-error-1">last_backend_error</a>()</code> set and <code><a href="#function-stats-1">stats</a>()</code>.<code>fellBackToNull</code> <code>true</code>; the error of the new device is returned.</p>

#### `set_attenuation` {#function-set-attenuation}

<div class="function-declaration" id="function-set-attenuation-1"><pre><code><span class="k">void</span> set_attenuation(
    <a href="../voice/voice-id.html">voice_id</a> voice,
    <span class="k">const</span> <a href="../spatialisation/attenuation-params.html">attenuation_params</a> &amp;params);</code></pre></div>

<p>Attenuation model and distances of a spatial voice, clamped per (minDistance &gt;= 0.01, maxDistance &gt;= minDistance, rolloff in [0, 10]); applied by the next <code><a href="#function-update-1">update</a>()</code>. No-op on a stale id.</p>

#### `set_group` {#function-set-group}

<div class="function-declaration" id="function-set-group-1"><pre><code><span class="k">void</span> set_group(
    <a href="../voice/voice-id.html">voice_id</a> voice,
    <a href="../mix-groups/group-id.html">group_id</a> group);</code></pre></div>

<p>Moves the voice to a live group; an invalid or stale group id is a no-op with a <code>TRACE</code>, as is a stale voice id.</p>

#### `set_group_gain` {#function-set-group-gain}

<div class="function-declaration" id="function-set-group-gain-1"><pre><code><span class="k">void</span> set_group_gain(
    <a href="../mix-groups/group-id.html">group_id</a> group,
    <a href="../../core/types.html#symbol-f32">f32</a> gain);</code></pre></div>

<p>Gain clamped to [0, 2]; the effective gains of the group and its descendants are recomputed and sent. NaN asserts and becomes 1. Stale id: no-op with a <code>TRACE</code>.</p>

#### `set_group_paused` {#function-set-group-paused}

<div class="function-declaration" id="function-set-group-paused-1"><pre><code><span class="k">void</span> set_group_paused(
    <a href="../mix-groups/group-id.html">group_id</a> group,
    <span class="k">bool</span> paused);</code></pre></div>

<p>Sets the group&#x27;s own paused flag. Pause is hierarchical, like gain: a paused group freezes its own voices and those of every descendant group (cursors hold, output is silence), and a group plays again only once neither it nor any ancestor is paused, so releasing a child under a paused parent changes <code>group_info::paused</code> but not <code>group_info::effectivePaused</code>. <code><a href="#function-set-group-paused-1">set_group_paused</a>(MASTER_GROUP, true)</code> therefore freezes every voice; it is a flag of its own, independent of <code><a href="#function-set-paused-1">set_paused</a>()</code> and <code><a href="#function-is-paused-1">is_paused</a>()</code>. The flags reach the mixer as one command per group, so groups of one subtree may freeze or resume at different times: normally at most one <code>MIX_BLOCK_FRAMES</code> block apart, and with no upper bound when the main thread is interrupted in the middle of the call or the command ring spills. Voices that must stay sample-synchronised belong in the same group, or are held with <code><a href="#function-set-paused-1">set_paused</a>()</code>. Stale id: no-op with a <code>TRACE</code>.</p>

#### `set_listener` {#function-set-listener}

<div class="function-declaration" id="function-set-listener-1"><pre><code><span class="k">void</span> set_listener(
    <span class="k">const</span> <a href="../spatialisation/listener-state.html">listener_state</a> &amp;listener);</code></pre></div>

<p>The single listener (<code>clamp_listener</code> applied: basis normalised, gain in [0, 2], non-finite values sanitised). Spatial voices pick it up at the next <code><a href="#function-update-1">update</a>()</code>.</p>

#### `set_loop` {#function-set-loop}

<div class="function-declaration" id="function-set-loop-1"><pre><code><span class="k">void</span> set_loop(
    <a href="../voice/voice-id.html">voice_id</a> voice,
    <span class="k">bool</span> loop);</code></pre></div>

<p>Loops at the clip end (streamed voices rewind their decoder). No-op on a stale id.</p>

#### `set_master_gain` {#function-set-master-gain}

<div class="function-declaration" id="function-set-master-gain-1"><pre><code><span class="k">void</span> set_master_gain(
    <a href="../../core/types.html#symbol-f32">f32</a> gain);</code></pre></div>

<p><code><a href="#function-set-group-gain-1">set_group_gain</a>(MASTER_GROUP, gain)</code>.</p>

#### `set_pan` {#function-set-pan}

<div class="function-declaration" id="function-set-pan-1"><pre><code><span class="k">void</span> set_pan(
    <a href="../voice/voice-id.html">voice_id</a> voice,
    <a href="../../core/types.html#symbol-f32">f32</a> pan);</code></pre></div>

<p>Pan clamped to [-1, 1], used only while the voice is not spatial (a spatial voice pans from its position). NaN asserts and becomes 0. No-op with a <code>TRACE</code> on a stale id.</p>

#### `set_paused` {#function-set-paused}

<div class="function-declaration" id="function-set-paused-1"><pre><code><span class="k">void</span> set_paused(
    <span class="k">bool</span> paused);</code></pre></div>

<p>Pauses or resumes the whole mixer; cursors hold, output is silence. Independent of the group flags: <code>group_info::effectivePaused</code> does not report it.</p>

#### `set_pitch` {#function-set-pitch}

<div class="function-declaration" id="function-set-pitch-1"><pre><code><span class="k">void</span> set_pitch(
    <a href="../voice/voice-id.html">voice_id</a> voice,
    <a href="../../core/types.html#symbol-f32">f32</a> pitch);</code></pre></div>

<p>Pitch clamped to [0.25, 4]; the mixer step changes at the next block. NaN asserts and becomes 1. No-op with a <code>TRACE</code> on a stale id.</p>

#### `set_position` {#function-set-position}

<div class="function-declaration" id="function-set-position-1"><pre><code><span class="k">void</span> set_position(
    <a href="../voice/voice-id.html">voice_id</a> voice,
    <a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a> position,
    <a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a> velocity = <a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a>(<span class="mi">0.0</span>f));</code></pre></div>

<p>World position and velocity of a spatial voice; applied by the next <code><a href="#function-update-1">update</a>()</code>.</p>

#### `set_positions` {#function-set-positions}

<div class="function-declaration" id="function-set-positions-1"><pre><code><span class="k">void</span> set_positions(
    std::span&lt;<span class="k">const</span> <a href="voice-transform.html">voice_transform</a>&gt; transforms);</code></pre></div>

<p><code><a href="#function-set-position-1">set_position</a>()</code> for every entry; one call per frame from <code>engine.audio_sync</code>.</p>

#### `set_priority` {#function-set-priority}

<div class="function-declaration" id="function-set-priority-1"><pre><code><span class="k">void</span> set_priority(
    <a href="../voice/voice-id.html">voice_id</a> voice,
    <a href="../../core/types.html#symbol-u8">u8</a> priority);</code></pre></div>

<p>Voice-stealing priority; lower is stolen first. No-op on a stale id.</p>

#### `set_spatial` {#function-set-spatial}

<div class="function-declaration" id="function-set-spatial-1"><pre><code><span class="k">void</span> set_spatial(
    <a href="../voice/voice-id.html">voice_id</a> voice,
    <span class="k">bool</span> spatial);</code></pre></div>

<p>Switches between flat playback (pan, unity gain) and 3D playback (attenuation and pan from <code><a href="#function-set-position-1">set_position</a>()</code>; a stereo clip is downmixed to mono). No-op on a stale id.</p>

#### `set_volume` {#function-set-volume}

<div class="function-declaration" id="function-set-volume-1"><pre><code><span class="k">void</span> set_volume(
    <a href="../voice/voice-id.html">voice_id</a> voice,
    <a href="../../core/types.html#symbol-f32">f32</a> volume);</code></pre></div>

<p>Volume clamped to [0, 2], ramped over <code>DEFAULT_RAMP_SECONDS</code> in the mixer. NaN asserts and becomes 1. No-op with a <code>TRACE</code> on a stale id. Main thread, like every setter.</p>

#### `settings` {#function-settings}

<div class="function-declaration" id="function-settings-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="audio-settings.html">audio_settings</a> &amp;settings() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The settings the engine was constructed from.</p>

#### `stats` {#function-stats}

<div class="function-declaration" id="function-stats-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="audio-engine-stats.html">audio_engine_stats</a> stats() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Counters; lock-free, any thread (<code>outputFormat</code> is fixed at construction, the rest are relaxed atomics).</p>

#### `stop` {#function-stop}

<div class="function-declaration" id="function-stop-1"><pre><code><span class="k">void</span> stop(
    <a href="../voice/voice-id.html">voice_id</a> voice,
    <a href="../../core/types.html#symbol-f32">f32</a> fadeSeconds = DEFAULT_RAMP_SECONDS);</code></pre></div>

<p>Fades the voice out over <code>fadeSeconds</code>, then it reports <code>STOPPED</code>. No-op for an invalid id or an already ended voice. <code>A</code> voice that is paused, or whose group is frozen (paused itself or through an ancestor group, <code>group_info::effectivePaused</code>), or while the master is paused, ends at once without a fade.</p>

#### `stop_all` {#function-stop-all}

<div class="function-declaration" id="function-stop-all-1"><pre><code><span class="k">void</span> stop_all(
    <a href="../../core/types.html#symbol-f32">f32</a> fadeSeconds = DEFAULT_RAMP_SECONDS);</code></pre></div>

<p><code><a href="#function-stop-1">stop</a>()</code> for every live voice.</p>

#### `update` {#function-update}

<div class="function-declaration" id="function-update-1"><pre><code><span class="k">void</span> update(
    <a href="../../core/types.html#symbol-f32">f32</a> deltaSeconds);</code></pre></div>

<p>Once per frame: backend update, event drain, spatial pass, streaming pass, spill flush, stats. Main thread.</p>

#### `voice` {#function-voice}

<div class="function-declaration" id="function-voice-1"><pre><code>[[<span class="na">nodiscard</span>]] std::optional&lt;<a href="../voice/voice-info.html">voice_info</a>&gt; voice(
    <a href="../voice/voice-id.html">voice_id</a> voice) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The voice&#x27;s mirror, or nullopt for an invalid id. Any thread: state and cursor come from atomics the mixer writes; the other fields are a snapshot the main thread publishes (seqlock) whenever <code><a href="#function-play-1">play</a>()</code>, a setter or <code><a href="#function-update-1">update</a>()</code> changes them.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

---
kind: class
---

[Engine](../../../engine.md) → [Audio](../../audio.md) → [null_backend.h](../null-backend.md)

# null_backend
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::audio` |
| Include File Path | `engine/include/conjure/audio/null_backend.h` |
| Source File Path | `engine/src/audio/null_backend.cpp` |
| API ↗ | [Clips and decoding](../../../../api/audio/clips-and-decoding.md) |

## Syntax

```cpp
class CONJURE_API null_backend final : public audio_backend
```

## Inheritance Hierarchy


[`audio_backend`](../audio-backend/audio-backend.md) → `null_backend`

## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">null_backend();</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~null_backend() <span class="k">override</span>;</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mCaptured</code></td><td><code class="signature">std::vector&lt;<a href="../../core/types.html#symbol-f32">f32</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Capture<wbr>Frames</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mDesc</code></td><td><code class="signature"><a href="../audio-device/backend-desc.html">backend_desc</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Failing<wbr>Starts</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Fractional<wbr>Frames</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f64">f64</a></code></td><td><code class="signature"><span class="mi">0.0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Render<wbr>Buffer</code></td><td><code class="signature">std::vector&lt;<a href="../../core/types.html#symbol-f32">f32</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mState</code></td><td><code class="signature">std::<wbr>atomic&lt;<wbr><a href="../audio-types.html#symbol-device-state">device_<wbr>state</a>&gt;</code></td><td><code class="signature">device_<wbr>state::<wbr>STOPPED</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mStats</code></td><td><code class="signature"><a href="null-backend-stats.html">null_backend_stats</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `captured` {#function-captured}

<div class="function-declaration" id="function-captured-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> <a href="../../core/types.html#symbol-f32">f32</a>&gt; captured() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The captured frames, oldest first, interleaved stereo.</p>

#### `enumerate_devices` {#function-enumerate-devices}

<div class="function-declaration" id="function-enumerate-devices-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;std::vector&lt;<a href="../audio-device/audio-device-info.html">audio_device_info</a>&gt;&gt; enumerate_devices() <span class="k">const</span> <span class="k">override</span>;</code></pre></div>

<p>One device, &quot;Null Output&quot;, 48000 Hz stereo, default.</p>

#### `fail_next_starts` {#function-fail-next-starts}

<div class="function-declaration" id="function-fail-next-starts-1"><pre><code><span class="k">void</span> fail_next_starts(
    <a href="../../core/types.html#symbol-u32">u32</a> count) <span class="k">noexcept</span>;</code></pre></div>

<p>Tests: the next <code>count</code> <code><a href="#function-start-1">start</a>()</code> calls fail with <code>DEVICE_START_FAILED</code> and leave the backend <code>STOPPED</code>, like a device that cannot be reopened after a loss.</p>

#### `kind` {#function-kind}

<div class="function-declaration" id="function-kind-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../audio-types.html#symbol-audio-backend-kind">audio_backend_kind</a> kind() <span class="k">const</span> <span class="k">noexcept</span> <span class="k">override</span>;</code></pre></div>

#### `output_format` {#function-output-format}

<div class="function-declaration" id="function-output-format-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../audio-types/audio-format.html">audio_format</a> output_format() <span class="k">const</span> <span class="k">noexcept</span> <span class="k">override</span>;</code></pre></div>

#### `render_frames` {#function-render-frames}

<div class="function-declaration" id="function-render-frames-1"><pre><code><span class="k">void</span> render_frames(
    <a href="../../core/types.html#symbol-u32">u32</a> frameCount);</code></pre></div>

<p>Renders exactly <code>frameCount</code> frames, ignoring the clock. Tests use it to step by block.</p>

#### `set_capture_frames` {#function-set-capture-frames}

<div class="function-declaration" id="function-set-capture-frames-1"><pre><code><span class="k">void</span> set_capture_frames(
    <a href="../../core/types.html#symbol-u32">u32</a> frames);</code></pre></div>

<p>Tests: keep the last <code>frames</code> rendered stereo <code><a href="../../core/types.html#symbol-f32">f32</a></code> frames for inspection (0 disables).</p>

#### `simulate_device_loss` {#function-simulate-device-loss}

<div class="function-declaration" id="function-simulate-device-loss-1"><pre><code><span class="k">void</span> simulate_device_loss() <span class="k">noexcept</span>;</code></pre></div>

<p>Tests: publishes <code>device_state::LOST</code>, as the miniaudio notification callback does when the OS stops a device. Nothing renders until the next successful <code><a href="#function-start-1">start</a>()</code>, which is how the engine&#x27;s recovery path is exercised headlessly.</p>

#### `start` {#function-start}

<div class="function-declaration" id="function-start-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; start(
    <span class="k">const</span> <a href="../audio-device/backend-desc.html">backend_desc</a> &amp;desc) <span class="k">override</span>;</code></pre></div>

<p>Resets the virtual clock. Fails with <code>INVALID_SETTINGS</code> only when <code>desc.callback</code> is <code>null</code> or <code>desc.format.sampleRate</code> is outside [<code>MIN_SAMPLE_RATE</code>, <code>MAX_SAMPLE_RATE</code>], and with <code>DEVICE_START_FAILED</code> while a <code><a href="#function-fail-next-starts-1">fail_next_starts</a>()</code> budget remains.</p>

#### `state` {#function-state}

<div class="function-declaration" id="function-state-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../audio-types.html#symbol-device-state">device_state</a> state() <span class="k">const</span> <span class="k">noexcept</span> <span class="k">override</span>;</code></pre></div>

#### `stats` {#function-stats}

<div class="function-declaration" id="function-stats-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="null-backend-stats.html">null_backend_stats</a> &amp;stats() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The snapshot written by the last <code><a href="#function-update-1">update</a>()</code>.</p>

#### `stop` {#function-stop}

<div class="function-declaration" id="function-stop-1"><pre><code><span class="k">void</span> stop() <span class="k">noexcept</span> <span class="k">override</span>;</code></pre></div>

#### `update` {#function-update}

<div class="function-declaration" id="function-update-1"><pre><code><span class="k">void</span> update(
    <a href="../../core/types.html#symbol-f32">f32</a> deltaSeconds) <span class="k">override</span>;</code></pre></div>

<p>Renders <code>round(deltaSeconds * sampleRate)</code> frames (fraction carried to the next call) through the callback in <code>MIX_BLOCK_FRAMES</code> blocks. Nothing happens while stopped or for a non-positive delta.</p>

#### `virtual_time` {#function-virtual-time}

<div class="function-declaration" id="function-virtual-time-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-f64">f64</a> virtual_time() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Seconds of audio rendered since <code><a href="#function-start-1">start</a>()</code>.</p>

### Private

#### `mutable_stats` {#function-mutable-stats}

<div class="function-declaration" id="function-mutable-stats-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="null-backend-stats.html">null_backend_stats</a> &amp;mutable_stats() <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

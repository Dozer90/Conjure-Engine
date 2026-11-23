---
kind: class
---

[Engine](../../../engine.md) → [Audio](../../audio.md) → [audio_backend.h](../audio-backend.md)

# audio_backend
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::audio` |
| Include File Path | `engine/include/conjure/audio/audio_backend.h` |
| Source File Path | `engine/src/audio/audio_backend.cpp` |
| API ↗ | [Clips and decoding](../../../../api/audio/clips-and-decoding.md) |

## Syntax

```cpp
class CONJURE_API audio_backend
```

## Derived Classes


<details><summary>Classes that use this as a base</summary>

<ul>

<li><a href="../null-backend/null-backend.html">null_backend</a></li>

</ul>

</details>

## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">audio_backend();</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">virtual</span> ~audio_backend();</code></td><td></td></tr>
</tbody>
</table>



## Functions

### Public

#### `enumerate_devices` {#function-enumerate-devices}

<div class="function-declaration" id="function-enumerate-devices-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../../core/result/result.html">core::result</a>&lt;std::vector&lt;<a href="../audio-device/audio-device-info.html">audio_device_info</a>&gt;&gt; enumerate_devices() <span class="k">const</span> = <span class="mi">0</span>;</code></pre></div>

<p>Playback devices this backend can open.</p>

#### `kind` {#function-kind}

<div class="function-declaration" id="function-kind-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../audio-types.html#symbol-audio-backend-kind">audio_backend_kind</a> kind() <span class="k">const</span> <span class="k">noexcept</span> = <span class="mi">0</span>;</code></pre></div>

<p>Which implementation this is (never <code>AUTO</code>).</p>

#### `output_format` {#function-output-format}

<div class="function-declaration" id="function-output-format-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../audio-types/audio-format.html">audio_format</a> output_format() <span class="k">const</span> <span class="k">noexcept</span> = <span class="mi">0</span>;</code></pre></div>

<p>The format the callback is driven at after <code><a href="#function-start-1">start</a>()</code> (the requested one; miniaudio converts to the device&#x27;s native format internally).</p>

#### `start` {#function-start}

<div class="function-declaration" id="function-start-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; start(
    <span class="k">const</span> <a href="../audio-device/backend-desc.html">backend_desc</a> &amp;desc) = <span class="mi">0</span>;</code></pre></div>

<p>Opens the device and starts driving <code>desc.callback</code>. Fails with <code>INVALID_SETTINGS</code> for a <code>null</code> callback or an unsupported sample rate, <code>BACKEND_INIT_FAILED</code> when the device cannot be opened and <code>DEVICE_START_FAILED</code> when it cannot be started; an unknown <code>desc.deviceName</code> is not an error (<code>WARN</code>, default device). <code>A</code> running backend is stopped first, and a failed start leaves the backend <code>STOPPED</code>.</p>

#### `state` {#function-state}

<div class="function-declaration" id="function-state-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../audio-types.html#symbol-device-state">device_state</a> state() <span class="k">const</span> <span class="k">noexcept</span> = <span class="mi">0</span>;</code></pre></div>

<p>Atomic device state; readable from any thread.</p>

#### `stop` {#function-stop}

<div class="function-declaration" id="function-stop-1"><pre><code><span class="k">virtual</span> <span class="k">void</span> stop() <span class="k">noexcept</span> = <span class="mi">0</span>;</code></pre></div>

<p>Stops the device; the callback is not running when this returns. Safe to call twice.</p>

#### `update` {#function-update}

<div class="function-declaration" id="function-update-1"><pre><code><span class="k">virtual</span> <span class="k">void</span> update(
    <a href="../../core/types.html#symbol-f32">f32</a> deltaSeconds) = <span class="mi">0</span>;</code></pre></div>

<p>Main thread, once per frame. <code>null</code>: advance the clock and render; miniaudio: poll.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="variables"></a>
<!-- doc-legacy-anchors:end -->

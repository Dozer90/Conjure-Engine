---
kind: struct
---

[Engine](../../../engine.md) → [Audio](../../audio.md) → [audio_engine.h](../audio-engine.md)

# audio_settings
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::audio` |
| Include File Path | `engine/include/conjure/audio/audio_engine.h` |
| Source File Path | `engine/src/audio/audio_engine.cpp` |
| API ↗ | [Clips and decoding](../../../../api/audio/clips-and-decoding.md), [Audio](../../../../api/audio/index.md), [Playing sounds](../../../../api/audio/playing-sounds.md) |

## Syntax

```cpp
struct CONJURE_API audio_settings
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">backend</code></td><td><code class="signature"><a href="../audio-types.html#symbol-audio-backend-kind">audio_backend_kind</a></code></td><td><code class="signature">audio_<wbr>backend_<wbr>kind::<wbr>AUTO</code></td><td>Requested backend. <code>AUTO</code> allows normal backend selection; <code>NULL_BACKEND</code> selects deterministic headless playback.</td></tr>
<tr><td>public</td><td><code class="signature">deviceName</code></td><td><code class="signature">std::string</code></td><td><code class="signature">{}</code></td><td>Requested playback device name. An empty name selects the default device.</td></tr>
<tr><td>public</td><td><code class="signature">doppler<wbr>Factor</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">0.0</span>f</code></td><td>Doppler scale; zero disables the effect. The supported range is zero to ten.</td></tr>
<tr><td>public</td><td><code class="signature">groups</code></td><td><code class="signature">std::vector&lt;<a href="../mix-groups/group-desc.html">group_desc</a>&gt;</code></td><td><code class="signature">{}</code></td><td>Groups created at construction. Their parents are resolved by name.</td></tr>
<tr><td>public</td><td><code class="signature">jobs</code></td><td><code class="signature"><a href="../../core/jobs/job-system/jobs-job-system.html">core::jobs::job_system</a> *</code></td><td><code class="signature">nullptr</code></td><td>Optional job system for streaming decode work. <code>A</code> <code>null</code> pointer runs decoding inline during <code>update()</code>.</td></tr>
<tr><td>public</td><td><code class="signature">masterGain</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">1.0</span>f</code></td><td>Initial gain for the master mix group.</td></tr>
<tr><td>public</td><td><code class="signature">max<wbr>Recovery<wbr>Attempts</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">5</span></code></td><td>Number of failed restart attempts allowed before falling back to the <code>null</code> backend.</td></tr>
<tr><td>public</td><td><code class="signature">periodFrames</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">480</span></code></td><td>Requested number of frames per device callback; it must be non-zero.</td></tr>
<tr><td>public</td><td><code class="signature">recovery<wbr>Backoff<wbr>Seconds</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">1.0</span>f</code></td><td>Frame-time delay between device-recovery attempts.</td></tr>
<tr><td>public</td><td><code class="signature">sampleRate</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">48000</span></code></td><td>Requested mix rate in Hz. <code>validate()</code> checks it against <code>MIN_SAMPLE_RATE</code> and <code>MAX_SAMPLE_RATE</code>.</td></tr>
<tr><td>public</td><td><code class="signature">speedOfSound</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f32">f32</a></code></td><td><code class="signature">DEFAULT_<wbr>SPEED_<wbr>OF_<wbr>SOUND</code></td><td>Speed of sound used by Doppler calculations, in world-distance units per second.</td></tr>
</tbody>
</table>

## Functions

### Public

#### `validate` {#function-validate}

<div class="function-declaration" id="function-validate-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; validate() <span class="k">const</span>;</code></pre></div>

<p><code>INVALID_SETTINGS</code> when the sample rate is outside [<code>MIN_SAMPLE_RATE</code>, <code>MAX_SAMPLE_RATE</code>], the period is 0, a group name is <code>null</code> or duplicated, a parent name is unknown, the group tree has a cycle or exceeds <code>MAX_GROUP_DEPTH</code>, or more than <code>MAX_GROUPS</code> - 1 groups are listed.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->

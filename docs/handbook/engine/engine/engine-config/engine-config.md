---
kind: struct
---

[Engine](../../../engine.md) → [Engine](../../engine.md) → [engine_config.h](../engine-config.md)

# engine_config
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::engine` |
| Include File Path | `engine/include/conjure/engine/engine_config.h` |
| Source File Path | `engine/src/engine/engine_config.cpp` |
| API ↗ | [Engine: the application and the frame](../../../../api/engine/index.md), [Running the engine](../../../../api/engine/running-the-engine.md) |

## Syntax

```cpp
struct CONJURE_API engine_config
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">assets</code></td><td><code class="signature"><a href="assets-settings.html">assets_settings</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">audio</code></td><td><code class="signature"><a href="audio-config.html">audio_config</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">editor</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">headless</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">jobs</code></td><td><code class="signature"><a href="jobs-settings.html">jobs_settings</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">logFile</code></td><td><code class="signature">std::filesystem::path</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">logLevel</code></td><td><code class="signature"><a href="../../core/log.html#symbol-log-level">core::log_level</a></code></td><td><code class="signature">core::<wbr>log_<wbr>level::<wbr>INFO</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">maxFrames</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">memory</code></td><td><code class="signature"><a href="memory-settings.html">memory_settings</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">modules</code></td><td><code class="signature"><a href="modules-settings.html">modules_settings</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">pacing</code></td><td><code class="signature"><a href="pacing-settings.html">pacing_settings</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">physics</code></td><td><code class="signature"><a href="physics-config.html">physics_config</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">projectPath</code></td><td><code class="signature">std::filesystem::path</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">rendering</code></td><td><code class="signature"><a href="rendering-settings.html">rendering_settings</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">scripting</code></td><td><code class="signature"><a href="scripting-settings.html">scripting_settings</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">startup<wbr>Scene<wbr>Override</code></td><td><code class="signature">std::filesystem::path</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">time</code></td><td><code class="signature"><a href="time-settings.html">time_settings</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">window</code></td><td><code class="signature"><a href="window-settings.html">window_settings</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `defaults` {#function-defaults}

<div class="function-declaration" id="function-defaults-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="engine-config.html">engine_config</a> defaults();</code></pre></div>

<p>The desktop defaults of; <code>gpuValidation</code> is forced on in Debug builds.</p>

#### `headless_defaults` {#function-headless-defaults}

<div class="function-declaration" id="function-headless-defaults-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="engine-config.html">engine_config</a> headless_defaults();</code></pre></div>

<p>headless = <code>true</code>, a 1280 x 720 <code>null</code> window, rendering disabled on the <code>null</code> device, the <code>null</code> audio backend, scripting disabled, asset hot reload off (no file watcher stat passes on the job threads), a forced 1/60 frame delta and two job workers. Tests opt individual subsystems back in; on the command line --scripting does that for scripting.</p>

#### `validate` {#function-validate}

<div class="function-declaration" id="function-validate-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; validate() <span class="k">const</span>;</code></pre></div>

<p>Rejects (<code>INVALID_CONFIG</code>, the message names the field): a non-finite or non-positive <code>fixedDeltaSeconds</code>, a non-finite <code>maxFrameDeltaSeconds</code> or one below <code>fixedDeltaSeconds</code>, <code>maxFixedStepsPerFrame</code> == 0, a <code>forcedFrameDeltaSeconds</code> that is negative, non-finite or above <code>maxFrameDeltaSeconds</code>, a negative or non-finite <code>timeScale</code>, a zero window size when not headless, <code>rendering.backend</code> == <code>NULL_DEVICE</code> when not headless, headless with rendering enabled on a real backend, <code>frameThreadBlockBytes</code> &gt; <code>frameArenaBytes</code>, a negative or non-finite <code>audio.masterGain</code>, and <code>jobs.workerCount</code> above <code>core::jobs::MAX_WORKER_COUNT</code> unless it is <code>AUTO_WORKER_COUNT</code>.</p>

## Operator Overloads

### Public

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="engine-config.html">engine_config</a> &amp;) <span class="k">const</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->

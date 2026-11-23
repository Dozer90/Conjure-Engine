---
kind: struct
---

[Engine](../../../engine.md) → [Audio](../../audio.md) → [audio_engine.h](../audio-engine.md)

# audio_engine_stats
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::audio` |
| Include File Path | `engine/include/conjure/audio/audio_engine.h` |
| Source File Path | `engine/src/audio/audio_engine.cpp` |
| API ↗ | [Clips and decoding](../../../../api/audio/clips-and-decoding.md), [Audio](../../../../api/audio/index.md), [Playing sounds](../../../../api/audio/playing-sounds.md) |

## Syntax

```cpp
struct audio_engine_stats
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">activeVoices</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td>Number of currently active voices in the mixer snapshot.</td></tr>
<tr><td>public</td><td><code class="signature">commandsSent</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td>Commands sent to the mixer.</td></tr>
<tr><td>public</td><td><code class="signature">commands<wbr>Spilled</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td>Commands buffered outside the command ring when it was full.</td></tr>
<tr><td>public</td><td><code class="signature">device<wbr>Restarts</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td>Successful device restarts during recovery.</td></tr>
<tr><td>public</td><td><code class="signature">deviceState</code></td><td><code class="signature"><a href="../audio-types.html#symbol-device-state">device_state</a></code></td><td><code class="signature">device_<wbr>state::<wbr>STOPPED</code></td><td>Mirrored device state used for diagnostics, including <code>LOST</code> during recovery.</td></tr>
<tr><td>public</td><td><code class="signature">events<wbr>Dropped</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td>Mixer events dropped when the event ring overflowed.</td></tr>
<tr><td>public</td><td><code class="signature">events<wbr>Received</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td>Mixer events received on the main thread.</td></tr>
<tr><td>public</td><td><code class="signature">fell<wbr>Back<wbr>To<wbr>Null</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td>Whether backend recovery switched playback to the <code>null</code> backend.</td></tr>
<tr><td>public</td><td><code class="signature">frames<wbr>Rendered</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td>Total sample frames rendered by the mixer.</td></tr>
<tr><td>public</td><td><code class="signature">loops</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td>Loop completions recorded by the mixer.</td></tr>
<tr><td>public</td><td><code class="signature">outputFormat</code></td><td><code class="signature"><a href="../audio-types/audio-format.html">audio_format</a></code></td><td><code class="signature">{}</code></td><td>The mix output format negotiated at construction.</td></tr>
<tr><td>public</td><td><code class="signature">peakVoices</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td>Highest number of active voices observed.</td></tr>
<tr><td>public</td><td><code class="signature">renderCalls</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td>Number of mixer render calls.</td></tr>
<tr><td>public</td><td><code class="signature">streaming<wbr>Voices</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td>Streaming slots currently tracked by the engine.</td></tr>
<tr><td>public</td><td><code class="signature">underruns</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td>Streaming underruns observed during rendering.</td></tr>
<tr><td>public</td><td><code class="signature">voices<wbr>Dropped</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td>Playback requests dropped when a voice could not be admitted.</td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->

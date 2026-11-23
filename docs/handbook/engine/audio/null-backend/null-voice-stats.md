---
kind: struct
---

[Engine](../../../engine.md) → [Audio](../../audio.md) → [null_backend.h](../null-backend.md)

# null_voice_stats
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::audio` |
| Include File Path | `engine/include/conjure/audio/null_backend.h` |
| Source File Path | `engine/src/audio/null_backend.cpp` |
| API ↗ | [Clips and decoding](../../../../api/audio/clips-and-decoding.md) |

## Syntax

```cpp
struct null_voice_stats
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">cursorFrames</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">gain</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">0.0</span>f</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">groupIndex</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u8">u8</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">loop</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">pan</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">0.0</span>f</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">state</code></td><td><code class="signature"><a href="../audio-types.html#symbol-voice-state">voice_state</a></code></td><td><code class="signature">voice_<wbr>state::<wbr>FREE</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">step</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f64">f64</a></code></td><td><code class="signature"><span class="mi">0.0</span></code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->

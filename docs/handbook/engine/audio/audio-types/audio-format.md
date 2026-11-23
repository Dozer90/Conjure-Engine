---
kind: struct
---

[Engine](../../../engine.md) → [Audio](../../audio.md) → [audio_types.h](../audio-types.md)

# audio_format
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::audio` |
| Include File Path | `engine/include/conjure/audio/audio_types.h` |
| Source File Path | `engine/src/audio/audio_types.cpp` |
| API ↗ | [Playing sounds](../../../../api/audio/playing-sounds.md) |

## Syntax

```cpp
struct audio_format
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">channels</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u16">u16</a></code></td><td><code class="signature">static_<wbr>cast&lt;<wbr><a href="../../core/types.html#symbol-u16">u16</a>&gt;(MIX_<wbr>CHANNELS)</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">format</code></td><td><code class="signature"><a href="../audio-types.html#symbol-sample-format">sample_format</a></code></td><td><code class="signature">sample_<wbr>format::<wbr>F32</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">sampleRate</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">48000</span></code></td><td></td></tr>
</tbody>
</table>



## Operator Overloads

### Public

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="audio-format.html">audio_format</a> &amp;other) <span class="k">const</span> <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->

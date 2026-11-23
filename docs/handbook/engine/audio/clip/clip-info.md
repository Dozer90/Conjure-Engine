---
kind: struct
---

[Engine](../../../engine.md) → [Audio](../../audio.md) → [clip.h](../clip.md)

# clip_info
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::audio` |
| Include File Path | `engine/include/conjure/audio/clip.h` |
| Source File Path | `engine/src/audio/clip.cpp` |
| API ↗ | [Clips and decoding](../../../../api/audio/clips-and-decoding.md) |

## Syntax

```cpp
struct CONJURE_API clip_info
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">encoding</code></td><td><code class="signature"><a href="../audio-types.html#symbol-clip-encoding">clip_encoding</a></code></td><td><code class="signature">clip_<wbr>encoding::<wbr>UNKNOWN</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">format</code></td><td><code class="signature"><a href="../audio-types/audio-format.html">audio_format</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">frameCount</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">storage</code></td><td><code class="signature"><a href="../audio-types.html#symbol-clip-storage">clip_storage</a></code></td><td><code class="signature">clip_<wbr>storage::<wbr>PCM</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `duration_seconds` {#function-duration-seconds}

<div class="function-declaration" id="function-duration-seconds-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-f64">f64</a> duration_seconds() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p><code>frameCount</code> / sampleRate; 0 for a zero rate.</p>

## Operator Overloads

### Public

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="clip-info.html">clip_info</a> &amp;other) <span class="k">const</span> <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->

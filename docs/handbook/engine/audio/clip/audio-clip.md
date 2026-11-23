---
kind: struct
---

[Engine](../../../engine.md) → [Audio](../../audio.md) → [clip.h](../clip.md)

# audio_clip
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::audio` |
| Include File Path | `engine/include/conjure/audio/clip.h` |
| Source File Path | `engine/src/audio/clip.cpp` |
| API ↗ | [Clips and decoding](../../../../api/audio/clips-and-decoding.md) |

## Syntax

```cpp
struct CONJURE_API audio_clip
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">encoded</code></td><td><code class="signature">std::<wbr>shared_<wbr>ptr&lt;<wbr><span class="k">const</span> std::<wbr>vector&lt;<wbr><a href="../../core/types.html#symbol-byte">byte</a>&gt;&gt;</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">info</code></td><td><code class="signature"><a href="clip-info.html">clip_info</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">pcm</code></td><td><code class="signature">std::<wbr>shared_<wbr>ptr&lt;<wbr><span class="k">const</span> <a href="pcm-buffer.html">pcm_<wbr>buffer</a>&gt;</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `duration_seconds` {#function-duration-seconds}

<div class="function-declaration" id="function-duration-seconds-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-f64">f64</a> duration_seconds() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Duration in seconds from info.</p>

#### `is_streamed` {#function-is-streamed}

<div class="function-declaration" id="function-is-streamed-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_streamed() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when the clip decodes while playing instead of holding <code>PCM</code>.</p>

#### `is_valid` {#function-is-valid}

<div class="function-declaration" id="function-is-valid-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_valid() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when the clip carries the buffer its storage kind needs and the info agrees with it.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->

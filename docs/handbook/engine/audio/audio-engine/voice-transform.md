---
kind: struct
---

[Engine](../../../engine.md) → [Audio](../../audio.md) → [audio_engine.h](../audio-engine.md)

# voice_transform
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::audio` |
| Include File Path | `engine/include/conjure/audio/audio_engine.h` |
| Source File Path | `engine/src/audio/audio_engine.cpp` |
| API ↗ | [Clips and decoding](../../../../api/audio/clips-and-decoding.md), [Audio](../../../../api/audio/index.md), [Playing sounds](../../../../api/audio/playing-sounds.md) |

## Syntax

```cpp
struct voice_transform
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">position</code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a></code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a>(<span class="mi">0.0</span>f)</code></td><td>World-space position of the voice.</td></tr>
<tr><td>public</td><td><code class="signature">velocity</code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a></code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a>(<span class="mi">0.0</span>f)</code></td><td>World-space velocity used by spatial and Doppler calculations.</td></tr>
<tr><td>public</td><td><code class="signature">voice</code></td><td><code class="signature"><a href="../voice/voice-id.html">voice_id</a></code></td><td><code class="signature">{}</code></td><td>Voice receiving the transform.</td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->

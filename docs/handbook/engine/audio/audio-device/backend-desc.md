---
kind: struct
---

[Engine](../../../engine.md) → [Audio](../../audio.md) → [audio_device.h](../audio-device.md)

# backend_desc
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::audio` |
| Include File Path | `engine/include/conjure/audio/audio_device.h` |
| Source File Path | `engine/src/audio/audio_device.cpp` |
| API ↗ | [Playing sounds](../../../../api/audio/playing-sounds.md) |

## Syntax

```cpp
struct backend_desc
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">callback</code></td><td><code class="signature"><a href="../audio-device.html#symbol-mix-callback">mix_callback</a></code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">deviceName</code></td><td><code class="signature">std::string</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">format</code></td><td><code class="signature"><a href="../audio-types/audio-format.html">audio_format</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">periodFrames</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">480</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">user</code></td><td><code class="signature"><span class="k">void</span> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->

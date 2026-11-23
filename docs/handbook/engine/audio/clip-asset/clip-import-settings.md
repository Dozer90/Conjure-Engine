---
kind: struct
---

[Engine](../../../engine.md) → [Audio](../../audio.md) → [clip_asset.h](../clip-asset.md)

# clip_import_settings
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::audio` |
| Include File Path | `engine/include/conjure/audio/clip_asset.h` |
| Source File Path | `engine/src/audio/clip_asset.cpp` |
| API ↗ | [Clips and decoding](../../../../api/audio/clips-and-decoding.md) |

## Syntax

```cpp
struct CONJURE_API clip_import_settings
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">forceMono</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">normalize</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">sampleRate</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">storage</code></td><td><code class="signature"><a href="../clip-asset.html#symbol-clip-storage-setting">clip_storage_setting</a></code></td><td><code class="signature">clip_<wbr>storage_<wbr>setting::<wbr>AUTO</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">stream<wbr>Threshold<wbr>Seconds</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f64">f64</a></code></td><td><code class="signature"><span class="mi">10.0</span></code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `from_json` {#function-from-json}

<div class="function-declaration" id="function-from-json-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../core/result/result.html">core::result</a>&lt;<a href="clip-import-settings.html">clip_import_settings</a>&gt; from_json(
    <span class="k">const</span> <a href="../../core/json/value/json-value.html">core::json::value</a> &amp;source);</code></pre></div>

<p>Reads the settings object; missing keys keep their defaults. Fails with <code>INVALID_SETTINGS</code> for an unknown storage name, a negative threshold or a sample rate outside 0 or [<code>MIN_SAMPLE_RATE</code>, <code>MAX_SAMPLE_RATE</code>]. An invalid (missing) value yields the defaults.</p>

#### `to_document` {#function-to-document}

<div class="function-declaration" id="function-to-document-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/json/document/json-document.html">core::json::document</a> to_document() <span class="k">const</span>;</code></pre></div>

<p>The settings as an owning document.</p>

#### `to_json` {#function-to-json}

<div class="function-declaration" id="function-to-json-1"><pre><code><span class="k">void</span> to_json(
    <a href="../../core/json/writer/json-writer.html">core::json::writer</a> &amp;out) <span class="k">const</span>;</code></pre></div>

<p>Writes the settings object with keys in sorted order (canonical form).</p>

## Operator Overloads

### Public

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="clip-import-settings.html">clip_import_settings</a> &amp;other) <span class="k">const</span> <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->

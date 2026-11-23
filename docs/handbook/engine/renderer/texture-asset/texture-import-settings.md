---
kind: struct
---

[Engine](../../../engine.md) → [Renderer](../../renderer.md) → [texture_asset.h](../texture-asset.md)

# texture_import_settings
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::renderer` |
| Include File Path | `engine/include/conjure/renderer/texture_asset.h` |
| Source File Path | `engine/src/renderer/assets/texture_asset.cpp` |
| API ↗ | [Rendering](../../../../api/renderer/index.md), [JPEG XL images](../../../../api/renderer/jpeg-xl.md), [Renderer assets](../../../../api/renderer/renderer-assets.md) |

## Syntax

```cpp
struct CONJURE_API texture_import_settings
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">colourSpace</code></td><td><code class="signature"><a href="../texture-asset.html#symbol-texture-colour-space">texture_colour_space</a></code></td><td><code class="signature">texture_<wbr>colour_<wbr>space::<wbr>SRGB</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">filter</code></td><td><code class="signature"><a href="../texture-asset.html#symbol-sampler-filter">sampler_filter</a></code></td><td><code class="signature">sampler_<wbr>filter::<wbr>LINEAR</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">generateMips</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">true</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">wrap</code></td><td><code class="signature"><a href="../texture-asset.html#symbol-texture-wrap">texture_wrap</a></code></td><td><code class="signature">texture_<wbr>wrap::<wbr>REPEAT</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `from_json` {#function-from-json}

<div class="function-declaration" id="function-from-json-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../core/result/result.html">core::result</a>&lt;<a href="texture-import-settings.html">texture_import_settings</a>&gt; from_json(
    <span class="k">const</span> <a href="../../core/json/value/json-value.html">core::json::value</a> &amp;source);</code></pre></div>

<p>Reads the settings object; missing keys keep their defaults, an invalid value yields the defaults. Fails with <code>INVALID_ARGUMENT</code> for an unknown enumerator name or a non-object.</p>

#### `to_document` {#function-to-document}

<div class="function-declaration" id="function-to-document-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/json/document/json-document.html">core::json::document</a> to_document() <span class="k">const</span>;</code></pre></div>

#### `to_json` {#function-to-json}

<div class="function-declaration" id="function-to-json-1"><pre><code><span class="k">void</span> to_json(
    <a href="../../core/json/writer/json-writer.html">core::json::writer</a> &amp;out) <span class="k">const</span>;</code></pre></div>

## Operator Overloads

### Public

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="texture-import-settings.html">texture_import_settings</a> &amp;other) <span class="k">const</span> <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->

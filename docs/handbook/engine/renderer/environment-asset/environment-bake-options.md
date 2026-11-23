---
kind: struct
---

[Engine](../../../engine.md) → [Renderer](../../renderer.md) → [environment_asset.h](../environment-asset.md)

# environment_bake_options
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::renderer` |
| Include File Path | `engine/include/conjure/renderer/environment_asset.h` |
| Source File Path | `engine/src/renderer/assets/environment_asset.cpp` |
| API ↗ | [Rendering](../../../../api/renderer/index.md), [Renderer assets](../../../../api/renderer/renderer-assets.md) |

## Syntax

```cpp
struct CONJURE_API environment_bake_options
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">prefiltered<wbr>Size</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">128</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">sampleCount</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">128</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">skySize</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">512</span></code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `from_json` {#function-from-json}

<div class="function-declaration" id="function-from-json-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../core/result/result.html">core::result</a>&lt;<a href="environment-bake-options.html">environment_bake_options</a>&gt; from_json(
    <span class="k">const</span> <a href="../../core/json/value/json-value.html">core::json::value</a> &amp;source);</code></pre></div>

#### `to_document` {#function-to-document}

<div class="function-declaration" id="function-to-document-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/json/document/json-document.html">core::json::document</a> to_document() <span class="k">const</span>;</code></pre></div>

#### `to_json` {#function-to-json}

<div class="function-declaration" id="function-to-json-1"><pre><code><span class="k">void</span> to_json(
    <a href="../../core/json/writer/json-writer.html">core::json::writer</a> &amp;out) <span class="k">const</span>;</code></pre></div>

## Operator Overloads

### Public

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="environment-bake-options.html">environment_bake_options</a> &amp;other) <span class="k">const</span> <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->

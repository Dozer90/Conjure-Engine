---
kind: struct
---

[Engine](../../../engine.md) → [Renderer](../../renderer.md) → [model_asset.h](../model-asset.md)

# model_node
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::renderer` |
| Include File Path | `engine/include/conjure/renderer/model_asset.h` |
| Source File Path | `engine/src/renderer/assets/model_asset.cpp` |
| API ↗ | [Rendering](../../../../api/renderer/index.md), [Renderer assets](../../../../api/renderer/renderer-assets.md) |

## Syntax

```cpp
struct CONJURE_API model_node
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">model_node();</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">model_node(
    <span class="k">const</span> <a href="model-node.html">model_node</a> &amp;) = <span class="k">default</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">model_node(
    <a href="model-node.html">model_node</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~model_node();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">cameraData</code></td><td><code class="signature"><a href="../camera/camera.html">camera</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">hasCamera</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">hasLight</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">lightData</code></td><td><code class="signature"><a href="../light/light.html">light</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">local<wbr>Transform</code></td><td><code class="signature"><a href="../../core/math/transform/transform.html">math::transform</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">materials</code></td><td><code class="signature">std::<wbr>array&lt;<wbr>u32,<wbr> MAX_<wbr>MATERIAL_<wbr>SLOTS&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">mesh</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature">INVALID_INDEX</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">name</code></td><td><code class="signature">std::string</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">parent</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature">INVALID_INDEX</code></td><td></td></tr>
</tbody>
</table>



## Operator Overloads

### Public

#### `operator=` {#operator-3d}

<div class="function-declaration" id="operator-3d-1"><pre><code><a href="model-node.html">model_node</a> &amp;<span class="k">operator</span>=(
    <span class="k">const</span> <a href="model-node.html">model_node</a> &amp;) = <span class="k">default</span>;</code></pre></div>

<div class="function-declaration" id="operator-3d-2"><pre><code><a href="model-node.html">model_node</a> &amp;<span class="k">operator</span>=(
    <a href="model-node.html">model_node</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="model-node.html">model_node</a> &amp;other) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->

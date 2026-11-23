---
kind: class
---

[Engine](../../../engine.md) → [Renderer](../../renderer.md) → [render_scene.h](../render-scene.md)

# render_scene_pair
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::renderer` |
| Include File Path | `engine/include/conjure/renderer/render_scene.h` |
| Source File Path | `engine/src/renderer/render_scene.cpp` |
| API ↗ | [Rendering](../../../../api/renderer/index.md), [Rendering a frame](../../../../api/renderer/rendering-a-frame.md) |

## Syntax

```cpp
class CONJURE_API render_scene_pair
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">render_scene_pair();</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~render_scene_pair();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mExtracting</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mOwner</code></td><td><code class="signature">std::thread::id</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mScenes</code></td><td><code class="signature">std::<wbr>array&lt;<wbr>render_<wbr>scene,<wbr> <span class="mi">2</span>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mWriteIndex</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `begin_extract` {#function-begin-extract}

<div class="function-declaration" id="function-begin-extract-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="render-scene.html">render_scene</a> &amp;begin_extract();</code></pre></div>

<p>Clears the write slot and returns it. Asserts when extraction is already open.</p>

#### `end_extract` {#function-end-extract}

<div class="function-declaration" id="function-end-extract-1"><pre><code><span class="k">void</span> end_extract();</code></pre></div>

<p>Swaps the slots; what was written becomes <code><a href="#function-read-1">read</a>()</code>. Asserts without a matching <code>begin_extract</code>.</p>

#### `is_extracting` {#function-is-extracting}

<div class="function-declaration" id="function-is-extracting-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_extracting() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `read` {#function-read}

<div class="function-declaration" id="function-read-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="render-scene.html">render_scene</a> &amp;read() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The slot the renderer reads. Stable between <code>end_extract</code> calls.</p>

#### `write` {#function-write}

<div class="function-declaration" id="function-write-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="render-scene.html">render_scene</a> &amp;write() <span class="k">noexcept</span>;</code></pre></div>

<p>The slot extraction writes (valid between <code>begin_extract</code> and <code>end_extract</code>).</p>

#### `write_index` {#function-write-index}

<div class="function-declaration" id="function-write-index-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> write_index() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Index of the write slot (0 or 1); flips on every <code>end_extract</code>.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

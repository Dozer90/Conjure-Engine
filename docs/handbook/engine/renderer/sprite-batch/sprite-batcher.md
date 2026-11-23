---
kind: class
---

[Engine](../../../engine.md) → [Renderer](../../renderer.md) → [sprite_batch.h](../sprite-batch.md)

# sprite_batcher
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::renderer` |
| Include File Path | `engine/include/conjure/renderer/sprite_batch.h` |
| Source File Path | N/A |
| API ↗ | [Rendering](../../../../api/renderer/index.md), [Rendering a frame](../../../../api/renderer/rendering-a-frame.md) |

## Syntax

```cpp
class CONJURE_API sprite_batcher
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">sprite_batcher();</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~sprite_batcher();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mDropped</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mIndices</code></td><td><code class="signature">std::vector&lt;<a href="../../core/types.html#symbol-u32">u32</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mKeys</code></td><td><code class="signature">std::vector&lt;<a href="../../core/types.html#symbol-u64">u64</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mOrder</code></td><td><code class="signature">std::vector&lt;<a href="../../core/types.html#symbol-u32">u32</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Overflow<wbr>Warned</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mRuns</code></td><td><code class="signature">std::vector&lt;<a href="sprite-run.html">sprite_run</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mScratchKeys</code></td><td><code class="signature">std::vector&lt;<a href="../../core/types.html#symbol-u64">u64</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Scratch<wbr>Values</code></td><td><code class="signature">std::vector&lt;<a href="../../core/types.html#symbol-u32">u32</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mVertices</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="sprite-vertex.html">sprite_<wbr>vertex</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `build` {#function-build}

<div class="function-declaration" id="function-build-1"><pre><code><span class="k">void</span> build(
    std::span&lt;<span class="k">const</span> <a href="sprite.html">sprite</a>&gt; sprites);</code></pre></div>

<p>Rebuilds vertices, indices and runs from the sprites. Keeps capacity between calls. Logs the overflow <code>WARN</code> when this build drops sprites and the one before it dropped none.</p>

#### `clear` {#function-clear}

<div class="function-declaration" id="function-clear-1"><pre><code><span class="k">void</span> clear() <span class="k">noexcept</span>;</code></pre></div>

<p>Forgets the output and the dropped count; capacity is kept, and so is whether the overflow <code>WARN</code> is armed (only a build decides that).</p>

#### `dropped_count` {#function-dropped-count}

<div class="function-declaration" id="function-dropped-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> dropped_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Sprites dropped by the last build because the cap was reached (both spaces); 0 after <code><a href="#function-clear-1">clear</a>()</code>. The renderer reports its batcher&#x27;s count as <code>render_stats::spritesDropped</code>.</p>

#### `indices` {#function-indices}

<div class="function-declaration" id="function-indices-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> <a href="../../core/types.html#symbol-u32">u32</a>&gt; indices() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `runs` {#function-runs}

<div class="function-declaration" id="function-runs-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> <a href="sprite-run.html">sprite_run</a>&gt; runs() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `sort_key` {#function-sort-key}

<div class="function-declaration" id="function-sort-key-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../core/types.html#symbol-u64">u64</a> sort_key(
    <span class="k">const</span> <a href="sprite.html">sprite</a> &amp;value) <span class="k">noexcept</span>;</code></pre></div>

<p>The sort key of a <code><a href="sprite.html">sprite</a></code>: space, then layer, order and texture, all ascending.</p>

#### `sprite_count` {#function-sprite-count}

<div class="function-declaration" id="function-sprite-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> sprite_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Number of sprites the last build emitted.</p>

#### `vertices` {#function-vertices}

<div class="function-declaration" id="function-vertices-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> <a href="sprite-vertex.html">sprite_vertex</a>&gt; vertices() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

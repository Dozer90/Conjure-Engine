---
kind: class
---

[Engine](../../../engine.md) → [Renderer](../../renderer.md) → [render_settings.h](../render-settings.md)

# render_defaults
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::renderer` |
| Include File Path | `engine/include/conjure/renderer/render_settings.h` |
| Source File Path | `engine/src/renderer/render_settings.cpp` |
| API ↗ | [Rendering](../../../../api/renderer/index.md), [Rendering a frame](../../../../api/renderer/rendering-a-frame.md) |

## Syntax

```cpp
class CONJURE_API render_defaults
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mRevision</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mValue</code></td><td><code class="signature"><a href="render-settings.html">render_settings</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `get` {#function-get}

<div class="function-declaration" id="function-get-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="render-settings.html">render_settings</a> &amp;get() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The current defaults; engine defaults until the first <code><a href="#function-set-1">set</a>()</code>.</p>

#### `revision` {#function-revision}

<div class="function-declaration" id="function-revision-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u64">u64</a> revision() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Bumps on every <code><a href="#function-set-1">set</a>()</code>, also when the value does not change; 0 before the first.</p>

#### `set` {#function-set}

<div class="function-declaration" id="function-set-1"><pre><code><span class="k">void</span> set(
    <span class="k">const</span> <a href="render-settings.html">render_settings</a> &amp;value) <span class="k">noexcept</span>;</code></pre></div>

<p>Sanitises a copy of the value (callers pass validated settings; this is the backstop) and stores it. Main thread only.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

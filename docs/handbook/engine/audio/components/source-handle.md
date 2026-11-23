---
kind: struct
---

[Engine](../../../engine.md) → [Audio](../../audio.md) → [components.h](../components.md)

# source_handle
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::audio` |
| Include File Path | `engine/include/conjure/audio/components.h` |
| Source File Path | `engine/src/audio/components.cpp` |
| API ↗ | [Audio](../../../../api/audio/index.md) |

## Syntax

```cpp
struct source_handle
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">clipVersion</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">FLAG_<wbr>STARTED_<wbr>ONCE</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../core/types.html#symbol-u8">u8</a></code></td><td><code class="signature"><span class="mi">1</span>u &lt;&lt; <span class="mi">0</span></code></td><td>Bit of flags recording that the source was started at least once (<code>playOnStart</code> consumed).</td></tr>
<tr><td>public</td><td><code class="signature">flags</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u8">u8</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">voice</code></td><td><code class="signature"><a href="../voice/voice-id.html">voice_id</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `started_once` {#function-started-once}

<div class="function-declaration" id="function-started-once-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> started_once() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True once <code>FLAG_STARTED_ONCE</code> is set.</p>

## Operator Overloads

### Public

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="source-handle.html">source_handle</a> &amp;other) <span class="k">const</span> <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->

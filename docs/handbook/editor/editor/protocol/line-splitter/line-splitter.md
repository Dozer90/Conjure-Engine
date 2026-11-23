---
kind: class
---

[Editor](../../../../editor.md) → [Protocol](../../../protocol.md) → [line_splitter.h](../line-splitter.md)

# line_splitter
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::editor` |
| Include File Path | `engine/include/conjure/editor/protocol/line_splitter.h` |
| Source File Path | `engine/src/editor/core/protocol/line_splitter.cpp` |
| API ↗ | [The AI control protocol](../../../../../api/editor/ai-protocol.md) |

## Syntax

```cpp
class CONJURE_API line_splitter
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">explicit</span> line_splitter(
    <a href="../../../../engine/core/types.html#symbol-usize">usize</a> limit = MAX_MESSAGE_BYTES);</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">line_splitter(
    <span class="k">const</span> <a href="line-splitter.html">line_splitter</a> &amp;) = <span class="k">default</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">line_splitter(
    <a href="line-splitter.html">line_splitter</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~line_splitter();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mCurrent</code></td><td><code class="signature">std::string</code></td><td><code class="signature"><span class="s">""</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mDiscarding</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mFrames</code></td><td><code class="signature">std::deque&lt;<a href="line-splitter-framed-line.html">framed_line</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mLimit</code></td><td><code class="signature"><a href="../../../../engine/core/types.html#symbol-usize">usize</a></code></td><td><code class="signature">MAX_<wbr>MESSAGE_<wbr>BYTES</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mOverflowed</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `feed` {#function-feed}

<div class="function-declaration" id="function-feed-1"><pre><code><span class="k">void</span> feed(
    std::span&lt;<span class="k">const</span> std::byte&gt; bytes);</code></pre></div>

<p>Appends received bytes.</p>

<div class="function-declaration" id="function-feed-2"><pre><code><span class="k">void</span> feed(
    std::string_view text);</code></pre></div>

#### `frame_count` {#function-frame-count}

<div class="function-declaration" id="function-frame-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../../engine/core/types.html#symbol-usize">usize</a> frame_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Complete lines and markers waiting to be taken.</p>

#### `limit` {#function-limit}

<div class="function-declaration" id="function-limit-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../../engine/core/types.html#symbol-usize">usize</a> limit() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `next_frame` {#function-next-frame}

<div class="function-declaration" id="function-next-frame-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../line-splitter.html#symbol-line-splitter-frame">frame</a> next_frame(
    std::string &amp;out);</code></pre></div>

<p>Takes the next frame in arrival order: <code>LINE</code> fills <code>out</code> with the text (terminator removed), <code>OVERSIZED</code> marks where a line exceeded the limit (<code>out</code> is cleared), <code>NONE</code> when nothing is complete yet.</p>

#### `next_line` {#function-next-line}

<div class="function-declaration" id="function-next-line-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> next_line(
    std::string &amp;out);</code></pre></div>

<p>Takes the next complete line (without its terminator), skipping <code>OVERSIZED</code> markers. False when none is complete.</p>

#### `overflowed` {#function-overflowed}

<div class="function-declaration" id="function-overflowed-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> overflowed() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True once a line exceeded the limit. Stays set until <code><a href="#function-reset-1">reset</a>()</code>.</p>

#### `pending_bytes` {#function-pending-bytes}

<div class="function-declaration" id="function-pending-bytes-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../../engine/core/types.html#symbol-usize">usize</a> pending_bytes() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Bytes of the incomplete line held back.</p>

#### `reset` {#function-reset}

<div class="function-declaration" id="function-reset-1"><pre><code><span class="k">void</span> reset();</code></pre></div>

<p>Forgets the buffered bytes, the waiting frames and the overflow flag.</p>

## Operator Overloads

### Public

#### `operator=` {#operator-3d}

<div class="function-declaration" id="operator-3d-1"><pre><code><a href="line-splitter.html">line_splitter</a> &amp;<span class="k">operator</span>=(
    <span class="k">const</span> <a href="line-splitter.html">line_splitter</a> &amp;) = <span class="k">default</span>;</code></pre></div>

<div class="function-declaration" id="operator-3d-2"><pre><code><a href="line-splitter.html">line_splitter</a> &amp;<span class="k">operator</span>=(
    <a href="line-splitter.html">line_splitter</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

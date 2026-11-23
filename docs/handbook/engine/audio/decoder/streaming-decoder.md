---
kind: class
---

[Engine](../../../engine.md) → [Audio](../../audio.md) → [decoder.h](../decoder.md)

# streaming_decoder
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::audio` |
| Include File Path | `engine/include/conjure/audio/decoder.h` |
| Source File Path | N/A |
| API ↗ | [Clips and decoding](../../../../api/audio/clips-and-decoding.md) |

## Syntax

```cpp
class CONJURE_API streaming_decoder
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">streaming_decoder();</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">streaming_decoder(
    <a href="streaming-decoder.html">streaming_decoder</a> &amp;&amp;) <span class="k">noexcept</span>;</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~streaming_decoder();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mInfo</code></td><td><code class="signature"><a href="../clip/clip-info.html">clip_info</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mPosition</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mState</code></td><td><code class="signature">std::unique_ptr&lt;<a href="../../core/event/event-state.html">state</a>&gt;</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `close` {#function-close}

<div class="function-declaration" id="function-close-1"><pre><code><span class="k">void</span> close() <span class="k">noexcept</span>;</code></pre></div>

<p>Releases the decoder. Safe to call twice.</p>

#### `info` {#function-info}

<div class="function-declaration" id="function-info-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../clip/clip-info.html">clip_info</a> &amp;info() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Format, length and encoding of the open stream (storage <code>STREAMED</code>).</p>

#### `is_open` {#function-is-open}

<div class="function-declaration" id="function-is-open-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_open() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True between a successful <code><a href="#function-open-1">open</a>()</code> and <code><a href="#function-close-1">close</a>()</code>.</p>

#### `open` {#function-open}

<div class="function-declaration" id="function-open-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; open(
    std::span&lt;<span class="k">const</span> <a href="../../core/types.html#symbol-byte">byte</a>&gt; encoded);</code></pre></div>

<p>Opens a decoder over caller-owned bytes, which must outlive the decoder (or <code><a href="#function-close-1">close</a>()</code>). An open decoder is closed first. <code>A</code> container that does not report its length (Ogg Vorbis) is decoded once to count it, which costs a full decode on the calling thread; use the overload below when the length is already known. Same errors as <code>probe()</code>.</p>

<div class="function-declaration" id="function-open-2"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; open(
    std::span&lt;<span class="k">const</span> <a href="../../core/types.html#symbol-byte">byte</a>&gt; encoded,
    <a href="../../core/types.html#symbol-u64">u64</a> knownFrameCount);</code></pre></div>

<p><code><a href="#function-open-1">open</a>()</code> with a length the caller already knows (a probed clip&#x27;s <code>info.frameCount</code>): the container&#x27;s own length wins when it reports one, otherwise <code>knownFrameCount</code> is used and nothing is decoded before the first <code><a href="#function-read-1">read</a>()</code>. 0 counts as unknown.</p>

#### `position` {#function-position}

<div class="function-declaration" id="function-position-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u64">u64</a> position() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Frame index the next <code><a href="#function-read-1">read</a>()</code> starts at.</p>

#### `read` {#function-read}

<div class="function-declaration" id="function-read-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-usize">usize</a> read(
    std::span&lt;<a href="../../core/types.html#symbol-f32">f32</a>&gt; out) <span class="k">noexcept</span>;</code></pre></div>

<p>Decodes up to <code>out.size()</code> / channels frames into interleaved <code><a href="../../core/types.html#symbol-f32">f32</a></code>; returns the number of frames written (0 at the end of the stream or when closed).</p>

#### `seek` {#function-seek}

<div class="function-declaration" id="function-seek-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; seek(
    <a href="../../core/types.html#symbol-u64">u64</a> frame) <span class="k">noexcept</span>;</code></pre></div>

<p>Positions the next read at <code>frame</code>. <code>INVALID_STATE</code> when closed, <code>DECODE_FAILED</code> when the decoder refuses the position.</p>

## Operator Overloads

### Public

#### `operator=` {#operator-3d}

<div class="function-declaration" id="operator-3d-1"><pre><code><a href="streaming-decoder.html">streaming_decoder</a> &amp;<span class="k">operator</span>=(
    <a href="streaming-decoder.html">streaming_decoder</a> &amp;&amp;) <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

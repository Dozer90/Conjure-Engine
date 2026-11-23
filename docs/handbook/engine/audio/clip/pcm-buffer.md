---
kind: class
---

[Engine](../../../engine.md) → [Audio](../../audio.md) → [clip.h](../clip.md)

# pcm_buffer
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::audio` |
| Include File Path | `engine/include/conjure/audio/clip.h` |
| Source File Path | `engine/src/audio/clip.cpp` |
| API ↗ | [Clips and decoding](../../../../api/audio/clips-and-decoding.md) |

## Syntax

```cpp
class CONJURE_API pcm_buffer
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">pcm_buffer();</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">pcm_buffer(
    <a href="../audio-types/audio-format.html">audio_format</a> format,
    std::vector&lt;<a href="../../core/types.html#symbol-f32">f32</a>&gt; samples);</code></td><td>Takes ownership of interleaved samples. The frame count is <code>samples.size()</code> / channels; a sample count that is not a multiple of the channel count asserts and drops the trailing partial frame. <code>A</code> channel count of 0 asserts and is treated as 1.</td></tr>
<tr><td>public</td><td><code class="signature">pcm_buffer(
    <span class="k">const</span> <a href="pcm-buffer.html">pcm_buffer</a> &amp;) = <span class="k">default</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">pcm_buffer(
    <a href="pcm-buffer.html">pcm_buffer</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~pcm_buffer();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mFormat</code></td><td><code class="signature"><a href="../audio-types/audio-format.html">audio_format</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mFrameCount</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mSamples</code></td><td><code class="signature">std::vector&lt;<a href="../../core/types.html#symbol-f32">f32</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `duration_seconds` {#function-duration-seconds}

<div class="function-declaration" id="function-duration-seconds-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-f64">f64</a> duration_seconds() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p><code><a href="#function-frame-count-1">frame_count</a>()</code> / sampleRate; 0 for an empty buffer or a zero rate.</p>

#### `format` {#function-format}

<div class="function-declaration" id="function-format-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../audio-types/audio-format.html">audio_format</a> &amp;format() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Sample rate, channel count and sample format of the samples.</p>

#### `frame_count` {#function-frame-count}

<div class="function-declaration" id="function-frame-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u64">u64</a> frame_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Number of frames (one sample per channel each).</p>

#### `samples` {#function-samples}

<div class="function-declaration" id="function-samples-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> <a href="../../core/types.html#symbol-f32">f32</a>&gt; samples() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Every sample, interleaved by channel.</p>

## Operator Overloads

### Public

#### `operator=` {#operator-3d}

<div class="function-declaration" id="operator-3d-1"><pre><code><a href="pcm-buffer.html">pcm_buffer</a> &amp;<span class="k">operator</span>=(
    <span class="k">const</span> <a href="pcm-buffer.html">pcm_buffer</a> &amp;) = <span class="k">default</span>;</code></pre></div>

<div class="function-declaration" id="operator-3d-2"><pre><code><a href="pcm-buffer.html">pcm_buffer</a> &amp;<span class="k">operator</span>=(
    <a href="pcm-buffer.html">pcm_buffer</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

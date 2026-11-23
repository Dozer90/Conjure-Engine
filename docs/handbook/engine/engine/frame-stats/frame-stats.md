---
kind: class
---

[Engine](../../../engine.md) → [Engine](../../engine.md) → [frame_stats.h](../frame-stats.md)

# frame_stats
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::engine` |
| Include File Path | `engine/include/conjure/engine/frame_stats.h` |
| Source File Path | `engine/src/engine/frame_stats.cpp` |
| API ↗ | [The frame, layers and time](../../../../api/engine/frame-layers-and-time.md), [Engine: the application and the frame](../../../../api/engine/index.md) |

## Syntax

```cpp
class CONJURE_API frame_stats
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">frame_stats();</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">frame_stats(
    <span class="k">const</span> <a href="frame-stats.html">frame_stats</a> &amp;) = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~frame_stats();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">HISTORY_SIZE</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../core/types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">240</span></code></td><td>Samples kept (four seconds at 60 Hz).</td></tr>
<tr><td>private</td><td><code class="signature">mCount</code></td><td><code class="signature"><a href="../../core/types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mSamples</code></td><td><code class="signature">std::<wbr>array&lt;<wbr>frame_<wbr>sample,<wbr> HISTORY_<wbr>SIZE&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Total<wbr>Committed</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Total<wbr>Frames<wbr>With<wbr>Dropped<wbr>Time</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mWriteIndex</code></td><td><code class="signature"><a href="../../core/types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `average_frame_milliseconds` {#function-average-frame-milliseconds}

<div class="function-declaration" id="function-average-frame-milliseconds-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-f32">f32</a> average_frame_milliseconds(
    <a href="../../core/types.html#symbol-usize">usize</a> frameCount) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Mean <code>frameMilliseconds</code> over the latest <code>min(frameCount, <a href="#function-sample-count-1">sample_count</a>())</code> samples; 0 when empty.</p>

#### `clear` {#function-clear}

<div class="function-declaration" id="function-clear-1"><pre><code><span class="k">void</span> clear() <span class="k">noexcept</span>;</code></pre></div>

<p>Forgets every sample.</p>

#### `commit` {#function-commit}

<div class="function-declaration" id="function-commit-1"><pre><code><span class="k">void</span> commit(
    <span class="k">const</span> <a href="frame-sample.html">frame_sample</a> &amp;sample);</code></pre></div>

<p>Stores a sample, overwriting the oldest once the ring is full.</p>

#### `history` {#function-history}

<div class="function-declaration" id="function-history-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> <a href="frame-sample.html">frame_sample</a>&gt; history() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The ring in storage order: <code>HISTORY_SIZE</code> slots once full, the committed prefix before that. Use <code><a href="#function-sample-at-1">sample_at</a>()</code> for chronological access.</p>

#### `latest` {#function-latest}

<div class="function-declaration" id="function-latest-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="frame-sample.html">frame_sample</a> &amp;latest() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The last committed sample (a default sample before the first commit).</p>

#### `sample_at` {#function-sample-at}

<div class="function-declaration" id="function-sample-at-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="frame-sample.html">frame_sample</a> &amp;sample_at(
    <a href="../../core/types.html#symbol-usize">usize</a> age) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The sample <code>age</code> frames before the latest (0 = latest); a default sample beyond the history.</p>

#### `sample_count` {#function-sample-count}

<div class="function-declaration" id="function-sample-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-usize">usize</a> sample_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Samples currently in the ring (at most <code>HISTORY_SIZE</code>).</p>

#### `total_committed` {#function-total-committed}

<div class="function-declaration" id="function-total-committed-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u64">u64</a> total_committed() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Samples committed since construction.</p>

#### `total_frames_with_dropped_time` {#function-total-frames-with-dropped-time}

<div class="function-declaration" id="function-total-frames-with-dropped-time-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u64">u64</a> total_frames_with_dropped_time() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Frames in which the spiral-of-death guard dropped fixed time, since construction (one per frame, however much time that frame dropped).</p>

## Operator Overloads

### Public

#### `operator=` {#operator-3d}

<div class="function-declaration" id="operator-3d-1"><pre><code><a href="frame-stats.html">frame_stats</a> &amp;<span class="k">operator</span>=(
    <span class="k">const</span> <a href="frame-stats.html">frame_stats</a> &amp;) = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

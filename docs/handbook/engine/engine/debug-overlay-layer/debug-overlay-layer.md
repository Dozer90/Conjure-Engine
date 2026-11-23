---
kind: class
---

[Engine](../../../engine.md) → [Engine](../../engine.md) → [debug_overlay_layer.h](../debug-overlay-layer.md)

# debug_overlay_layer
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::engine` |
| Include File Path | `engine/include/conjure/engine/debug_overlay_layer.h` |
| Source File Path | `engine/src/engine/debug_overlay_layer.cpp` |
| API ↗ | [The frame, layers and time](../../../../api/engine/frame-layers-and-time.md), [Engine: the application and the frame](../../../../api/engine/index.md) |

## Syntax

```cpp
class CONJURE_API debug_overlay_layer final : public layer
```

## Inheritance Hierarchy


[`layer`](../layer/layer.md) → `debug_overlay_layer`

## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">debug_overlay_layer();</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~debug_overlay_layer() <span class="k">override</span>;</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">DEFAULT_<wbr>LOG_<wbr>INTERVAL_<wbr>FRAMES</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">300</span></code></td><td>Default logging interval (five seconds at 60 Hz).</td></tr>
<tr><td>private</td><td><code class="signature">mFramesSeen</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Log<wbr>Interval<wbr>Frames</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u64">u64</a></code></td><td><code class="signature">DEFAULT_<wbr>LOG_<wbr>INTERVAL_<wbr>FRAMES</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mText</code></td><td><code class="signature">std::string</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mVisible</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">true</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `format_sample` {#function-format-sample}

<div class="function-declaration" id="function-format-sample-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> std::string format_sample(
    <span class="k">const</span> <a href="../engine-context/engine-context.html">engine_context</a> &amp;context);</code></pre></div>

<p>The text the overlay would show for a sample: the formatting without the layer.</p>

#### `frames_seen` {#function-frames-seen}

<div class="function-declaration" id="function-frames-seen-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u64">u64</a> frames_seen() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Frames observed by <code>on_frame_end</code>.</p>

#### `is_visible` {#function-is-visible}

<div class="function-declaration" id="function-is-visible-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_visible() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `log_interval_frames` {#function-log-interval-frames}

<div class="function-declaration" id="function-log-interval-frames-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u64">u64</a> log_interval_frames() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `on_frame_end` {#function-on-frame-end}

<div class="function-declaration" id="function-on-frame-end-1"><pre><code><span class="k">void</span> on_frame_end(
    <a href="../engine-context/engine-context.html">engine_context</a> &amp;context) <span class="k">override</span>;</code></pre></div>

<p>Formats the latest committed sample and logs it when the interval elapsed.</p>

#### `on_imgui` {#function-on-imgui}

<div class="function-declaration" id="function-on-imgui-1"><pre><code><span class="k">void</span> on_imgui(
    <a href="../engine-context/engine-context.html">engine_context</a> &amp;context) <span class="k">override</span>;</code></pre></div>

<p>Draws the text in an ImGui window when a frame is open (editor builds) and the overlay is visible.</p>

#### `set_log_interval_frames` {#function-set-log-interval-frames}

<div class="function-declaration" id="function-set-log-interval-frames-1"><pre><code><span class="k">void</span> set_log_interval_frames(
    <a href="../../core/types.html#symbol-u64">u64</a> frames) <span class="k">noexcept</span>;</code></pre></div>

<p>Frames between two <code>INFO</code> log lines; 0 never logs.</p>

#### `set_visible` {#function-set-visible}

<div class="function-declaration" id="function-set-visible-1"><pre><code><span class="k">void</span> set_visible(
    <span class="k">bool</span> visible) <span class="k">noexcept</span>;</code></pre></div>

<p>Shows or hides the ImGui window; the text is formatted either way.</p>

#### `text` {#function-text}

<div class="function-declaration" id="function-text-1"><pre><code>[[<span class="na">nodiscard</span>]] std::string_view text() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The text of the last frame (empty before the first <code>on_frame_end</code>).</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

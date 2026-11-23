---
kind: class
---

[Engine](../../../engine.md) → [Engine](../../engine.md) → [imgui_layer.h](../imgui-layer.md)

# imgui_layer
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::engine` |
| Include File Path | `engine/include/conjure/engine/imgui_layer.h` |
| Source File Path | `engine/src/engine/imgui_layer.cpp` |
| API ↗ | [The frame, layers and time](../../../../api/engine/frame-layers-and-time.md), [Engine: the application and the frame](../../../../api/engine/index.md) |

## Syntax

```cpp
class CONJURE_API imgui_layer final : public layer
```

## Inheritance Hierarchy


[`layer`](../layer/layer.md) → `imgui_layer`

## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">imgui_layer();</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~imgui_layer() <span class="k">override</span>;</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mClipboard</code></td><td><code class="signature">std::string</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mContext</code></td><td><code class="signature"><a href="../imgui-layer.html">ImGuiContext</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mFrameOpen</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mStats</code></td><td><code class="signature"><a href="imgui-layer-stats.html">imgui_layer_stats</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mWindow</code></td><td><code class="signature"><a href="../../platform/window/window.html">platform::window</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `clipboard_text` {#function-clipboard-text}

<div class="function-declaration" id="function-clipboard-text-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <span class="k">char</span> *clipboard_text();</code></pre></div>

<p>The platform clipboard as ImGui reads it (the string stays valid until the next call).</p>

#### `context` {#function-context}

<div class="function-declaration" id="function-context-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../imgui-layer.html">ImGuiContext</a> *context() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The context this layer owns (<code>nullptr</code> before <code>on_attach</code> / after <code>on_detach</code>).</p>

#### `is_frame_open` {#function-is-frame-open}

<div class="function-declaration" id="function-is-frame-open-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_frame_open() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True between <code>on_frame_begin</code> and <code>on_imgui_end</code> of the current frame.</p>

#### `on_attach` {#function-on-attach}

<div class="function-declaration" id="function-on-attach-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; on_attach(
    <a href="../engine-context/engine-context.html">engine_context</a> &amp;context) <span class="k">override</span>;</code></pre></div>

<p>Creates the context, builds the default font atlas, sizes the display from the window and wires the clipboard to the platform window.</p>

#### `on_detach` {#function-on-detach}

<div class="function-declaration" id="function-on-detach-1"><pre><code><span class="k">void</span> on_detach(
    <a href="../engine-context/engine-context.html">engine_context</a> &amp;context) <span class="k">override</span>;</code></pre></div>

<p>Destroys the context.</p>

#### `on_event` {#function-on-event}

<div class="function-declaration" id="function-on-event-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> on_event(
    <a href="../engine-context/engine-context.html">engine_context</a> &amp;context,
    <span class="k">const</span> <a href="../../platform/event.html#symbol-event">platform::event</a> &amp;event) <span class="k">override</span>;</code></pre></div>

<p>Forwards keys, text, mouse, scroll and focus to the ImGui IO. Mouse events are consumed while ImGui wants the mouse, key and text events while it wants the keyboard; key and mouse-button releases and window events are never consumed, so nothing pressed before ImGui took capture stays down in the engine&#x27;s input state.</p>

#### `on_frame_begin` {#function-on-frame-begin}

<div class="function-declaration" id="function-on-frame-begin-1"><pre><code><span class="k">void</span> on_frame_begin(
    <a href="../engine-context/engine-context.html">engine_context</a> &amp;context) <span class="k">override</span>;</code></pre></div>

<p>Updates display size, framebuffer scale and delta time, then <code>ImGui::NewFrame()</code>.</p>

#### `on_imgui_end` {#function-on-imgui-end}

<div class="function-declaration" id="function-on-imgui-end-1"><pre><code><span class="k">void</span> on_imgui_end(
    <a href="../engine-context/engine-context.html">engine_context</a> &amp;context) <span class="k">override</span>;</code></pre></div>

<p><code>ImGui::Render()</code> and <code>renderer::set_imgui_draw_data</code> (when a renderer exists).</p>

#### `set_clipboard_text` {#function-set-clipboard-text}

<div class="function-declaration" id="function-set-clipboard-text-1"><pre><code><span class="k">void</span> set_clipboard_text(
    std::string_view text);</code></pre></div>

<p>Writes the platform clipboard (ImGui&#x27;s copy path).</p>

#### `stats` {#function-stats}

<div class="function-declaration" id="function-stats-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="imgui-layer-stats.html">imgui_layer_stats</a> &amp;stats() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

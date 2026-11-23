---
kind: class
---

[Engine](../../../engine.md) → [Engine](../../engine.md) → [layer.h](../layer.md)

# layer
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::engine` |
| Include File Path | `engine/include/conjure/engine/layer.h` |
| Source File Path | `engine/src/engine/layer.cpp` |
| API ↗ | [The frame, layers and time](../../../../api/engine/frame-layers-and-time.md), [Engine: the application and the frame](../../../../api/engine/index.md) |

## Syntax

```cpp
class CONJURE_API layer
```

## Derived Classes


<details><summary>Classes that use this as a base</summary>

<ul>

<li><a href="../debug-overlay-layer/debug-overlay-layer.html">debug_overlay_layer</a></li>

<li><a href="../game-layer/game-layer.html">game_layer</a></li>

<li><a href="../imgui-layer/imgui-layer.html">imgui_layer</a></li>

</ul>

</details>

## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">explicit</span> layer(
    std::string name);</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">virtual</span> ~layer();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mName</code></td><td><code class="signature">std::string</code></td><td><code class="signature"><span class="s">""</span></code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `name` {#function-name}

<div class="function-declaration" id="function-name-1"><pre><code>[[<span class="na">nodiscard</span>]] std::string_view name() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `on_attach` {#function-on-attach}

<div class="function-declaration" id="function-on-attach-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; on_attach(
    <a href="../engine-context/engine-context.html">engine_context</a> &amp;context);</code></pre></div>

<p>Startup step 19 (or <code>layer_stack::apply_pending</code> for a <code><a href="layer.html">layer</a></code> pushed later). <code>A</code> failure fails <code>initialise()</code> with <code>LAYER_ATTACH_FAILED</code>.</p>

#### `on_detach` {#function-on-detach}

<div class="function-declaration" id="function-on-detach-1"><pre><code><span class="k">virtual</span> <span class="k">void</span> on_detach(
    <a href="../engine-context/engine-context.html">engine_context</a> &amp;context);</code></pre></div>

<p>Shutdown step 3 (reverse stack order) or the pop that removes the <code><a href="layer.html">layer</a></code>.</p>

#### `on_event` {#function-on-event}

<div class="function-declaration" id="function-on-event-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <span class="k">bool</span> on_event(
    <a href="../engine-context/engine-context.html">engine_context</a> &amp;context,
    <span class="k">const</span> <a href="../../platform/event.html#symbol-event">platform::event</a> &amp;event);</code></pre></div>

<p>Frame step 3, top-down. Return <code>true</code> to consume the event (it then never reaches the input state or the layers below).</p>

#### `on_frame_begin` {#function-on-frame-begin}

<div class="function-declaration" id="function-on-frame-begin-1"><pre><code><span class="k">virtual</span> <span class="k">void</span> on_frame_begin(
    <a href="../engine-context/engine-context.html">engine_context</a> &amp;context);</code></pre></div>

<p>Frame step 5: after the input snapshot, before <code>FRAME_BEGIN</code>.</p>

#### `on_frame_end` {#function-on-frame-end}

<div class="function-declaration" id="function-on-frame-end-1"><pre><code><span class="k">virtual</span> <span class="k">void</span> on_frame_end(
    <a href="../engine-context/engine-context.html">engine_context</a> &amp;context);</code></pre></div>

<p>Frame step 13: after <code>FRAME_END</code>.</p>

#### `on_imgui` {#function-on-imgui}

<div class="function-declaration" id="function-on-imgui-1"><pre><code><span class="k">virtual</span> <span class="k">void</span> on_imgui(
    <a href="../engine-context/engine-context.html">engine_context</a> &amp;context);</code></pre></div>

<p>Frame step 10: after <code>SYNC</code>, before <code>EXTRACT</code>, with the ImGui frame open (editor builds).</p>

#### `on_imgui_end` {#function-on-imgui-end}

<div class="function-declaration" id="function-on-imgui-end-1"><pre><code><span class="k">virtual</span> <span class="k">void</span> on_imgui_end(
    <a href="../engine-context/engine-context.html">engine_context</a> &amp;context);</code></pre></div>

<p>Frame step 10, after every <code><a href="layer.html">layer</a></code>&#x27;s <code>on_imgui</code>: <code><a href="../imgui-layer/imgui-layer.html">imgui_layer</a></code> ends the ImGui frame here and hands the draw data to the renderer, so the editor panels drawn in <code>on_imgui</code> are complete.</p>

#### `on_update` {#function-on-update}

<div class="function-declaration" id="function-on-update-1"><pre><code><span class="k">virtual</span> <span class="k">void</span> on_update(
    <a href="../engine-context/engine-context.html">engine_context</a> &amp;context);</code></pre></div>

<p>Frame step 7: after <code>UPDATE</code>, before <code>LATE_UPDATE</code> (skipped systems aside, also while <code>PAUSED</code>).</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

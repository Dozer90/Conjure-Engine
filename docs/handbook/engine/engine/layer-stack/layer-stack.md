---
kind: class
---

[Engine](../../../engine.md) → [Engine](../../engine.md) → [layer_stack.h](../layer-stack.md)

# layer_stack
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::engine` |
| Include File Path | `engine/include/conjure/engine/layer_stack.h` |
| Source File Path | `engine/src/engine/layer_stack.cpp` |
| API ↗ | [The frame, layers and time](../../../../api/engine/frame-layers-and-time.md), [Engine: the application and the frame](../../../../api/engine/index.md) |

## Syntax

```cpp
class CONJURE_API layer_stack
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">layer_stack();</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~layer_stack();</code></td><td>Destroys the layers top first (overlays, then layers), without detaching them: the owner calls <code>detach_all()</code> while the context is still alive.</td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mAttached</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mIterating</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mLayers</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr>std::<wbr>unique_<wbr>ptr&lt;<wbr><a href="../layer/layer.html">layer</a>&gt;&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Overlay<wbr>Begin</code></td><td><code class="signature"><a href="../../core/types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mPendingPop</code></td><td><code class="signature">std::vector&lt;<a href="../layer/layer.html">layer</a> *&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mPendingPush</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr>std::<wbr>unique_<wbr>ptr&lt;<wbr><a href="../layer/layer.html">layer</a>&gt;&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Pending<wbr>Push<wbr>Is<wbr>Overlay</code></td><td><code class="signature">std::vector&lt;<span class="k">bool</span>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `apply_pending` {#function-apply-pending}

<div class="function-declaration" id="function-apply-pending-1"><pre><code><span class="k">void</span> apply_pending(
    <a href="../engine-context/engine-context.html">engine_context</a> &amp;context);</code></pre></div>

<p>Frame step 13: pops first (detach, erase), then pushes (insert, attach; a failed attach logs an ERROR and drops the layer).</p>

#### `attach_all` {#function-attach-all}

<div class="function-declaration" id="function-attach-all-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; attach_all(
    <a href="../engine-context/engine-context.html">engine_context</a> &amp;context);</code></pre></div>

<p>Startup step 19: <code>on_attach</code> bottom-up. The first failure detaches the layers attached so far and returns <code>LAYER_ATTACH_FAILED</code> naming the layer.</p>

#### `detach_all` {#function-detach-all}

<div class="function-declaration" id="function-detach-all-1"><pre><code><span class="k">void</span> detach_all(
    <a href="../engine-context/engine-context.html">engine_context</a> &amp;context);</code></pre></div>

<p>Shutdown step 3: <code>on_detach</code> top-down; pending pushes are discarded, pending pops forgotten.</p>

#### `dispatch_event` {#function-dispatch-event}

<div class="function-declaration" id="function-dispatch-event-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> dispatch_event(
    <a href="../engine-context/engine-context.html">engine_context</a> &amp;context,
    <span class="k">const</span> <a href="../../platform/event.html#symbol-event">platform::event</a> &amp;event);</code></pre></div>

<p>Delivers an event top-down; <code>true</code> when a layer consumed it.</p>

#### `frame_begin` {#function-frame-begin}

<div class="function-declaration" id="function-frame-begin-1"><pre><code><span class="k">void</span> frame_begin(
    <a href="../engine-context/engine-context.html">engine_context</a> &amp;context);</code></pre></div>

#### `frame_end` {#function-frame-end}

<div class="function-declaration" id="function-frame-end-1"><pre><code><span class="k">void</span> frame_end(
    <a href="../engine-context/engine-context.html">engine_context</a> &amp;context);</code></pre></div>

#### `imgui` {#function-imgui}

<div class="function-declaration" id="function-imgui-1"><pre><code><span class="k">void</span> imgui(
    <a href="../engine-context/engine-context.html">engine_context</a> &amp;context);</code></pre></div>

#### `imgui_end` {#function-imgui-end}

<div class="function-declaration" id="function-imgui-end-1"><pre><code><span class="k">void</span> imgui_end(
    <a href="../engine-context/engine-context.html">engine_context</a> &amp;context);</code></pre></div>

#### `is_attached` {#function-is-attached}

<div class="function-declaration" id="function-is-attached-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_attached() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True between <code><a href="#function-attach-all-1">attach_all</a>()</code> and <code><a href="#function-detach-all-1">detach_all</a>()</code>.</p>

#### `layers` {#function-layers}

<div class="function-declaration" id="function-layers-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> std::unique_ptr&lt;<a href="../layer/layer.html">layer</a>&gt;&gt; layers() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Every layer bottom-up: layers first in push order, then overlays in push order.</p>

#### `overlay_count` {#function-overlay-count}

<div class="function-declaration" id="function-overlay-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-usize">usize</a> overlay_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Number of overlays (the tail of <code><a href="#function-layers-1">layers</a>()</code>).</p>

#### `pending_count` {#function-pending-count}

<div class="function-declaration" id="function-pending-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-usize">usize</a> pending_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Pushes and pops waiting for <code><a href="#function-apply-pending-1">apply_pending</a>()</code>.</p>

#### `pop` {#function-pop}

<div class="function-declaration" id="function-pop-1"><pre><code><span class="k">void</span> pop(
    <a href="../layer/layer.html">layer</a> &amp;target);</code></pre></div>

<p>Removes a layer or overlay. Deferred to <code><a href="#function-apply-pending-1">apply_pending</a>()</code> while attached or iterating, where an attached layer detaches with the real context before it is destroyed; before <code>attach_all</code> the layer is destroyed at once. <code>A</code> layer still waiting in the push queue is forgotten. Unknown layers are ignored.</p>

#### `push_layer` {#function-push-layer}

<div class="function-declaration" id="function-push-layer-1"><pre><code><a href="../layer/layer.html">layer</a> *push_layer(
    std::unique_ptr&lt;<a href="../layer/layer.html">layer</a>&gt; value);</code></pre></div>

<p>Adds a layer below the overlays and returns it. Deferred to <code><a href="#function-apply-pending-1">apply_pending</a>()</code> while attached or iterating (the reference stays valid either way). <code>A</code> <code>null</code> pointer asserts and is ignored.</p>

#### `push_overlay` {#function-push-overlay}

<div class="function-declaration" id="function-push-overlay-1"><pre><code><a href="../layer/layer.html">layer</a> *push_overlay(
    std::unique_ptr&lt;<a href="../layer/layer.html">layer</a>&gt; value);</code></pre></div>

<p>Adds an overlay above every layer and the earlier overlays.</p>

#### `update` {#function-update}

<div class="function-declaration" id="function-update-1"><pre><code><span class="k">void</span> update(
    <a href="../engine-context/engine-context.html">engine_context</a> &amp;context);</code></pre></div>

### Private

#### `erase` {#function-erase}

<div class="function-declaration" id="function-erase-1"><pre><code><span class="k">void</span> erase(
    <a href="../layer/layer.html">layer</a> &amp;target,
    <a href="../engine-context/engine-context.html">engine_context</a> *context);</code></pre></div>

#### `for_each_bottom_up` {#function-for-each-bottom-up}

<div class="function-declaration" id="function-for-each-bottom-up-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> Callable&gt; <span class="k">void</span> for_each_bottom_up(
    Callable &amp;&amp;callable);</code></pre></div>

#### `forget_pending_push` {#function-forget-pending-push}

<div class="function-declaration" id="function-forget-pending-push-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> forget_pending_push(
    <a href="../layer/layer.html">layer</a> &amp;target);</code></pre></div>

#### `insert` {#function-insert}

<div class="function-declaration" id="function-insert-1"><pre><code><span class="k">void</span> insert(
    std::unique_ptr&lt;<a href="../layer/layer.html">layer</a>&gt; value,
    <span class="k">bool</span> overlay);</code></pre></div>

#### `push` {#function-push}

<div class="function-declaration" id="function-push-1"><pre><code><a href="../layer/layer.html">layer</a> *push(
    std::unique_ptr&lt;<a href="../layer/layer.html">layer</a>&gt; value,
    <span class="k">bool</span> overlay);</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

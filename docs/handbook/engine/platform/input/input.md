---
kind: class
---

[Engine](../../../engine.md) → [Platform](../../platform.md) → [input.h](../input.md)

# input
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::platform` |
| Include File Path | `engine/include/conjure/platform/input.h` |
| Source File Path | `engine/src/platform/input.cpp` |
| API ↗ | [Input](../../../../api/platform/input.md) |

## Syntax

```cpp
class CONJURE_API input
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">input();</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~input();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">m<wbr>Frame<wbr>Counter</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mPending</code></td><td><code class="signature"><a href="../input-state/input-state.html">input_state</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mPublished</code></td><td><code class="signature"><a href="../input-state/input-state.html">input_state</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Skip<wbr>Next<wbr>Mouse<wbr>Delta</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">true</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mStats</code></td><td><code class="signature"><a href="input-stats.html">input_stats</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `apply` {#function-apply}

<div class="function-declaration" id="function-apply-1"><pre><code><span class="k">bool</span> apply(
    <span class="k">const</span> <a href="../event.html#symbol-event">event</a> &amp;value);</code></pre></div>

<p>Applies one pumped event. Returns <code>true</code> when it was an <code><a href="input.html">input</a></code> event; window events return <code>false</code> (a <code><a href="../event/focus-event.html">focus_event</a></code> with focused == <code>false</code> still triggers <code><a href="#function-release-all-1">release_all</a>()</code>).</p>

#### `end_frame` {#function-end-frame}

<div class="function-declaration" id="function-end-frame-1"><pre><code><span class="k">void</span> end_frame();</code></pre></div>

<p>Publishes the frame&#x27;s snapshot (frameIndex incremented) and clears edges, deltas, scroll and text in the accumulating state. Levels persist.</p>

#### `inject` {#function-inject}

<div class="function-declaration" id="function-inject-1"><pre><code><span class="k">void</span> inject(
    <span class="k">const</span> <a href="../event.html#symbol-input-event">input_event</a> &amp;value);</code></pre></div>

<p>Same state machine as <code><a href="#function-apply-1">apply</a>()</code>; the source for tests and the editor&#x27;s <code><a href="input.html">input</a>.inject</code>.</p>

<div class="function-declaration" id="function-inject-2"><pre><code><span class="k">void</span> inject(
    <span class="k">const</span> <a href="../event.html#symbol-event">event</a> &amp;value);</code></pre></div>

<p>Convenience overload: injects the event when it is an <code><a href="input.html">input</a></code> event, ignores it otherwise.</p>

<div class="function-declaration" id="function-inject-3"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> EventType&gt; <span class="k">requires</span> std::constructible_from&lt;input_event, <span class="k">const</span> EventType &amp;&gt; &amp;&amp; (!std::same_as&lt;EventType, <a href="../event.html#symbol-input-event">input_event</a>&gt;) &amp;&amp; (!std::same_as&lt;EventType, <a href="../event.html#symbol-event">event</a>&gt;) <span class="k">void</span> inject(
    <span class="k">const</span> EventType &amp;value);</code></pre></div>

<p>Injects one concrete <code><a href="input.html">input</a></code> alternative (<code><a href="../event/key-event.html">key_event</a></code>, <code><a href="../event/mouse-move-event.html">mouse_move_event</a></code>, ...) without the caller spelling the variant; both variants convert from the same structs, so this overload is what makes <code><a href="#function-inject-1">inject</a>(make_key(...))</code> unambiguous.</p>

#### `release_all` {#function-release-all}

<div class="function-declaration" id="function-release-all-1"><pre><code><span class="k">void</span> release_all() <span class="k">noexcept</span>;</code></pre></div>

<p>Releases every down key, mouse button and gamepad button with a released edge this frame and clears the modifiers (focus loss, the editor&#x27;s <code><a href="input.html">input</a>.clear</code>).</p>

#### `skip_next_mouse_delta` {#function-skip-next-mouse-delta}

<div class="function-declaration" id="function-skip-next-mouse-delta-1"><pre><code><span class="k">void</span> skip_next_mouse_delta() <span class="k">noexcept</span>;</code></pre></div>

<p>Makes the next <code><a href="../event/mouse-move-event.html">mouse_move_event</a></code> set the position without adding to mouseDelta. The window owner calls this after a cursor mode change, where the OS repositions the cursor and the first report would otherwise read as a jump.</p>

#### `snapshot` {#function-snapshot}

<div class="function-declaration" id="function-snapshot-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../input-state/input-state.html">input_state</a> &amp;snapshot() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The snapshot published by the last <code><a href="#function-end-frame-1">end_frame</a>()</code>. Readable from any thread until the next <code><a href="#function-end-frame-1">end_frame</a>()</code>; never hold the reference across one.</p>

#### `stats` {#function-stats}

<div class="function-declaration" id="function-stats-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="input-stats.html">input_stats</a> &amp;stats() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

### Private

#### `apply_cursor_enter` {#function-apply-cursor-enter}

<div class="function-declaration" id="function-apply-cursor-enter-1"><pre><code><span class="k">void</span> apply_cursor_enter(
    <span class="k">const</span> <a href="../event/cursor-enter-event.html">cursor_enter_event</a> &amp;value);</code></pre></div>

#### `apply_gamepad_axis` {#function-apply-gamepad-axis}

<div class="function-declaration" id="function-apply-gamepad-axis-1"><pre><code><span class="k">void</span> apply_gamepad_axis(
    <span class="k">const</span> <a href="../event/gamepad-axis-event.html">gamepad_axis_event</a> &amp;value);</code></pre></div>

#### `apply_gamepad_button` {#function-apply-gamepad-button}

<div class="function-declaration" id="function-apply-gamepad-button-1"><pre><code><span class="k">void</span> apply_gamepad_button(
    <span class="k">const</span> <a href="../event/gamepad-button-event.html">gamepad_button_event</a> &amp;value);</code></pre></div>

#### `apply_gamepad_connection` {#function-apply-gamepad-connection}

<div class="function-declaration" id="function-apply-gamepad-connection-1"><pre><code><span class="k">void</span> apply_gamepad_connection(
    <span class="k">const</span> <a href="../event/gamepad-connection-event.html">gamepad_connection_event</a> &amp;value);</code></pre></div>

#### `apply_input` {#function-apply-input}

<div class="function-declaration" id="function-apply-input-1"><pre><code><span class="k">void</span> apply_input(
    <span class="k">const</span> <a href="../event.html#symbol-input-event">input_event</a> &amp;value);</code></pre></div>

#### `apply_key` {#function-apply-key}

<div class="function-declaration" id="function-apply-key-1"><pre><code><span class="k">void</span> apply_key(
    <span class="k">const</span> <a href="../event/key-event.html">key_event</a> &amp;value);</code></pre></div>

#### `apply_mouse_button` {#function-apply-mouse-button}

<div class="function-declaration" id="function-apply-mouse-button-1"><pre><code><span class="k">void</span> apply_mouse_button(
    <span class="k">const</span> <a href="../event/mouse-button-event.html">mouse_button_event</a> &amp;value);</code></pre></div>

#### `apply_mouse_move` {#function-apply-mouse-move}

<div class="function-declaration" id="function-apply-mouse-move-1"><pre><code><span class="k">void</span> apply_mouse_move(
    <span class="k">const</span> <a href="../event/mouse-move-event.html">mouse_move_event</a> &amp;value);</code></pre></div>

#### `apply_mouse_scroll` {#function-apply-mouse-scroll}

<div class="function-declaration" id="function-apply-mouse-scroll-1"><pre><code><span class="k">void</span> apply_mouse_scroll(
    <span class="k">const</span> <a href="../event/mouse-scroll-event.html">mouse_scroll_event</a> &amp;value);</code></pre></div>

#### `apply_text` {#function-apply-text}

<div class="function-declaration" id="function-apply-text-1"><pre><code><span class="k">void</span> apply_text(
    <span class="k">const</span> <a href="../event/text-event.html">text_event</a> &amp;value);</code></pre></div>

#### `gamepad_for` {#function-gamepad-for}

<div class="function-declaration" id="function-gamepad-for-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../input-state/gamepad-state.html">gamepad_state</a> *gamepad_for(
    <a href="../../core/types.html#symbol-u8">u8</a> index);</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

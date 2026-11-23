---
kind: class
---

[Engine](../../../engine.md) → [Engine](../../engine.md) → [game_layer.h](../game-layer.md)

# game_layer
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::engine` |
| Include File Path | `engine/include/conjure/engine/game_layer.h` |
| Source File Path | `engine/src/engine/game_layer.cpp` |
| API ↗ | [The frame, layers and time](../../../../api/engine/frame-layers-and-time.md), [Engine: the application and the frame](../../../../api/engine/index.md) |

## Syntax

```cpp
class CONJURE_API game_layer final : public layer
```

## Inheritance Hierarchy


[`layer`](../layer/layer.md) → `game_layer`

## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">game_layer();</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~game_layer() <span class="k">override</span>;</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">m<wbr>Close<wbr>Requests</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `close_requests` {#function-close-requests}

<div class="function-declaration" id="function-close-requests-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> close_requests() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Close events seen so far.</p>

#### `on_attach` {#function-on-attach}

<div class="function-declaration" id="function-on-attach-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; on_attach(
    <a href="../engine-context/engine-context.html">engine_context</a> &amp;context) <span class="k">override</span>;</code></pre></div>

#### `on_event` {#function-on-event}

<div class="function-declaration" id="function-on-event-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> on_event(
    <a href="../engine-context/engine-context.html">engine_context</a> &amp;context,
    <span class="k">const</span> <a href="../../platform/event.html#symbol-event">platform::event</a> &amp;event) <span class="k">override</span>;</code></pre></div>

<p><code>A</code> <code><a href="../../platform/event/close-event.html">close_event</a></code> requests <code>exit_code::OK</code> through the application; nothing else is consumed.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

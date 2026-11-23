---
kind: class
---

[Engine](../../../engine.md) → [Core](../../core.md) → [event.h](../event.md)

# event::state
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::core` |
| Include File Path | `engine/include/conjure/core/event.h` |
| Source File Path | `engine/src/core/time/event.cpp` |
| API ↗ | [Time, events and random](../../../../api/core/time-and-events.md) |

## Syntax

```cpp
class state final : public detail::subscription_target
```

## Inheritance Hierarchy


[`detail::subscription_target`](detail-subscription-target.md) → `event::state`

## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">state() = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~state() <span class="k">override</span> = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mEmitDepth</code></td><td><code class="signature"><a href="../types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Needs<wbr>Compaction</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mNextId</code></td><td><code class="signature"><a href="../types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">1</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mPending</code></td><td><code class="signature">std::vector&lt;<a href="event-slot.html">slot</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mSlots</code></td><td><code class="signature">std::vector&lt;<a href="event-slot.html">slot</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `add` {#function-add}

<div class="function-declaration" id="function-add-1"><pre><code><a href="../types.html#symbol-u64">u64</a> add(
    <a href="../event.html#symbol-event-handler-type">handler_type</a> handler);</code></pre></div>

#### `count` {#function-count}

<div class="function-declaration" id="function-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../types.html#symbol-usize">usize</a> count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `emit` {#function-emit}

<div class="function-declaration" id="function-emit-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span>... Params&gt; <span class="k">void</span> emit(
    Params &amp;...params);</code></pre></div>

#### `is_subscribed` {#function-is-subscribed}

<div class="function-declaration" id="function-is-subscribed-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_subscribed(
    <a href="../types.html#symbol-u64">u64</a> id) <span class="k">const</span> <span class="k">noexcept</span> <span class="k">override</span>;</code></pre></div>

#### `unsubscribe` {#function-unsubscribe}

<div class="function-declaration" id="function-unsubscribe-1"><pre><code><span class="k">void</span> unsubscribe(
    <a href="../types.html#symbol-u64">u64</a> id) <span class="k">noexcept</span> <span class="k">override</span>;</code></pre></div>

### Private

#### `finish_emit` {#function-finish-emit}

<div class="function-declaration" id="function-finish-emit-1"><pre><code><span class="k">void</span> finish_emit();</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

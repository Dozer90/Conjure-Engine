---
kind: class
---

[Engine](../../../engine.md) → [Core](../../core.md) → [event.h](../event.md)

# event
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::core` |
| Include File Path | `engine/include/conjure/core/event.h` |
| Source File Path | `engine/src/core/time/event.cpp` |
| API ↗ | [Time, events and random](../../../../api/core/time-and-events.md) |

## Syntax

```cpp
template <typename... Args> class event
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">event() = <span class="k">default</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">event(
    <a href="event.html">event</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~event() = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mState</code></td><td><code class="signature">std::shared_ptr&lt;<a href="event-state.html">state</a>&gt;</code></td><td><code class="signature">std::<wbr>make_<wbr>shared&lt;<wbr><a href="event-state.html">state</a>&gt;()</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `emit` {#function-emit}

<div class="function-declaration" id="function-emit-1"><pre><code><span class="k">void</span> emit(
    Args... args) <span class="k">const</span>;</code></pre></div>

<p>Invokes every live handler in subscription order. Safe to call re-entrantly from a handler, and safe even when a handler destroys this <code><a href="event.html">event</a></code>.</p>

#### `subscribe` {#function-subscribe}

<div class="function-declaration" id="function-subscribe-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="event-subscription.html">event_subscription</a> subscribe(
    <a href="../event.html#symbol-event-handler-type">handler_type</a> handler);</code></pre></div>

<p>Registers a handler. The handler runs until the returned subscription is released or destroyed. An empty handler asserts and yields an inactive subscription.</p>

#### `subscriber_count` {#function-subscriber-count}

<div class="function-declaration" id="function-subscriber-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../types.html#symbol-usize">usize</a> subscriber_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Number of live subscriptions, including ones made during a running emit.</p>

### Private

#### `ensure_state` {#function-ensure-state}

<div class="function-declaration" id="function-ensure-state-1"><pre><code><span class="k">void</span> ensure_state();</code></pre></div>

## Operator Overloads

### Public

#### `operator=` {#operator-3d}

<div class="function-declaration" id="operator-3d-1"><pre><code><a href="event.html">event</a> &amp;<span class="k">operator</span>=(
    <a href="event.html">event</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

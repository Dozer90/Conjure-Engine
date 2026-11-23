---
kind: class
---

[Engine](../../../engine.md) → [Core](../../core.md) → [event_bus.h](../event-bus.md)

# event_bus::channel
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::core` |
| Include File Path | `engine/include/conjure/core/event_bus.h` |
| Source File Path | `engine/src/core/time/event_bus.cpp` |
| API ↗ | [Time, events and random](../../../../api/core/time-and-events.md) |

## Syntax

```cpp
template <typename Event> class channel final : public channel_base
```

## Inheritance Hierarchy


[`event_bus::channel_base`](event-bus-channel-base.md) → `event_bus::channel`

## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">channel() = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~channel() <span class="k">override</span> = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mSignal</code></td><td><code class="signature"><a href="../event/event.html">event</a>&lt;<span class="k">const</span> Event &amp;&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `signal` {#function-signal}

<div class="function-declaration" id="function-signal-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../event/event.html">event</a>&lt;<span class="k">const</span> Event &amp;&gt; &amp;signal() <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-signal-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../event/event.html">event</a>&lt;<span class="k">const</span> Event &amp;&gt; &amp;signal() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

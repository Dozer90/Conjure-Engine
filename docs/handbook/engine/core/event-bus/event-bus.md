---
kind: class
---

[Engine](../../../engine.md) → [Core](../../core.md) → [event_bus.h](../event-bus.md)

# event_bus
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::core` |
| Include File Path | `engine/include/conjure/core/event_bus.h` |
| Source File Path | `engine/src/core/time/event_bus.cpp` |
| API ↗ | [Time, events and random](../../../../api/core/time-and-events.md) |

## Syntax

```cpp
class CONJURE_API event_bus
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">event_bus();</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted. Move constructors have been deleted. Move assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~event_bus();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mChannels</code></td><td><code class="signature">std::<wbr>unordered_<wbr>map&lt;<wbr>std::<wbr>type_<wbr>index,<wbr> std::<wbr>unique_<wbr>ptr&lt;<wbr><a href="event-bus-channel-base.html">channel_<wbr>base</a>&gt;&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mQueue</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="../event-bus.html#symbol-event-bus-deferred-publish">deferred_<wbr>publish</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mQueueMutex</code></td><td><code class="signature"><span class="k">mutable</span> std::mutex</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `clear_queue` {#function-clear-queue}

<div class="function-declaration" id="function-clear-queue-1"><pre><code><span class="k">void</span> clear_queue();</code></pre></div>

<p>Drops every queued event without publishing it. Thread-safe.</p>

#### `dispatch_queued` {#function-dispatch-queued}

<div class="function-declaration" id="function-dispatch-queued-1"><pre><code><a href="../types.html#symbol-usize">usize</a> dispatch_queued();</code></pre></div>

<p>Publishes every event enqueued before the call, in enqueue order, and returns how many were published (events whose type has no subscriber count too). Events enqueued while dispatching (by handlers or other threads) wait for the next call; a handler may call this re-entrantly to drain them early. Main thread only.</p>

#### `enqueue` {#function-enqueue}

<div class="function-declaration" id="function-enqueue-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> Event&gt; <span class="k">void</span> enqueue(
    Event eventValue);</code></pre></div>

<p>Stores a copy of the event for the next <code><a href="#function-dispatch-queued-1">dispatch_queued</a>()</code>. Thread-safe.</p>

#### `publish` {#function-publish}

<div class="function-declaration" id="function-publish-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> Event&gt; <span class="k">void</span> publish(
    <span class="k">const</span> Event &amp;eventValue);</code></pre></div>

<p>Delivers the event to every subscriber of its type now, on the calling thread. Publishing a type nobody listens to does nothing.</p>

#### `queued_count` {#function-queued-count}

<div class="function-declaration" id="function-queued-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../types.html#symbol-usize">usize</a> queued_count() <span class="k">const</span>;</code></pre></div>

<p>Number of events waiting for <code><a href="#function-dispatch-queued-1">dispatch_queued</a>()</code>. Thread-safe.</p>

#### `subscribe` {#function-subscribe}

<div class="function-declaration" id="function-subscribe-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> Event&gt; [[<span class="na">nodiscard</span>]] <a href="../event/event-subscription.html">event_subscription</a> subscribe(
    std::function&lt;<span class="k">void</span>(<span class="k">const</span> Event &amp;)&gt; handler);</code></pre></div>

<p>Registers a handler for one event type. The subscription follows event semantics: RAII, move-only, inert once the bus is destroyed.</p>

#### `subscriber_count` {#function-subscriber-count}

<div class="function-declaration" id="function-subscriber-count-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> Event&gt; [[<span class="na">nodiscard</span>]] <a href="../types.html#symbol-usize">usize</a> subscriber_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Number of live subscriptions for one event type.</p>

### Private

#### `channel_for` {#function-channel-for}

<div class="function-declaration" id="function-channel-for-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> Event&gt; [[<span class="na">nodiscard</span>]] <a href="event-bus-channel.html">channel</a>&lt;Event&gt; &amp;channel_for();</code></pre></div>

#### `find_channel` {#function-find-channel}

<div class="function-declaration" id="function-find-channel-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> Event&gt; [[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="event-bus-channel.html">channel</a>&lt;Event&gt; *find_channel() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

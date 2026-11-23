---
kind: class
---

[Engine](../../../engine.md) → [Core](../../core.md) → [event.h](../event.md)

# event_subscription
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::core` |
| Include File Path | `engine/include/conjure/core/event.h` |
| Source File Path | `engine/src/core/time/event.cpp` |
| API ↗ | [Time, events and random](../../../../api/core/time-and-events.md) |

## Syntax

```cpp
class CONJURE_API event_subscription
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">event_subscription() <span class="k">noexcept</span>;</code></td><td>An inactive subscription.</td></tr>
<tr><td>public</td><td><code class="signature">event_subscription(
    std::weak_ptr&lt;<a href="detail-subscription-target.html">detail::subscription_target</a>&gt; target,
    <a href="../types.html#symbol-u64">u64</a> id) <span class="k">noexcept</span>;</code></td><td>Binds to a handler id on a target. Used by <code><a href="event.html">event</a></code> implementations.</td></tr>
<tr><td>public</td><td><code class="signature">event_subscription(
    <a href="event-subscription.html">event_subscription</a> &amp;&amp;other) <span class="k">noexcept</span>;</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~event_subscription();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mId</code></td><td><code class="signature"><a href="../types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mTarget</code></td><td><code class="signature">std::<wbr>weak_<wbr>ptr&lt;<wbr><a href="detail-subscription-target.html">detail::<wbr>subscription_<wbr>target</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `is_active` {#function-is-active}

<div class="function-declaration" id="function-is-active-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_active() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True while the handler is registered on a live <code><a href="event.html">event</a></code>.</p>

#### `release` {#function-release}

<div class="function-declaration" id="function-release-1"><pre><code><span class="k">void</span> release() <span class="k">noexcept</span>;</code></pre></div>

<p>Unsubscribes now. Safe to call more than once and on inactive subscriptions.</p>

## Operator Overloads

### Public

#### `operator=` {#operator-3d}

<div class="function-declaration" id="operator-3d-1"><pre><code><a href="event-subscription.html">event_subscription</a> &amp;<span class="k">operator</span>=(
    <a href="event-subscription.html">event_subscription</a> &amp;&amp;other) <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

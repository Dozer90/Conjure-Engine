---
kind: header
---

[Engine](../../engine.md) → [Core](../core.md)

# event.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/core/event.h` |
| API ↗ | [Time, events and random](../../../api/core/time-and-events.md) |

<p><code>A</code> multicast callback list. Handlers run synchronously on the emitting thread in subscription order. Movable (subscriptions follow the moved state), not copyable, and not thread-safe.</p>

## Structs

| File | Description |
|---|---|
| [`event::slot`](event/event-slot.md) |  |

## Classes

| File | Description |
|---|---|
| [`detail::subscription_target`](event/detail-subscription-target.md) | What an <code><a href="event/event-subscription.html">event_subscription</a></code> talks to. Events implement it through their shared state so a subscription outliving its <code><a href="event/event.html">event</a></code> finds the state gone and becomes inert. |
| [`event`](event/event.md) | <code>A</code> multicast callback list. Handlers run synchronously on the emitting thread in subscription order. Movable (subscriptions follow the moved state), not copyable, and not thread-safe. |
| [`event::state`](event/event-state.md) |  |
| [`event_subscription`](event/event-subscription.md) | Owns one subscription to an <code><a href="event/event.html">event</a></code> (or an <code><a href="event-bus/event-bus.html">event_bus</a></code> channel). Move-only; destroying or releasing it unsubscribes. When the <code><a href="event/event.html">event</a></code> is destroyed first the subscription becomes inert and <code>release()</code> is a no-op. |

## Type Aliases

### `event::handler_type` {#symbol-event-handler-type}

```cpp
using handler_type = std::function<void(Args...)>;
```

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="detailsubscription_target"></a>
<a id="detailsubscription_targetis_subscribed"></a>
<a id="detailsubscription_targetoperator"></a>
<a id="detailsubscription_targetsubscription_target"></a>
<a id="detailsubscription_targetsubscription_target_1"></a>
<a id="detailsubscription_targetunsubscribe"></a>
<a id="direct-header-dependencies"></a>
<a id="event"></a>
<a id="event_subscription"></a>
<a id="event_subscriptionevent_subscription"></a>
<a id="event_subscriptionevent_subscription_1"></a>
<a id="event_subscriptionis_active"></a>
<a id="event_subscriptionmid"></a>
<a id="event_subscriptionmtarget"></a>
<a id="event_subscriptionoperator"></a>
<a id="event_subscriptionrelease"></a>
<a id="eventemit"></a>
<a id="eventensure_state"></a>
<a id="eventevent"></a>
<a id="eventevent_1"></a>
<a id="eventhandler_type"></a>
<a id="eventmstate"></a>
<a id="eventoperator"></a>
<a id="eventslot"></a>
<a id="eventslotalive"></a>
<a id="eventslothandler"></a>
<a id="eventslotid"></a>
<a id="eventstate"></a>
<a id="eventstateadd"></a>
<a id="eventstatecount"></a>
<a id="eventstateemit"></a>
<a id="eventstatefinish_emit"></a>
<a id="eventstateis_subscribed"></a>
<a id="eventstatememitdepth"></a>
<a id="eventstatemneedscompaction"></a>
<a id="eventstatemnextid"></a>
<a id="eventstatempending"></a>
<a id="eventstatemslots"></a>
<a id="eventstatestate"></a>
<a id="eventstatestate_1"></a>
<a id="eventstateunsubscribe"></a>
<a id="eventsubscribe"></a>
<a id="eventsubscriber_count"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="other-declarations"></a>
<a id="structs-and-types"></a>
<a id="subscription_target"></a>
<a id="subscription_targetis_subscribed"></a>
<a id="subscription_targetoperator"></a>
<a id="subscription_targetsubscription_target"></a>
<a id="subscription_targetsubscription_target_1"></a>
<a id="subscription_targetunsubscribe"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->

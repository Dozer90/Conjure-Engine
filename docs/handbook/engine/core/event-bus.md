---
kind: header
---

[Engine](../../engine.md) → [Core](../core.md)

# event_bus.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/core/event_bus.h` |
| API ↗ | [Time, events and random](../../../api/core/time-and-events.md) |

<p>Engine-wide typed publish/subscribe.</p>



## Classes

| File | Description |
|---|---|
| [`event_bus`](event-bus/event-bus.md) |  |
| [`event_bus::channel`](event-bus/event-bus-channel.md) |  |
| [`event_bus::channel_base`](event-bus/event-bus-channel-base.md) | Type-erased owner of one event type&#x27;s subscriber list. |

## Type Aliases

### `event_bus::deferred_publish` {#symbol-event-bus-deferred-publish}

```cpp
using deferred_publish = std::function<void(event_bus &bus)>;
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
<a id="direct-header-dependencies"></a>
<a id="event_bus"></a>
<a id="event_buschannel"></a>
<a id="event_buschannel_base"></a>
<a id="event_buschannel_basechannel_base"></a>
<a id="event_buschannel_basechannel_base_1"></a>
<a id="event_buschannel_baseoperator"></a>
<a id="event_buschannel_for"></a>
<a id="event_buschannelchannel"></a>
<a id="event_buschannelchannel_1"></a>
<a id="event_buschannelmsignal"></a>
<a id="event_buschannelsignal"></a>
<a id="event_busclear_queue"></a>
<a id="event_busdeferred_publish"></a>
<a id="event_busdispatch_queued"></a>
<a id="event_busenqueue"></a>
<a id="event_busevent_bus"></a>
<a id="event_busevent_bus_1"></a>
<a id="event_busfind_channel"></a>
<a id="event_busmchannels"></a>
<a id="event_busmqueue"></a>
<a id="event_busmqueuemutex"></a>
<a id="event_busoperator"></a>
<a id="event_buspublish"></a>
<a id="event_busqueued_count"></a>
<a id="event_bussubscribe"></a>
<a id="event_bussubscriber_count"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="other-declarations"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->

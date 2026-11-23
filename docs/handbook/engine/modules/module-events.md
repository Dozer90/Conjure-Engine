---
kind: header
---

[Engine](../../engine.md) → [Modules](../modules.md)

# module_events.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/modules/module_events.h` |
| API ↗ | [Writing a module](../../../api/modules/writing-a-module.md) |

<p>Events the module manager publishes on the engine&#x27;s <code><a href="../core/event-bus/event-bus.html">core::event_bus</a></code>. All are published synchronously on the main thread from inside the manager&#x27;s mutators.</p>

## Structs

| File | Description |
|---|---|
| [`module_load_failed_event`](module-events/module-load-failed-event.md) | <code>A</code> load attempt failed and applied nothing; the error is also <code>module_registry::last_error()</code>. |
| [`module_loaded_event`](module-events/module-loaded-event.md) | <code>A</code> module is <code>LOADED</code>: every consumer attached and onActivated ran. |
| [`module_unloaded_event`](module-events/module-unloaded-event.md) | <code>A</code> module is <code>UNLOADED</code> and its id is stale; the name is kept for listeners that key by name. |
| [`module_unloading_event`](module-events/module-unloading-event.md) | <code>A</code> module is about to detach. <code>reloading</code> is <code>true</code> during a reload cascade, when the module comes back under the same id. |

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="classes"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="module_load_failed_event"></a>
<a id="module_load_failed_eventerror"></a>
<a id="module_load_failed_eventid"></a>
<a id="module_loaded_event"></a>
<a id="module_loaded_eventid"></a>
<a id="module_unloaded_event"></a>
<a id="module_unloaded_eventid"></a>
<a id="module_unloaded_eventname"></a>
<a id="module_unloading_event"></a>
<a id="module_unloading_eventid"></a>
<a id="module_unloading_eventreloading"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->

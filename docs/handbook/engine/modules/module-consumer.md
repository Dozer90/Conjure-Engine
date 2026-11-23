---
kind: header
---

[Engine](../../engine.md) → [Modules](../modules.md)

# module_consumer.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/modules/module_consumer.h` |
| API ↗ | [Loading and managing modules](../../../api/modules/loading-and-managing.md) |

<p>One area&#x27;s view of module registrations. Every call runs on the main thread with no jobs in flight.</p>



## Classes

| File | Description |
|---|---|
| [`module_consumer`](module-consumer/module-consumer.md) | One area&#x27;s view of module registrations. Every call runs on the main thread with no jobs in flight. |
| [`reload_context`](module-consumer/reload-context.md) | Keyed binary blobs a consumer stores on detach during a reload and reads back after attach. |

## Forward Declarations

### `module_context` {#symbol-module-context}

```cpp
class module_context;
```

### `module_registry` {#symbol-module-registry}

```cpp
class module_registry;
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
<a id="extension-points"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="module_consumer"></a>
<a id="module_consumerattach"></a>
<a id="module_consumercan_detach"></a>
<a id="module_consumerdetach"></a>
<a id="module_consumerlive_use_count"></a>
<a id="module_consumermodule_consumer"></a>
<a id="module_consumermodule_consumer_1"></a>
<a id="module_consumername"></a>
<a id="module_consumeroperator"></a>
<a id="module_consumerparticipates_in_managed_reload"></a>
<a id="module_consumerrestore"></a>
<a id="module_consumervalidate"></a>
<a id="module_context"></a>
<a id="module_registry"></a>
<a id="reload_context"></a>
<a id="reload_contextcontains"></a>
<a id="reload_contextfetch"></a>
<a id="reload_contextmblobs"></a>
<a id="reload_contextoperator"></a>
<a id="reload_contextreload_context"></a>
<a id="reload_contextreload_context_1"></a>
<a id="reload_contextsize"></a>
<a id="reload_contextstore"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->

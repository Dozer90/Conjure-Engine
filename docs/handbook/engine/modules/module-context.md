---
kind: header
---

[Engine](../../engine.md) → [Modules](../modules.md)

# module_context.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/modules/module_context.h` |
| API ↗ | [Writing a module](../../../api/modules/writing-a-module.md) |

<p>What a module sees of the host. One context exists per <code>LOADED</code> module; it is valid during the entry and in both lifecycle callbacks and is destroyed before the binary is unmapped, taking every retained subscription with it.</p>



## Classes

| File | Description |
|---|---|
| [`module_context`](module-context/module-context.md) |  |

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
<a id="module_context"></a>
<a id="module_contextbegin_entry"></a>
<a id="module_contextend_entry"></a>
<a id="module_contextengine_version"></a>
<a id="module_contexthost"></a>
<a id="module_contextid"></a>
<a id="module_contextis_entry_running"></a>
<a id="module_contextmanifest"></a>
<a id="module_contextmengineversion"></a>
<a id="module_contextmhost"></a>
<a id="module_contextmid"></a>
<a id="module_contextmmanifest"></a>
<a id="module_contextmodule_context"></a>
<a id="module_contextmodule_context_1"></a>
<a id="module_contextmregistrar"></a>
<a id="module_contextmrootdirectory"></a>
<a id="module_contextmservices"></a>
<a id="module_contextmsubscriptions"></a>
<a id="module_contextoperator"></a>
<a id="module_contextregistrar"></a>
<a id="module_contextrequire_service"></a>
<a id="module_contextretain"></a>
<a id="module_contextretained_subscription_count"></a>
<a id="module_contextroot_directory"></a>
<a id="module_contexttry_service"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->

---
kind: header
---

[Engine](../../engine.md) → [Modules](../modules.md)

# module_manager.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/modules/module_manager.h` |
| API ↗ | [Loading and managing modules](../../../api/modules/loading-and-managing.md) |

<p>The module manager: discovers manifests, resolves dependencies, loads, unloads and reloads modules in order and hands registrations to consumers. Every mutator runs on the main thread at the engine&#x27;s structural sync point (asserted); <code>request_</code>* enqueue work that <code>apply_pending()</code> performs at the next frame start.</p>

## Structs

| File | Description |
|---|---|
| [`module_manager::pending_operation`](module-manager/module-manager-pending-operation.md) |  |
| [`module_manager_desc`](module-manager/module-manager-desc.md) |  |

## Classes

| File | Description |
|---|---|
| [`module_manager`](module-manager/module-manager.md) |  |

## Enumerations

### `module_manager::pending_kind` {#symbol-module-manager-pending-kind}

```cpp
enum class pending_kind : u8 {
    LOAD,
    UNLOAD,
    RELOAD
};
```

## Forward Declarations

### `module_record` {#symbol-module-record}

```cpp
struct module_record;
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
<a id="module_manager"></a>
<a id="module_manager_desc"></a>
<a id="module_manager_descallowsharedlibraries"></a>
<a id="module_manager_descbinaryconfig"></a>
<a id="module_manager_descdefaultunloadpolicy"></a>
<a id="module_manager_descenginefeatures"></a>
<a id="module_manager_descengineversion"></a>
<a id="module_manager_deschost"></a>
<a id="module_manager_descisinsideframe"></a>
<a id="module_manager_desckeepbinariesresident"></a>
<a id="module_manager_descshadowcopybinaries"></a>
<a id="module_manager_descshadowcopydirectory"></a>
<a id="module_manageradd_consumer"></a>
<a id="module_manageradd_manifest"></a>
<a id="module_manageradd_static_module"></a>
<a id="module_managerany_loaded"></a>
<a id="module_managerapply_one"></a>
<a id="module_managerapply_pending"></a>
<a id="module_managercheck_sync_point"></a>
<a id="module_managerchecked_record"></a>
<a id="module_managerclose_binary"></a>
<a id="module_managercollect_reload_cascade"></a>
<a id="module_managerdesc"></a>
<a id="module_managerdetach_record"></a>
<a id="module_managerdiscover"></a>
<a id="module_managerenqueue"></a>
<a id="module_managerexport_layout"></a>
<a id="module_managerfail_record"></a>
<a id="module_managerfailed_dependency_of"></a>
<a id="module_managerlink_dependencies"></a>
<a id="module_managerload"></a>
<a id="module_managerload_all"></a>
<a id="module_managerload_one"></a>
<a id="module_managerload_resolved"></a>
<a id="module_managermconsumers"></a>
<a id="module_managermdesc"></a>
<a id="module_managermevents"></a>
<a id="module_managermhaspending"></a>
<a id="module_managermnextloadorder"></a>
<a id="module_managermodule_manager"></a>
<a id="module_managermodule_manager_1"></a>
<a id="module_managermpending"></a>
<a id="module_managermregistry"></a>
<a id="module_managermservices"></a>
<a id="module_manageroperator"></a>
<a id="module_managerpending_count"></a>
<a id="module_managerpending_kind"></a>
<a id="module_managerpending_kindload"></a>
<a id="module_managerpending_kindreload"></a>
<a id="module_managerpending_kindunload"></a>
<a id="module_managerpending_operation"></a>
<a id="module_managerpending_operationid"></a>
<a id="module_managerpending_operationkind"></a>
<a id="module_managerpending_operationpolicy"></a>
<a id="module_managerpending_operationscope"></a>
<a id="module_managerprecheck_unload"></a>
<a id="module_managerprovide_service"></a>
<a id="module_managerregister_manifest"></a>
<a id="module_managerregistry"></a>
<a id="module_managerreload"></a>
<a id="module_managerreload_full"></a>
<a id="module_managerreload_managed"></a>
<a id="module_managerremove_consumer"></a>
<a id="module_managerrequest_load"></a>
<a id="module_managerrequest_reload"></a>
<a id="module_managerrequest_unload"></a>
<a id="module_managerservices"></a>
<a id="module_managerunlink_dependencies"></a>
<a id="module_managerunload"></a>
<a id="module_managerunload_all"></a>
<a id="module_managerunload_checked"></a>
<a id="module_record"></a>
<a id="other-declarations"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->

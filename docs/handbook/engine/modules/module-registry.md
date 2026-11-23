---
kind: header
---

[Engine](../../engine.md) → [Modules](../modules.md)

# module_registry.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/modules/module_registry.h` |
| API ↗ | [Loading and managing modules](../../../api/modules/loading-and-managing.md) |

<p>Read-only view of every registered module. Mutated only by the owning <code><a href="module-manager/module-manager.html">module_manager</a></code> at the structural sync point; between sync points it is immutable and may be read from job threads. Getters taking an id assert on an invalid or stale id and return an empty value, so a stale handle never dereferences another module&#x27;s record.</p>



## Classes

| File | Description |
|---|---|
| [`module_registry`](module-registry/module-registry.md) |  |

## Forward Declarations

### `module_manager` {#symbol-module-manager}

```cpp
class module_manager;
```

### `module_record` {#symbol-module-record}

```cpp
struct module_record;
```

### `module_registry::registry_state` {#symbol-module-registry-registry-state}

```cpp
struct registry_state;
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
<a id="module_record"></a>
<a id="module_registry"></a>
<a id="module_registryadd_record"></a>
<a id="module_registryall"></a>
<a id="module_registryappend_load_order"></a>
<a id="module_registrybinary_path"></a>
<a id="module_registrydependencies"></a>
<a id="module_registrydependents"></a>
<a id="module_registryfind"></a>
<a id="module_registryforget_ownership"></a>
<a id="module_registryis_valid"></a>
<a id="module_registrylast_error"></a>
<a id="module_registryload_count"></a>
<a id="module_registryload_order"></a>
<a id="module_registrymanifest"></a>
<a id="module_registrymodule_manager"></a>
<a id="module_registrymodule_registry"></a>
<a id="module_registrymodule_registry_1"></a>
<a id="module_registrymstate"></a>
<a id="module_registrynote_ownership"></a>
<a id="module_registryoperator"></a>
<a id="module_registryowner_of"></a>
<a id="module_registryrecord"></a>
<a id="module_registryrecord_at"></a>
<a id="module_registryregistrations"></a>
<a id="module_registryregistry_state"></a>
<a id="module_registryremove_load_order"></a>
<a id="module_registryretire_generation"></a>
<a id="module_registryroot_directory"></a>
<a id="module_registrysize"></a>
<a id="module_registrystate"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->

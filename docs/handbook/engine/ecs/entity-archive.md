---
kind: header
---

[Engine](../../engine.md) → [ECS](../ecs.md)

# entity_archive.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/ecs/entity_archive.h` |
| API ↗ | [Archives and serialisation](../../../api/ecs/archives-and-serialization.md) |

<p><code>A</code> typed copy of a set of entities, bound to the registry whose ids it uses. Move-only; the destructor destroys non-trivial component payloads.</p>

## Structs

| File | Description |
|---|---|
| [`archive_component`](entity-archive/archive-component.md) | One component of one archived entity. The bytes are an object image produced by <code>ops.copy</code> (or memcpy) and destroyed by the archive; tags have no bytes. |
| [`archive_entry`](entity-archive/archive-entry.md) | One archived entity. |
| [`archive_options`](entity-archive/archive-options.md) | Capture options. |

## Classes

| File | Description |
|---|---|
| [`entity_archive`](entity-archive/entity-archive.md) | <code>A</code> typed copy of a set of entities, bound to the registry whose ids it uses. Move-only; the destructor destroys non-trivial component payloads. |

## Forward Declarations

### `component_registry` {#symbol-component-registry}

```cpp
class component_registry;
```

### `world` {#symbol-world}

```cpp
class world;
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
<a id="archive_component"></a>
<a id="archive_componentbytes"></a>
<a id="archive_componentid"></a>
<a id="archive_entry"></a>
<a id="archive_entrycomponents"></a>
<a id="archive_entrymask"></a>
<a id="archive_options"></a>
<a id="archive_optionskeepexternalreferences"></a>
<a id="component_registry"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="entity_archive"></a>
<a id="entity_archivecapture"></a>
<a id="entity_archivecomponent_bytes"></a>
<a id="entity_archivedestroy_payloads"></a>
<a id="entity_archiveentity_archive"></a>
<a id="entity_archiveentity_archive_1"></a>
<a id="entity_archiveentity_count"></a>
<a id="entity_archiveentries"></a>
<a id="entity_archivementries"></a>
<a id="entity_archivemregistry"></a>
<a id="entity_archiveoperator"></a>
<a id="entity_archivepush_entry"></a>
<a id="entity_archiveregistry"></a>
<a id="entity_archivevalidate"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<a id="world"></a>
<!-- doc-legacy-anchors:end -->

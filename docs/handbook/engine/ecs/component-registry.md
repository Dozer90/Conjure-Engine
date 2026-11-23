---
kind: header
---

[Engine](../../engine.md) → [ECS](../ecs.md)

# component_registry.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/ecs/component_registry.h` |
| API ↗ | [Entities and components](../../../api/ecs/entities-and-components.md), [Entities, components and systems](../../../api/ecs/index.md) |

<p>Owns the <code><a href="component/component-info.html">component_info</a></code> of every registered type. Thread-safe: every method takes the registry mutex except <code>info()</code> and <code>count()</code>, which read the pre-reserved, append-only vector guarded by the atomic count. Non-copyable and non-movable (worlds and caches hold its address).</p>

## Structs

| File | Description |
|---|---|
| [`dynamic_component_desc`](component-registry/dynamic-component-desc.md) | <code>A</code> runtime-described component type. <code>DYNAMIC</code> \| <code>TRIVIAL</code> are forced on; the alignment must be a power of two in [1, 16]. |
| [`dynamic_field_desc`](component-registry/dynamic-field-desc.md) | One field of a runtime-described (<code>C</code>#) component. |

## Classes

| File | Description |
|---|---|
| [`component_registry`](component-registry/component-registry.md) | Owns the <code><a href="component/component-info.html">component_info</a></code> of every registered type. Thread-safe: every method takes the registry mutex except <code>info()</code> and <code>count()</code>, which read the pre-reserved, append-only vector guarded by the atomic count. Non-copyable and non-movable (worlds and caches hold its address). |

## Forward Declarations

### `component_type_desc` {#symbol-component-type-desc}

```cpp
struct component_type_desc;
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
<a id="component_registry"></a>
<a id="component_registryattach_world"></a>
<a id="component_registrybuild_dynamic_type"></a>
<a id="component_registrycollect_field_offsets"></a>
<a id="component_registrycomponent_registry"></a>
<a id="component_registrycomponent_registry_1"></a>
<a id="component_registrycount"></a>
<a id="component_registrydetach_world"></a>
<a id="component_registryfind"></a>
<a id="component_registryhas_instances"></a>
<a id="component_registryid_of"></a>
<a id="component_registryinfo"></a>
<a id="component_registryis_registered"></a>
<a id="component_registrymbyname"></a>
<a id="component_registrymcount"></a>
<a id="component_registrymdynamicenums"></a>
<a id="component_registrymdynamictypes"></a>
<a id="component_registrymmutex"></a>
<a id="component_registrymonregistered"></a>
<a id="component_registrymserial"></a>
<a id="component_registrymtypes"></a>
<a id="component_registrymworlds"></a>
<a id="component_registryon_registered"></a>
<a id="component_registryoperator"></a>
<a id="component_registryredefine_dynamic"></a>
<a id="component_registryregister_component"></a>
<a id="component_registryregister_described"></a>
<a id="component_registryregister_dynamic"></a>
<a id="component_registryregister_info"></a>
<a id="component_registryserial"></a>
<a id="component_registrytypes"></a>
<a id="component_registryvalidate_layout"></a>
<a id="component_registryworld"></a>
<a id="component_type_desc"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="dynamic_component_desc"></a>
<a id="dynamic_component_descalignment"></a>
<a id="dynamic_component_descdefaultvalue"></a>
<a id="dynamic_component_descfields"></a>
<a id="dynamic_component_descflags"></a>
<a id="dynamic_component_descname"></a>
<a id="dynamic_component_descsize"></a>
<a id="dynamic_field_desc"></a>
<a id="dynamic_field_descname"></a>
<a id="dynamic_field_descoffset"></a>
<a id="dynamic_field_desctype"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<a id="world"></a>
<!-- doc-legacy-anchors:end -->

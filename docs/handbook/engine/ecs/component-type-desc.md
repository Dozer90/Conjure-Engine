---
kind: header
---

[Engine](../../engine.md) → [ECS](../ecs.md)

# component_type_desc.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/ecs/component_type_desc.h` |
| API ↗ | [Entities and components](../../../api/ecs/entities-and-components.md) |

<p><code>A</code> component type exported by a module.</p>

## Structs

| File | Description |
|---|---|
| [`component_type_desc`](component-type-desc/component-type-desc.md) | <code>A</code> component type exported by a module. |
| [`system_extension_desc`](component-type-desc/system-extension-desc.md) | <code>A</code> system exported by a module. The name must be unique within its phase (prefix it with the module name): the engine&#x27;s consumer refuses the load with <code>REGISTRATION_CONFLICT</code> when the phase already holds a system of that name or the module registers it twice. reads and writes are component names, resolved when the system is added and again whenever the registry gains component types; a name nothing has registered yet is one <code>WARN</code> at attach, not an error, and joins the access set once a component of that name is registered. after and before are system names of the same phase; an unknown one is ignored. |

## Functions

### Public

#### `describe_component` {#function-describe-component}

<div class="function-declaration" id="function-describe-component-1"><pre><code><span class="k">template</span> &lt;<a href="component.html#symbol-named-component">named_component</a> ComponentType&gt; [[<span class="na">nodiscard</span>]] <a href="component-type-desc/component-type-desc.html">component_type_desc</a> describe_component(
    <a href="component.html#symbol-component-flags">component_flags</a> flags = component_flags::NONE) <span class="k">noexcept</span>;</code></pre></div>

<p>The record of a <code>C</code>++ type named with <code><a href="../physics/character-controller.html#symbol-conjure-component">CONJURE_COMPONENT</a></code>: size, alignment, <code>TAG</code>/<code>TRIVIAL</code> flags, the lifecycle hooks and the reflection description (registered in <code>core::reflect</code> on first use, so this is not a constant expression). The serializer stays <code>null</code>: the registry serialises a reflected type through its own deep copy of the description, so no code of the module is reached once it has unloaded. <code>A</code> type without reflection (scripting&#x27;s <code><a href="../scripting/script-component/script-component.html">script_component</a></code>) fills the record by hand and supplies its own serializer hooks.</p>

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
<a id="component_type_desc"></a>
<a id="component_type_descalignment"></a>
<a id="component_type_descextension_point"></a>
<a id="component_type_descflags"></a>
<a id="component_type_descname"></a>
<a id="component_type_descops"></a>
<a id="component_type_descreflection"></a>
<a id="component_type_descserializer"></a>
<a id="component_type_descsize"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="describe_component"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="structs-and-types"></a>
<a id="system_extension_desc"></a>
<a id="system_extension_descafter"></a>
<a id="system_extension_descaftercount"></a>
<a id="system_extension_descbefore"></a>
<a id="system_extension_descbeforecount"></a>
<a id="system_extension_descextension_point"></a>
<a id="system_extension_descflags"></a>
<a id="system_extension_descname"></a>
<a id="system_extension_descphase"></a>
<a id="system_extension_descreadcount"></a>
<a id="system_extension_descreads"></a>
<a id="system_extension_descrun"></a>
<a id="system_extension_descwritecount"></a>
<a id="system_extension_descwrites"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->

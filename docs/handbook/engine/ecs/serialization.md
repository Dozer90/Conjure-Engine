---
kind: header
---

[Engine](../../engine.md) → [ECS](../ecs.md)

# serialization.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/ecs/serialization.h` |
| API ↗ | [Archives and serialisation](../../../api/ecs/archives-and-serialization.md) |

<p>Serialisation.</p>

## Structs

| File | Description |
|---|---|
| [`unresolved_component`](serialization/unresolved-component.md) | One component of an unknown type kept verbatim through a load/save round trip. |
| [`unresolved_components`](serialization/unresolved-components.md) | The built-in component (id 0, <code>EDITOR_HIDDEN</code>, non-trivial) every registry registers so unknown component names on load are preserved and written back unchanged. |

## Forward Declarations

### `component_registry` {#symbol-component-registry}

```cpp
class component_registry;
```

### `entity_archive` {#symbol-entity-archive}

```cpp
class entity_archive;
```

### `world` {#symbol-world}

```cpp
class world;
```

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-UNRESOLVED_COMPONENTS_NAME"></a><code>UNRESOLVED_COMPONENTS_NAME</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code> | <code class="signature"><span class="s">"conjure.ecs.unresolved_<wbr>components"</span></code> | Registered name of the <code><a href="serialization/unresolved-components.html">unresolved_components</a></code> component. |

## Functions

### Public

#### `component_from_json` {#function-component-from-json}

<div class="function-declaration" id="function-component-from-json-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; component_from_json(
    <a href="world/world.html">world</a> &amp;target,
    <a href="entity/entity.html">entity</a> owner,
    <a href="component/component-id.html">component_id</a> id,
    <span class="k">const</span> <a href="../core/json/value/json-value.html">core::json::value</a> &amp;in);</code></pre></div>

<p>Updates the live component <code>id</code> of <code>owner</code> from JSON (missing members keep their value) and stamps its column version. Same lookup errors as <code>component_to_json</code>; <code>SERIALIZATION_FAILED</code> with the field path for malformed input.</p>

#### `component_to_json` {#function-component-to-json}

<div class="function-declaration" id="function-component-to-json-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; component_to_json(
    <span class="k">const</span> <a href="world/world.html">world</a> &amp;source,
    <a href="entity/entity.html">entity</a> target,
    <a href="component/component-id.html">component_id</a> id,
    <a href="../core/json/writer/json-writer.html">core::json::writer</a> &amp;out);</code></pre></div>

<p>Serialises the live component <code>id</code> of <code>target</code> with its serializer (editor inspector and AI protocol). Entity references are written as {&quot;external&quot;:&quot;&lt;index&gt;:&lt;generation&gt;&quot;}. <code>ENTITY_NOT_ALIVE</code> / <code>COMPONENT_NOT_PRESENT</code> / <code>UNKNOWN_COMPONENT</code> when the component cannot be found.</p>

#### `detail::read_reflected_component` {#function-detail-read-reflected-component}

<div class="function-declaration" id="function-detail-read-reflected-component-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; read_reflected_component( <span class="k">const</span> <a href="../core/reflect/type-info/reflect-type-info.html">core::reflect::type_info</a> &amp;type, <span class="k">void</span> *component, <span class="k">const</span> <a href="../core/json/value/json-value.html">core::json::value</a> &amp;in );</code></pre></div>

<p>Reads a reflected component from a JSON object; missing members keep their value. Errors name the field path.</p>

#### `detail::read_typed_component` {#function-detail-read-typed-component}

<div class="function-declaration" id="function-detail-read-typed-component-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> ComponentType&gt; [[<span class="na">nodiscard</span>]] <a href="../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; read_typed_component(<span class="k">void</span> *component, <span class="k">const</span> <a href="../core/json/value/json-value.html">core::json::value</a> &amp;in);</code></pre></div>

#### `detail::read_unresolved_components` {#function-detail-read-unresolved-components}

<div class="function-declaration" id="function-detail-read-unresolved-components-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; read_unresolved_components( <span class="k">void</span> *component, <span class="k">const</span> <a href="../core/json/value/json-value.html">core::json::value</a> &amp;in );</code></pre></div>

#### `detail::remap_reflected_entities` {#function-detail-remap-reflected-entities}

<div class="function-declaration" id="function-detail-remap-reflected-entities-1"><pre><code><span class="na">CONJURE_API</span> <span class="k">void</span> remap_reflected_entities( <span class="k">const</span> <a href="../core/reflect/type-info/reflect-type-info.html">core::reflect::type_info</a> &amp;type, <span class="k">void</span> *component, <span class="k">const</span> <a href="entity/entity-remap.html">entity_remap</a> &amp;map );</code></pre></div>

<p>Rewrites every entity field a reflected type contains through the remap table.</p>

#### `detail::remap_typed_component` {#function-detail-remap-typed-component}

<div class="function-declaration" id="function-detail-remap-typed-component-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> ComponentType&gt; <span class="k">void</span> remap_typed_component(<span class="k">void</span> *component, <span class="k">const</span> <a href="entity/entity-remap.html">entity_remap</a> &amp;map);</code></pre></div>

#### `detail::write_reflected_component` {#function-detail-write-reflected-component}

<div class="function-declaration" id="function-detail-write-reflected-component-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; write_reflected_component( <span class="k">const</span> <a href="../core/reflect/type-info/reflect-type-info.html">core::reflect::type_info</a> &amp;type, <span class="k">const</span> <span class="k">void</span> *component, <a href="../core/json/writer/json-writer.html">core::json::writer</a> &amp;out );</code></pre></div>

<p>Writes a reflected component as a JSON object using the archive encoding for entity and asset reference fields.</p>

#### `detail::write_typed_component` {#function-detail-write-typed-component}

<div class="function-declaration" id="function-detail-write-typed-component-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> ComponentType&gt; [[<span class="na">nodiscard</span>]] <a href="../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; write_typed_component(<span class="k">const</span> <span class="k">void</span> *component, <a href="../core/json/writer/json-writer.html">core::json::writer</a> &amp;out);</code></pre></div>

#### `detail::write_unresolved_components` {#function-detail-write-unresolved-components}

<div class="function-declaration" id="function-detail-write-unresolved-components-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; write_unresolved_components( <span class="k">const</span> <span class="k">void</span> *component, <a href="../core/json/writer/json-writer.html">core::json::writer</a> &amp;out );</code></pre></div>

<p>Serializer hooks of the <code><a href="serialization/unresolved-components.html">unresolved_components</a></code> component: an object mapping each preserved name to its verbatim JSON.</p>

#### `make_default_serializer` {#function-make-default-serializer}

<div class="function-declaration" id="function-make-default-serializer-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> ComponentType&gt; [[<span class="na">nodiscard</span>]] <a href="component/component-serializer.html">component_serializer</a> make_default_serializer() <span class="k">noexcept</span>;</code></pre></div>

<p>The reflection-driven serializer of a <code><a href="../audio/components.html#symbol-conjure-reflect-struct">CONJURE_REFLECT_STRUCT</a></code> component type. Types without a description get hooks that fail with <code>INVALID_STATE</code>.</p>

#### `read_archive_json` {#function-read-archive-json}

<div class="function-declaration" id="function-read-archive-json-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<a href="entity-archive/entity-archive.html">entity_archive</a>&gt; read_archive_json(
    <span class="k">const</span> <a href="../core/json/value/json-value.html">core::json::value</a> &amp;in,
    <span class="k">const</span> <a href="component-registry/component-registry.html">component_registry</a> &amp;registry);</code></pre></div>

<p>Parses an archive written by <code>write_archive_json</code>. Unknown component names are preserved in an <code><a href="serialization/unresolved-components.html">unresolved_components</a></code> component (one <code>INFO</code> log each); malformed component JSON fails the whole load with <code>SERIALIZATION_FAILED</code> and an &quot;&lt;entity index&gt;/&lt;component&gt;/&lt;field&gt;&quot; path; an entity reference outside the archive fails with <code>ARCHIVE_CORRUPT</code>.</p>

#### `read_component_json` {#function-read-component-json}

<div class="function-declaration" id="function-read-component-json-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; read_component_json(
    <span class="k">const</span> <a href="component/component-info.html">component_info</a> &amp;info,
    <span class="k">void</span> *component,
    <span class="k">const</span> <a href="../core/json/value/json-value.html">core::json::value</a> &amp;in);</code></pre></div>

<p>Reads one component instance through the type&#x27;s serializer. <code>SERIALIZATION_FAILED</code> with the failing field path when the JSON does not match the type.</p>

#### `write_archive_json` {#function-write-archive-json}

<div class="function-declaration" id="function-write-archive-json-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; write_archive_json(
    <span class="k">const</span> <a href="entity-archive/entity-archive.html">entity_archive</a> &amp;archive,
    <a href="../core/json/writer/json-writer.html">core::json::writer</a> &amp;out);</code></pre></div>

<p>Emits {&quot;entities&quot;:[{&quot;components&quot;:{&quot;&lt;name&gt;&quot;:{...}}}]} for the archive, entities in archive order and each entity&#x27;s component keys sorted by name (<code><a href="../core/types.html#symbol-byte">byte</a></code> order of the UTF-8 text), so two registries that assigned the ids in different orders write the same text. Unresolved components are re-emitted verbatim under their original names, in the same sorted order, except a preserved entry whose name now resolves to a (non-transient) component the entity holds: it is dropped in favour of that component, so the name is written once. <code>ARCHIVE_CORRUPT</code> for an unregistered component id, or the component serializer&#x27;s error. On failure the writer holds partial, unbalanced output; <code>reset()</code> or discard it before reuse.</p>

#### `write_component_json` {#function-write-component-json}

<div class="function-declaration" id="function-write-component-json-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; write_component_json(
    <span class="k">const</span> <a href="component/component-info.html">component_info</a> &amp;info,
    <span class="k">const</span> <span class="k">void</span> *component,
    <a href="../core/json/writer/json-writer.html">core::json::writer</a> &amp;out);</code></pre></div>

<p>Writes one component instance through the type&#x27;s serializer (custom hooks, else the reflection walker, else the <code><a href="serialization/unresolved-components.html">unresolved_components</a></code> encoding). <code>SERIALIZATION_FAILED</code> when the type cannot be serialised; <code>TRANSIENT</code> types write an empty object.</p>

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
<a id="component_from_json"></a>
<a id="component_registry"></a>
<a id="component_to_json"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="detailread_reflected_component"></a>
<a id="detailread_typed_component"></a>
<a id="detailread_unresolved_components"></a>
<a id="detailremap_reflected_entities"></a>
<a id="detailremap_typed_component"></a>
<a id="detailwrite_reflected_component"></a>
<a id="detailwrite_typed_component"></a>
<a id="detailwrite_unresolved_components"></a>
<a id="direct-header-dependencies"></a>
<a id="entity_archive"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="make_default_serializer"></a>
<a id="read_archive_json"></a>
<a id="read_component_json"></a>
<a id="read_reflected_component"></a>
<a id="read_typed_component"></a>
<a id="read_unresolved_components"></a>
<a id="remap_reflected_entities"></a>
<a id="remap_typed_component"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="unresolved_component"></a>
<a id="unresolved_componentjson"></a>
<a id="unresolved_componentname"></a>
<a id="unresolved_componentoperator"></a>
<a id="unresolved_components"></a>
<a id="unresolved_components_name"></a>
<a id="unresolved_componentsentries"></a>
<a id="unresolved_componentsoperator"></a>
<a id="variables-and-properties"></a>
<a id="world"></a>
<a id="write_archive_json"></a>
<a id="write_component_json"></a>
<a id="write_reflected_component"></a>
<a id="write_typed_component"></a>
<a id="write_unresolved_components"></a>
<!-- doc-legacy-anchors:end -->

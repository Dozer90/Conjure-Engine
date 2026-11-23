---
kind: header
---

[Engine](../../engine.md) → [ECS](../ecs.md)

# component.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/ecs/component.h` |
| API ↗ | [Entities and components](../../../api/ecs/entities-and-components.md) |

<p>Components.</p>

## Structs

| File | Description |
|---|---|
| [`component_id`](component/component-id.md) | Dense index of a component type within one registry. <code>INVALID</code> means &quot;no component&quot;. |
| [`component_info`](component/component-info.md) | Everything the registry knows about one component type. <code>A</code> registered <code><a href="component/component-info.html">component_info</a></code> stays at a stable address for the registry&#x27;s lifetime. |
| [`component_mask::hasher`](component/component-mask-hasher.md) | Hasher for <code>std::unordered_map</code> keys. |
| [`component_ops`](component/component-ops.md) | Lifecycle hooks of a non-trivial component. <code>A</code> <code>null</code> pointer selects the trivial path: zero-fill or the registered default bytes for construction, no-op destruction, memcpy for relocation and copy. |
| [`component_serializer`](component/component-serializer.md) | Serialisation hooks of a component type. Null hooks select the reflection-driven default, which walks <code>component_info::reflection</code> (<code>serialization.h</code>). <code>remapEntities</code> is consulted only when a type stores entity references that reflection cannot see: <code>entity_archive::capture</code> calls it with a table whose targets are the captured entities (real -&gt; archive-local) and <code>world::instantiate</code> with the table of the created copy (archive-local -&gt; real). <code>A</code> hook rewrites each reference through <code>entity_remap::remap</code>, which serves both directions; references to entities outside the archive are left as they are (<code>archive_options::keepExternalReferences</code> only governs reflected fields). |
| [`component_type_cache`](component/component-type-cache.md) | The per-type lookup cache behind <code>component_registry::id_of</code>: one atomic word packing (registrySerial &lt;&lt; 16) \| id. <code>A</code> serial mismatch means &quot;look the name up again&quot;. The static lives per shared library but only caches a lookup result, so every library agrees. |

## Classes

| File | Description |
|---|---|
| [`component_mask`](component/component-mask.md) | <code>A</code> set of component ids: one bit per id. Archetype matching is a handful of 64-bit operations. |

## Enumerations

### `component_flags` {#symbol-component-flags}

```cpp
enum class component_flags : u32 {
    NONE = 0,
    TAG = 1 << 0,              // size 0, lives only in the mask
    TRIVIAL = 1 << 1,          // memcpy relocate/copy, no destructor
    DYNAMIC = 1 << 2,          // registered at runtime (C#), fields described by data
    TRANSIENT = 1 << 3,        // never serialised, no reflection required
    EDITOR_HIDDEN = 1 << 4     // internal types (unresolved_components); area H calls this INTERNAL
};
```

Properties of a component type.

## Concepts

### `named_component` {#symbol-named-component}

```cpp
template <typename ComponentType> concept named_component = requires { { component_name<std::remove_cvref_t<ComponentType>>::VALUE } -> std::convertible_to<std::string_view>; };
```

True when <code><a href="../physics/character-controller.html#symbol-conjure-component">CONJURE_COMPONENT</a></code> named the type.

## Forward Declarations

### `component_name` {#symbol-component-name}

```cpp
template <typename ComponentType> struct component_name;
```

The registered name of a <code>C</code>++ component type. Specialised by <code><a href="../physics/character-controller.html#symbol-conjure-component">CONJURE_COMPONENT</a></code>; the name&#x27;s <code><a href="../core/string-id/string-id.html">string_id</a></code> is the registry key, so the same name yields the same id in every registry.

### `json::value` {#symbol-json-value}

```cpp
class value;
```

### `json::writer` {#symbol-json-writer}

```cpp
class writer;
```

### `reflect::type_info` {#symbol-reflect-type-info}

```cpp
struct type_info;
```

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-MAX_COMPONENT_ALIGNMENT"></a><code>MAX_COMPONENT_ALIGNMENT</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">64</span></code> | Strictest component alignment accepted. |
| <a id="variable-MAX_COMPONENT_SIZE"></a><code>MAX_COMPONENT_SIZE</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">4096</span></code> | Largest component in bytes; larger data belongs behind an asset handle. |
| <a id="variable-MAX_COMPONENTS"></a><code>MAX_COMPONENTS</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">1024</span></code> | Most component types one registry can hold. |

## Macros

### `CONJURE_ENUM_FLAGS` {#symbol-conjure-enum-flags}

```cpp
CONJURE_ENUM_FLAGS(component_flags)
```

## Functions

### Public

#### `default_component_bytes` {#function-default-component-bytes}

<div class="function-declaration" id="function-default-component-bytes-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> ComponentType&gt; [[<span class="na">nodiscard</span>]] std::vector&lt;std::byte&gt; default_component_bytes();</code></pre></div>

<p>The default bytes of a trivially copyable type with a non-trivial default constructor (member initialisers), constructed into zeroed storage so padding stays zero. Empty when zero-fill is already the default or the type is not trivially copyable.</p>

#### `derive_component_flags` {#function-derive-component-flags}

<div class="function-declaration" id="function-derive-component-flags-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> ComponentType&gt; [[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="component.html#symbol-component-flags">component_flags</a> derive_component_flags(
    <a href="component.html#symbol-component-flags">component_flags</a> extra) <span class="k">noexcept</span>;</code></pre></div>

<p>The flags a <code>C</code>++ type implies: <code>TAG</code> for empty types, <code>TRIVIAL</code> for trivially copyable ones, combined with the caller&#x27;s extra flags.</p>

#### `detail::construct_default_components` {#function-detail-construct-default-components}

<div class="function-declaration" id="function-detail-construct-default-components-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> ComponentType&gt; <span class="k">void</span> construct_default_components(<span class="k">void</span> *destination, <a href="../core/types.html#symbol-u32">u32</a> count);</code></pre></div>

#### `detail::copy_components` {#function-detail-copy-components}

<div class="function-declaration" id="function-detail-copy-components-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> ComponentType&gt; <span class="k">void</span> copy_components(<span class="k">void</span> *destination, <span class="k">const</span> <span class="k">void</span> *source, <a href="../core/types.html#symbol-u32">u32</a> count);</code></pre></div>

#### `detail::destruct_components` {#function-detail-destruct-components}

<div class="function-declaration" id="function-detail-destruct-components-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> ComponentType&gt; <span class="k">void</span> destruct_components(<span class="k">void</span> *object, <a href="../core/types.html#symbol-u32">u32</a> count);</code></pre></div>

#### `detail::relocate_components` {#function-detail-relocate-components}

<div class="function-declaration" id="function-detail-relocate-components-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> ComponentType&gt; <span class="k">void</span> relocate_components(<span class="k">void</span> *destination, <span class="k">void</span> *source, <a href="../core/types.html#symbol-u32">u32</a> count);</code></pre></div>

#### `make_component_ops` {#function-make-component-ops}

<div class="function-declaration" id="function-make-component-ops-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> ComponentType&gt; [[<span class="na">nodiscard</span>]] <a href="component/component-ops.html">component_ops</a> make_component_ops() <span class="k">noexcept</span>;</code></pre></div>

<p>The lifecycle hooks of a <code>C</code>++ type: all <code>null</code> for trivially copyable types (the memcpy path), otherwise the type&#x27;s special members.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="component_flags"></a>
<a id="component_flagsdynamic"></a>
<a id="component_flagseditor_hidden"></a>
<a id="component_flagsnone"></a>
<a id="component_flagstag"></a>
<a id="component_flagstransient"></a>
<a id="component_flagstrivial"></a>
<a id="component_id"></a>
<a id="component_idinvalid"></a>
<a id="component_idis_valid"></a>
<a id="component_idoperator"></a>
<a id="component_idvalue"></a>
<a id="component_info"></a>
<a id="component_infoalignment"></a>
<a id="component_infoassetreffieldoffsets"></a>
<a id="component_infodefaultvalue"></a>
<a id="component_infoentityfieldoffsets"></a>
<a id="component_infoflags"></a>
<a id="component_infoid"></a>
<a id="component_infois_tag"></a>
<a id="component_infois_transient"></a>
<a id="component_infois_trivial"></a>
<a id="component_infoname"></a>
<a id="component_infoops"></a>
<a id="component_inforeflection"></a>
<a id="component_infoserializer"></a>
<a id="component_infosize"></a>
<a id="component_mask"></a>
<a id="component_maskclear"></a>
<a id="component_maskcomponent_mask"></a>
<a id="component_maskcontains_all"></a>
<a id="component_maskcontains_any"></a>
<a id="component_maskcount"></a>
<a id="component_maskfor_each_set"></a>
<a id="component_maskhash"></a>
<a id="component_maskhasher"></a>
<a id="component_maskhasheroperator"></a>
<a id="component_maskmwords"></a>
<a id="component_masknone"></a>
<a id="component_maskoperator"></a>
<a id="component_maskoperator_1"></a>
<a id="component_maskoperator_2"></a>
<a id="component_maskoperator_3"></a>
<a id="component_maskoperator_4"></a>
<a id="component_maskoperator_5"></a>
<a id="component_maskset"></a>
<a id="component_masktest"></a>
<a id="component_maskword_count"></a>
<a id="component_maskwords"></a>
<a id="component_name"></a>
<a id="component_ops"></a>
<a id="component_opsconstructdefault"></a>
<a id="component_opscopy"></a>
<a id="component_opsdestruct"></a>
<a id="component_opsrelocate"></a>
<a id="component_serializer"></a>
<a id="component_serializerreadjson"></a>
<a id="component_serializerremapentities"></a>
<a id="component_serializerwritejson"></a>
<a id="component_type_cache"></a>
<a id="component_type_cacheslot"></a>
<a id="conjure_enum_flags"></a>
<a id="construct_default_components"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="copy_components"></a>
<a id="declaration-summaries"></a>
<a id="default_component_bytes"></a>
<a id="dependencies"></a>
<a id="derive_component_flags"></a>
<a id="destruct_components"></a>
<a id="detailconstruct_default_components"></a>
<a id="detailcopy_components"></a>
<a id="detaildestruct_components"></a>
<a id="detailrelocate_components"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="jsonvalue"></a>
<a id="jsonwriter"></a>
<a id="make_component_ops"></a>
<a id="max_component_alignment"></a>
<a id="max_component_size"></a>
<a id="max_components"></a>
<a id="named_component"></a>
<a id="other-declarations"></a>
<a id="reflecttype_info"></a>
<a id="relocate_components"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="type_info"></a>
<a id="value"></a>
<a id="variables-and-properties"></a>
<a id="writer"></a>
<!-- doc-legacy-anchors:end -->

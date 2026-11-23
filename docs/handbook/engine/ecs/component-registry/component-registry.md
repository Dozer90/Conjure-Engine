---
kind: class
---

[Engine](../../../engine.md) → [ECS](../../ecs.md) → [component_registry.h](../component-registry.md)

# component_registry
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::ecs` |
| Include File Path | `engine/include/conjure/ecs/component_registry.h` |
| Source File Path | `engine/src/ecs/component_registry.cpp` |
| API ↗ | [Entities and components](../../../../api/ecs/entities-and-components.md), [Entities, components and systems](../../../../api/ecs/index.md) |

## Syntax

```cpp
class CONJURE_API component_registry
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">component_registry();</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted. Move constructors have been deleted. Move assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~component_registry();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mByName</code></td><td><code class="signature">std::<wbr>unordered_<wbr>map&lt;<wbr>core::<wbr>string_<wbr>id,<wbr> <a href="../component/component-id.html">component_<wbr>id</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mCount</code></td><td><code class="signature">std::atomic&lt;<a href="../../core/types.html#symbol-u32">u32</a>&gt;</code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Dynamic<wbr>Enums</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr>std::<wbr>unique_<wbr>ptr&lt;<wbr><a href="../../core/reflect/field-info/reflect-enum-info.html">core::<wbr>reflect::<wbr>enum_<wbr>info</a>&gt;&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Dynamic<wbr>Types</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr>std::<wbr>unique_<wbr>ptr&lt;<wbr><a href="../../core/reflect/type-info/reflect-type-info.html">core::<wbr>reflect::<wbr>type_<wbr>info</a>&gt;&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mMutex</code></td><td><code class="signature"><span class="k">mutable</span> std::mutex</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>On<wbr>Registered</code></td><td><code class="signature"><a href="../../core/event/event.html">core::<wbr>event</a>&lt;<wbr><span class="k">const</span> <a href="../component/component-info.html">component_<wbr>info</a> &amp;&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mSerial</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mTypes</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="../component/component-info.html">component_<wbr>info</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mWorlds</code></td><td><code class="signature"><span class="k">mutable</span> std::<wbr>vector&lt;<wbr><a href="../world/world.html">world</a> *&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `count` {#function-count}

<div class="function-declaration" id="function-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Number of registered types, <code><a href="../serialization/unresolved-components.html">unresolved_components</a></code> included. Lock-free.</p>

#### `find` {#function-find}

<div class="function-declaration" id="function-find-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../component/component-id.html">component_id</a> find(
    <a href="../../core/string-id/string-id.html">core::string_id</a> name) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The id registered under <code>name</code>, or <code>INVALID</code>.</p>

#### `has_instances` {#function-has-instances}

<div class="function-declaration" id="function-has-instances-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> has_instances(
    <a href="../component/component-id.html">component_id</a> id) <span class="k">const</span>;</code></pre></div>

<p>True when any live world created on this registry holds an instance of <code>id</code>.</p>

#### `id_of` {#function-id-of}

<div class="function-declaration" id="function-id-of-1"><pre><code><span class="k">template</span> &lt;<a href="../component.html#symbol-named-component">named_component</a> ComponentType&gt; [[<span class="na">nodiscard</span>]] <a href="../component/component-id.html">component_id</a> id_of() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The id of a <code>C</code>++ type, <code>INVALID</code> when it is not registered here. Cached per type and registry serial, so the lookup costs one atomic load after the first call.</p>

#### `info` {#function-info}

<div class="function-declaration" id="function-info-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../component/component-info.html">component_info</a> &amp;info(
    <a href="../component/component-id.html">component_id</a> id) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The description of a registered id. Asserts and returns the <code><a href="../serialization/unresolved-components.html">unresolved_components</a></code> description for an id that is not registered. Lock-free.</p>

#### `is_registered` {#function-is-registered}

<div class="function-declaration" id="function-is-registered-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_registered(
    <a href="../component/component-id.html">component_id</a> id) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when <code>id</code> names a registered type. Lock-free.</p>

#### `on_registered` {#function-on-registered}

<div class="function-declaration" id="function-on-registered-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/event/event.html">core::event</a>&lt;<span class="k">const</span> <a href="../component/component-info.html">component_info</a> &amp;&gt; &amp;on_registered() <span class="k">noexcept</span>;</code></pre></div>

<p>Fires once per new registration, on the registering thread, after the registry lock is released.</p>

#### `redefine_dynamic` {#function-redefine-dynamic}

<div class="function-declaration" id="function-redefine-dynamic-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; redefine_dynamic(
    <a href="../component/component-id.html">component_id</a> id,
    <span class="k">const</span> <a href="dynamic-component-desc.html">dynamic_component_desc</a> &amp;desc);</code></pre></div>

<p>Replaces the layout of a dynamic type while keeping its id (hot reload). <code>COMPONENT_IN_USE</code> while any live world holds an instance, <code>UNKNOWN_COMPONENT</code> for an id that is not dynamic, <code>NAME_CONFLICT</code> when the new description carries a different name.</p>

#### `register_component` {#function-register-component}

<div class="function-declaration" id="function-register-component-1"><pre><code><span class="k">template</span> &lt;<a href="../component.html#symbol-named-component">named_component</a> ComponentType&gt; [[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="../component/component-id.html">component_id</a>&gt; register_component(
    <a href="../component.html#symbol-component-flags">component_flags</a> flags = component_flags::NONE);</code></pre></div>

<p>Registers a <code>C</code>++ type named with <code><a href="../../physics/character-controller.html#symbol-conjure-component">CONJURE_COMPONENT</a></code>. Size, alignment, <code>TAG</code>/<code>TRIVIAL</code> flags and the lifecycle hooks come from the type, the reflection description from <code>core::reflect::type_info_of</code> (required unless the type is <code>TRANSIENT</code> or a tag), the entity and asset-reference field offsets and the default serializer from the description. Re-registering a name with the same layout and flags returns the existing id; <code>NAME_CONFLICT</code> otherwise. <code>COMPONENT_LIMIT_REACHED</code>, <code>COMPONENT_TOO_LARGE</code>, <code>BAD_ALIGNMENT</code> and <code>UNSUPPORTED_FIELD</code> as documented in.</p>

#### `register_described` {#function-register-described}

<div class="function-declaration" id="function-register-described-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="../component/component-id.html">component_id</a>&gt; register_described(
    <span class="k">const</span> <a href="../component-type-desc/component-type-desc.html">component_type_desc</a> &amp;desc);</code></pre></div>

<p>Registers a type from a module extension record (<code><a href="../component-type-desc/component-type-desc.html">component_type_desc</a>.h</code>). The reflection description is deep-copied into the registry, nested struct and enum descriptions included, so the module may unload. The record&#x27;s serializer hooks are copied as given: <code>null</code> for a reflected type (<code>describe_component</code> leaves them <code>null</code> and the registry&#x27;s reflection walker serialises the deep copy, so nothing points into the module), while a type without reflection must supply writeJson and readJson (<code>UNSUPPORTED_FIELD</code> otherwise). <code>A</code> record whose size and <code>TAG</code> flag disagree (size 0 without <code>TAG</code>, or <code>TAG</code> with a non-zero size) is <code>INVALID_ARGUMENT</code>.</p>

#### `register_dynamic` {#function-register-dynamic}

<div class="function-declaration" id="function-register-dynamic-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="../component/component-id.html">component_id</a>&gt; register_dynamic(
    <span class="k">const</span> <a href="dynamic-component-desc.html">dynamic_component_desc</a> &amp;desc);</code></pre></div>

<p>Registers a runtime-described type. Builds a reflection description from the fields, forces <code>DYNAMIC</code> | <code>TRIVIAL</code> and accepts an alignment in {1, 2, 4, 8, 16}. Same conflict and limit rules as <code>register_component</code>; for a dynamic type the fields are part of the layout, so re-registering a known name whose fields differ (order, name, type or offset) is <code>NAME_CONFLICT</code> even with the same size and alignment. <code>redefine_dynamic</code> replaces the description of such a type.</p>

#### `serial` {#function-serial}

<div class="function-declaration" id="function-serial-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> serial() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The process-unique serial of this registry (never 0).</p>

#### `types` {#function-types}

<div class="function-declaration" id="function-types-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> <a href="../component/component-info.html">component_info</a>&gt; types() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Every registered type in id order (editor and test enumeration). The view stays valid for the registry&#x27;s lifetime; it grows with later registrations.</p>

### Private

#### `attach_world` {#function-attach-world}

<div class="function-declaration" id="function-attach-world-1"><pre><code><span class="k">void</span> attach_world(
    <a href="../world/world.html">world</a> *target) <span class="k">const</span>;</code></pre></div>

#### `build_dynamic_type` {#function-build-dynamic-type}

<div class="function-declaration" id="function-build-dynamic-type-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;std::unique_ptr&lt;<a href="../../core/reflect/type-info/reflect-type-info.html">core::reflect::type_info</a>&gt;&gt; build_dynamic_type(
    <span class="k">const</span> <a href="dynamic-component-desc.html">dynamic_component_desc</a> &amp;desc) <span class="k">const</span>;</code></pre></div>

#### `collect_field_offsets` {#function-collect-field-offsets}

<div class="function-declaration" id="function-collect-field-offsets-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; collect_field_offsets(
    <a href="../component/component-info.html">component_info</a> &amp;info) <span class="k">const</span>;</code></pre></div>

#### `detach_world` {#function-detach-world}

<div class="function-declaration" id="function-detach-world-1"><pre><code><span class="k">void</span> detach_world(
    <a href="../world/world.html">world</a> *target) <span class="k">const</span>;</code></pre></div>

#### `register_info` {#function-register-info}

<div class="function-declaration" id="function-register-info-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="../component/component-id.html">component_id</a>&gt; register_info(
    <a href="../component/component-info.html">component_info</a> info);</code></pre></div>

#### `validate_layout` {#function-validate-layout}

<div class="function-declaration" id="function-validate-layout-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; validate_layout(
    <span class="k">const</span> <a href="../component/component-info.html">component_info</a> &amp;info) <span class="k">const</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

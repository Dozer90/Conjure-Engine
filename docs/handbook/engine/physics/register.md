---
kind: header
---

[Engine](../../engine.md) → [Physics](../physics.md)

# register.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/physics/register.h` |
| API ↗ | [Bodies, colliders and characters](../../../api/physics/bodies-and-colliders.md), [Physics](../../../api/physics/index.md) |

<p>Registration entry points the engine&#x27;s <code>physics_subsystem</code> calls: the eight component types (startup step 12, after <code>scene::register_components</code>), the collision mesh asset type with its importer, and the four systems.</p>

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-SYSTEM_CONTACT_EVENTS"></a><code>SYSTEM_CONTACT_EVENTS</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/string-id/string-id.html">core::<wbr>string_<wbr>id</a></code> | <code class="signature">{<span class="s">"physics.contact_<wbr>events"</span>}</code> |  |
| <a id="variable-SYSTEM_PULL_TRANSFORMS"></a><code>SYSTEM_PULL_TRANSFORMS</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/string-id/string-id.html">core::<wbr>string_<wbr>id</a></code> | <code class="signature">{<span class="s">"physics.pull_<wbr>transforms"</span>}</code> |  |
| <a id="variable-SYSTEM_PUSH_TRANSFORMS"></a><code>SYSTEM_PUSH_TRANSFORMS</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/string-id/string-id.html">core::<wbr>string_<wbr>id</a></code> | <code class="signature">{<span class="s">"physics.push_<wbr>transforms"</span>}</code> |  |
| <a id="variable-SYSTEM_STEP"></a><code>SYSTEM_STEP</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/string-id/string-id.html">core::<wbr>string_<wbr>id</a></code> | <code class="signature">{<span class="s">"physics.step"</span>}</code> |  |

## Functions

### Public

#### `register_asset_types` {#function-register-asset-types}

<div class="function-declaration" id="function-register-asset-types-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; register_asset_types(
    <a href="../assets/asset-type-registry/asset-type-registry.html">assets::asset_type_registry</a> &amp;types,
    <a href="../assets/asset-importer/importer-registry.html">assets::importer_registry</a> &amp;importers);</code></pre></div>

<p>Registers <code>conjure.physics.collision_mesh</code> (version 1) with its CPU-only loader and its importer, which claims .obj by extension and also reads a .gltf or .glb whose .meta names it.</p>

#### `register_components` {#function-register-components}

<div class="function-declaration" id="function-register-components-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; register_components(
    <a href="../ecs/component-registry/component-registry.html">ecs::component_registry</a> &amp;registry);</code></pre></div>

<p>Registers <code><a href="components/rigid-body.html">rigid_body</a></code>, the five colliders, <code><a href="components/physics-material.html">physics_material</a></code> and <code><a href="character-controller/character-controller.html">character_controller</a></code> in a fixed order. Idempotent per registry; errors are the registry&#x27;s (<code>NAME_CONFLICT</code>, ...).</p>

#### `register_systems` {#function-register-systems}

<div class="function-declaration" id="function-register-systems-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; register_systems(
    <a href="../ecs/scheduler/scheduler.html">ecs::scheduler</a> &amp;scheduler,
    <a href="physics-world/physics-world.html">physics_world</a> &amp;world);</code></pre></div>

<p>Adds <code>physics.push_transforms</code> and <code>physics.step</code> (<code>PHYSICS_STEP</code>, <code>MAIN_THREAD</code>) and <code>physics.pull_transforms</code> (<code>FIXED_END</code>, PARALLEL) and <code>physics.contact_events</code> (<code>FIXED_END</code>, <code>MAIN_THREAD</code>). None runs while paused. <code>NOT_ATTACHED</code> before <code>physics_world::attach</code>. The world must outlive the scheduler. Call once per scheduler: a second call returns <code>NAME_CONFLICT</code> from its first system and adds nothing.</p>

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
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="register_asset_types"></a>
<a id="register_components"></a>
<a id="register_systems"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="system_contact_events"></a>
<a id="system_pull_transforms"></a>
<a id="system_push_transforms"></a>
<a id="system_step"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->

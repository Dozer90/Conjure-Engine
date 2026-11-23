---
kind: header
---

[Engine](../../engine.md) → [Scene](../scene.md)

# register.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/scene/register.h` |
| API ↗ | [Scenes, hierarchy and prefabs](../../../api/scene/index.md), [Scenes and the hierarchy](../../../api/scene/scenes-and-hierarchy.md) |

<p>Registration entry points the engine&#x27;s <code>scene_subsystem</code> calls: the component types (startup step 10), the two asset types with their importers, and the four systems.</p>

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-PREFAB_IMPORTER_ID"></a><code>PREFAB_IMPORTER_ID</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code> | <code class="signature"><span class="s">"conjure.scene.prefab_<wbr>importer"</span></code> |  |
| <a id="variable-SCENE_IMPORTER_ID"></a><code>SCENE_IMPORTER_ID</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code> | <code class="signature"><span class="s">"conjure.scene.scene_<wbr>importer"</span></code> |  |
| <a id="variable-SYSTEM_APPLY_HIERARCHY_CHANGES"></a><code>SYSTEM_APPLY_HIERARCHY_CHANGES</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code> | <code class="signature"><span class="s">"scene.apply_<wbr>hierarchy_<wbr>changes"</span></code> |  |
| <a id="variable-SYSTEM_APPLY_PENDING_LOADS"></a><code>SYSTEM_APPLY_PENDING_LOADS</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code> | <code class="signature"><span class="s">"scene.apply_<wbr>pending_<wbr>loads"</span></code> |  |
| <a id="variable-SYSTEM_DESTROY_PENDING"></a><code>SYSTEM_DESTROY_PENDING</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code> | <code class="signature"><span class="s">"scene.destroy_pending"</span></code> |  |
| <a id="variable-SYSTEM_PROPAGATE_TRANSFORMS"></a><code>SYSTEM_PROPAGATE_TRANSFORMS</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code> | <code class="signature"><span class="s">"scene.propagate_<wbr>transforms"</span></code> |  |

## Functions

### Public

#### `register_asset_types` {#function-register-asset-types}

<div class="function-declaration" id="function-register-asset-types-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; register_asset_types(
    <a href="../assets/asset-type-registry/asset-type-registry.html">assets::asset_type_registry</a> &amp;types,
    <a href="../assets/asset-importer/importer-registry.html">assets::importer_registry</a> &amp;importers,
    <span class="k">const</span> <a href="../ecs/component-registry/component-registry.html">ecs::component_registry</a> &amp;registry);</code></pre></div>

<p>Registers <code>conjure.scene.scene</code> and <code>conjure.scene.prefab</code> (version 1) with loaders that parse on the worker against <code>registry</code>, and the .<code>scene.json</code> / .<code>prefab.json</code> importers.</p>

#### `register_components` {#function-register-components}

<div class="function-declaration" id="function-register-components-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; register_components(
    <a href="../ecs/component-registry/component-registry.html">ecs::component_registry</a> &amp;registry);</code></pre></div>

<p>Registers every scene component in a fixed order (idempotent per registry) and verifies the <code><a href="../assets/asset-ref/asset-ref.html">asset_ref</a></code> layout contract of the bind pass. Errors are the registry&#x27;s (<code>NAME_CONFLICT</code>, ...).</p>

#### `register_systems` {#function-register-systems}

<div class="function-declaration" id="function-register-systems-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; register_systems(
    <a href="../ecs/scheduler/scheduler.html">ecs::scheduler</a> &amp;scheduler,
    <a href="scene-manager/scene-manager.html">scene_manager</a> &amp;scenes);</code></pre></div>

<p>Adds <code>scene.apply_pending_loads</code> (<code>FRAME_BEGIN</code>), <code>scene.apply_hierarchy_changes</code> and <code>scene.propagate_transforms</code> (<code>TRANSFORM</code>) and <code>scene.destroy_pending</code> (<code>FRAME_END</code>, after <code>scripting.frame_end</code>), all <code>RUNS_WHILE_PAUSED</code>. The manager must outlive the scheduler. Call once per scheduler: a second call returns <code>NAME_CONFLICT</code> from its first system and adds nothing.</p>

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
<a id="prefab_importer_id"></a>
<a id="register_asset_types"></a>
<a id="register_components"></a>
<a id="register_systems"></a>
<a id="scene_importer_id"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="system_apply_hierarchy_changes"></a>
<a id="system_apply_pending_loads"></a>
<a id="system_destroy_pending"></a>
<a id="system_propagate_transforms"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->

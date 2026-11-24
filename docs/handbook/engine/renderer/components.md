---
kind: header
---

[Engine](../../engine.md) → [Renderer](../renderer.md)

# components.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/renderer/components.h` |
| API ↗ | [Rendering](../../../api/renderer/index.md), [Rendering a frame](../../../api/renderer/rendering-a-frame.md) |

<p>Component data for the ECS. Plain reflected structs; this module never includes ecs. The engine&#x27;s <code>renderer_subsystem</code> registers the mesh renderer, sprite, camera, light and environment components, and its extraction system maps them 1:1 onto the <code><a href="render-scene/render-scene.html">render_scene</a></code> structs, resolving asset references through <code>renderer::find_</code>*. The two rendering-settings components (<code><a href="components/render-settings-component.html">render_settings_component</a></code>, <code><a href="components/post-process-component.html">post_process_component</a></code>) are resolved by the pure functions below, which any caller with ECS access can use.</p>

## Structs

| File | Description |
|---|---|
| [`camera_component`](components/camera-component.md) |  |
| [`environment_component`](components/environment-component.md) |  |
| [`light_component`](components/light-component.md) |  |
| [`mesh_renderer_component`](components/mesh-renderer-component.md) |  |
| [`post_process_component`](components/post-process-component.md) | The camera layer of the rendering settings, for the entity of a <code><a href="components/camera-component.html">camera_component</a></code> (56 bytes): each override flag replaces that whole group for the camera&#x27;s view (<code>resolve_camera_settings</code>, <code>make_camera_view</code>). Shadows are shared by every view and are not overridable here. |
| [`render_settings_component`](components/render-settings-component.md) | The scene layer of the rendering settings, for any entity (88 bytes). Each override flag makes this component a candidate for that whole group; per group, the highest priority among the candidates wins (<code>choose_scene_settings</code>). <code>A</code> component with no flag set takes part in no group. |
| [`resolved_render_settings`](components/resolved-render-settings.md) | Rendering settings with where each group came from, indexed by <code><a href="render-settings.html#symbol-render-settings-group">render_settings_group</a></code>. |
| [`scene_settings_candidate`](components/scene-settings-candidate.md) | One entity carrying a <code><a href="components/render-settings-component.html">render_settings_component</a></code>, as the caller gathered it. The caller leaves out disabled and inactive entities. |
| [`scene_settings_choice`](components/scene-settings-choice.md) | The winner of every group, indexed by <code><a href="render-settings.html#symbol-render-settings-group">render_settings_group</a></code>. |
| [`sprite_component`](components/sprite-component.md) |  |

## Enumerations

### `render_settings_source` {#symbol-render-settings-source}

```cpp
enum class render_settings_source : u8 {
    PROJECT = 0,
    SCENE = 1,
    CAMERA = 2
};
```

The layer a resolved group came from.

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-CAMERA_COMPONENT_TYPE_NAME"></a><code>CAMERA_COMPONENT_TYPE_NAME</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code> | <code class="signature"><span class="s">"conjure.renderer.camera"</span></code> |  |
| <a id="variable-ENVIRONMENT_COMPONENT_TYPE_NAME"></a><code>ENVIRONMENT_COMPONENT_TYPE_NAME</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code> | <code class="signature"><span class="s">"conjure.renderer.environment"</span></code> |  |
| <a id="variable-LIGHT_COMPONENT_TYPE_NAME"></a><code>LIGHT_COMPONENT_TYPE_NAME</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code> | <code class="signature"><span class="s">"conjure.renderer.light"</span></code> |  |
| <a id="variable-MESH_RENDERER_COMPONENT_TYPE_NAME"></a><code>MESH_RENDERER_COMPONENT_TYPE_NAME</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code> | <code class="signature"><span class="s">"conjure.renderer.mesh_<wbr>renderer"</span></code> |  |
| <a id="variable-NO_SETTINGS_CANDIDATE"></a><code>NO_SETTINGS_CANDIDATE</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">0</span>xFFFFFFFFu</code> |  |
| <a id="variable-POST_PROCESS_COMPONENT_TYPE_NAME"></a><code>POST_PROCESS_COMPONENT_TYPE_NAME</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code> | <code class="signature"><span class="s">"conjure.renderer.post_<wbr>process"</span></code> |  |
| <a id="variable-RENDER_SETTINGS_COMPONENT_TYPE_NAME"></a><code>RENDER_SETTINGS_COMPONENT_TYPE_NAME</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code> | <code class="signature"><span class="s">"conjure.renderer.render_<wbr>settings"</span></code> |  |
| <a id="variable-SETTINGS_GROUP_COUNT"></a><code>SETTINGS_GROUP_COUNT</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-usize">usize</a></code> | <code class="signature">static_<wbr>cast&lt;<wbr><a href="../core/types.html#symbol-usize">usize</a>&gt;(render_<wbr>settings_<wbr>group::<wbr>COUNT)</code> |  |
| <a id="variable-SPRITE_COMPONENT_TYPE_NAME"></a><code>SPRITE_COMPONENT_TYPE_NAME</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code> | <code class="signature"><span class="s">"conjure.renderer.sprite"</span></code> |  |

## Macros

### `CONJURE_REFLECT_ENUM` {#symbol-conjure-reflect-enum}

```cpp
CONJURE_REFLECT_ENUM(conjure::renderer::projection_type, "conjure.renderer.projection_type", PERSPECTIVE, ORTHOGRAPHIC)
```

### `CONJURE_REFLECT_STRUCT` {#symbol-conjure-reflect-struct}

```cpp
CONJURE_REFLECT_STRUCT(conjure::renderer::camera, "conjure.renderer.camera_data", CONJURE_FIELD(projection), CONJURE_FIELD(verticalFovRadians, .min = 0.01, .max = 3.1, .unit = conjure::core::reflect::field_unit::RADIANS), CONJURE_FIELD(orthographicHeight, .min = 0.001), CONJURE_FIELD(nearPlane, .min = 0.0001), CONJURE_FIELD(farPlane, .min = 0.001))
```

## Functions

### Public

#### `choose_scene_settings` {#function-choose-scene-settings}

<div class="function-declaration" id="function-choose-scene-settings-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="components/scene-settings-choice.html">scene_settings_choice</a> choose_scene_settings(
    std::span&lt;<span class="k">const</span> <a href="components/scene-settings-candidate.html">scene_settings_candidate</a>&gt; candidates) <span class="k">noexcept</span>;</code></pre></div>

<p>The scene winner rule, in one pass over the candidates for every group: per group only the candidates whose override flag for it is set compete, the highest priority wins, a tie goes to the lowest guid, with candidates without a guid after every candidate that has one, and then to the lowest entity index. <code>A</code> group is ambiguous when two or more of its candidates share its top priority. <code>A</code> group without candidates has no winner (<code>NO_SETTINGS_CANDIDATE</code>). Winners are indices into <code>candidates</code>.</p>

#### `make_camera_view` {#function-make-camera-view}

<div class="function-declaration" id="function-make-camera-view-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="render-scene/render-view.html">render_view</a> make_camera_view(
    <span class="k">const</span> <a href="components/camera-component.html">camera_component</a> &amp;camera,
    <span class="k">const</span> <a href="../core/math/glm-types.html#symbol-mat4">math::mat4</a> &amp;worldFromCamera,
    <a href="gpu-ids.html#symbol-render-target-id">render_target_id</a> target = {},
    <span class="k">const</span> <a href="components/post-process-component.html">post_process_component</a> *overrides = nullptr) <span class="k">noexcept</span>;</code></pre></div>

<p>The view a camera component at a world matrix renders: the view matrix is the rigid inverse of the matrix (its scale is dropped, so a scaled parent does not warp the projection), with the camera&#x27;s position, camera data and layer mask, <code>NO_POST_PROCESS</code> unless <code>postProcess</code>, into <code>target</code> (invalid = the swapchain). With <code>overrides</code> (the camera entity&#x27;s <code><a href="components/post-process-component.html">post_process_component</a></code>), each group whose override flag is set is copied into the view and named in settingsOverride; the other groups keep their defaults and are not named. The engine&#x27;s extraction builds the scene cameras&#x27; views with it and the editor&#x27;s screenshots through a scene camera do too.</p>

#### `register_components` {#function-register-components}

<div class="function-declaration" id="function-register-components-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; register_components(
    <a href="../core/reflect/type-registry/reflect-type-registry.html">core::reflect::type_registry</a> &amp;registry);</code></pre></div>

<p>Registers the enums, the nested camera, light and settings structs (<code><a href="render-settings/render-settings.html">render_settings</a></code> as <code>conjure.renderer.render_settings_data</code>) and the seven components with a reflection registry, nested types first. Registering twice is harmless (<code>ALREADY_EXISTS</code> is skipped); any other registry error is returned.</p>

#### `resolve_camera_settings` {#function-resolve-camera-settings}

<div class="function-declaration" id="function-resolve-camera-settings-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="components/resolved-render-settings.html">resolved_render_settings</a> resolve_camera_settings(
    <span class="k">const</span> <a href="components/resolved-render-settings.html">resolved_render_settings</a> &amp;scene,
    <span class="k">const</span> <a href="components/camera-component.html">camera_component</a> &amp;camera,
    <span class="k">const</span> <a href="components/post-process-component.html">post_process_component</a> *overrides) <span class="k">noexcept</span>;</code></pre></div>

<p>The scene resolution with the camera&#x27;s override groups applied (source <code>CAMERA</code>; <code>overrides</code> may be <code>null</code>) and <code>postProcessDisabled</code> set when the camera turns post-processing off. The settings stay the configured values in that case, so a script can edit the look it would get. <code>sceneWinner</code> and ambiguous stay the scene layer&#x27;s.</p>

#### `resolve_scene_settings` {#function-resolve-scene-settings}

<div class="function-declaration" id="function-resolve-scene-settings-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="components/resolved-render-settings.html">resolved_render_settings</a> resolve_scene_settings(
    <span class="k">const</span> <a href="render-settings/render-settings.html">render_settings</a> &amp;projectDefaults,
    std::span&lt;<span class="k">const</span> <a href="components/scene-settings-candidate.html">scene_settings_candidate</a>&gt; candidates,
    <span class="k">const</span> <a href="components/scene-settings-choice.html">scene_settings_choice</a> &amp;choice) <span class="k">noexcept</span>;</code></pre></div>

<p>The project defaults with each group that has a winner replaced by the winner&#x27;s group (source <code>SCENE</code>), with the winners and the ambiguity copied from <code>choice</code>. <code>A</code> winner that is not a candidate with a component asserts and the group keeps the project&#x27;s values. The values are not sanitised.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="camera_component"></a>
<a id="camera_component_type_name"></a>
<a id="camera_componentcameradata"></a>
<a id="camera_componentismain"></a>
<a id="camera_componentlayermask"></a>
<a id="camera_componentoperator"></a>
<a id="camera_componentpostprocess"></a>
<a id="camera_componentpriority"></a>
<a id="classes"></a>
<a id="conjure_reflect_enum"></a>
<a id="conjure_reflect_struct"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="environment_component"></a>
<a id="environment_component_type_name"></a>
<a id="environment_componentenvironment"></a>
<a id="environment_componentintensity"></a>
<a id="environment_componentoperator"></a>
<a id="environment_componentrotationradians"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="light_component"></a>
<a id="light_component_type_name"></a>
<a id="light_componentlightdata"></a>
<a id="light_componentoperator"></a>
<a id="make_camera_view"></a>
<a id="mesh_renderer_component"></a>
<a id="mesh_renderer_component_type_name"></a>
<a id="mesh_renderer_componentcastsshadows"></a>
<a id="mesh_renderer_componentlayermask"></a>
<a id="mesh_renderer_componentmaterials"></a>
<a id="mesh_renderer_componentmesh"></a>
<a id="mesh_renderer_componentoperator"></a>
<a id="mesh_renderer_componentreceivesshadows"></a>
<a id="other-declarations"></a>
<a id="register_components"></a>
<a id="sprite_component"></a>
<a id="sprite_component_type_name"></a>
<a id="sprite_componentcolour"></a>
<a id="sprite_componentlayer"></a>
<a id="sprite_componentoperator"></a>
<a id="sprite_componentorder"></a>
<a id="sprite_componentsize"></a>
<a id="sprite_componentspace"></a>
<a id="sprite_componenttexture"></a>
<a id="sprite_componentuvrect"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->

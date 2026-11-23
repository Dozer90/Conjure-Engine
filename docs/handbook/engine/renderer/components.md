---
kind: header
---

[Engine](../../engine.md) → [Renderer](../renderer.md)

# components.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/renderer/components.h` |
| API ↗ | [Rendering](../../../api/renderer/index.md), [Rendering a frame](../../../api/renderer/rendering-a-frame.md) |

<p>Component data the engine&#x27;s <code>renderer_subsystem</code> registers into the ECS. Plain reflected structs; this module never includes ecs. The engine&#x27;s extraction system maps them 1:1 onto the <code><a href="render-scene/render-scene.html">render_scene</a></code> structs, resolving asset references through <code>renderer::find_</code>*.</p>

## Structs

| File | Description |
|---|---|
| [`camera_component`](components/camera-component.md) |  |
| [`environment_component`](components/environment-component.md) |  |
| [`light_component`](components/light-component.md) |  |
| [`mesh_renderer_component`](components/mesh-renderer-component.md) |  |
| [`sprite_component`](components/sprite-component.md) |  |

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-CAMERA_COMPONENT_TYPE_NAME"></a><code>CAMERA_COMPONENT_TYPE_NAME</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code> | <code class="signature"><span class="s">"conjure.renderer.camera"</span></code> |  |
| <a id="variable-ENVIRONMENT_COMPONENT_TYPE_NAME"></a><code>ENVIRONMENT_COMPONENT_TYPE_NAME</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code> | <code class="signature"><span class="s">"conjure.renderer.environment"</span></code> |  |
| <a id="variable-LIGHT_COMPONENT_TYPE_NAME"></a><code>LIGHT_COMPONENT_TYPE_NAME</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code> | <code class="signature"><span class="s">"conjure.renderer.light"</span></code> |  |
| <a id="variable-MESH_RENDERER_COMPONENT_TYPE_NAME"></a><code>MESH_RENDERER_COMPONENT_TYPE_NAME</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code> | <code class="signature"><span class="s">"conjure.renderer.mesh_<wbr>renderer"</span></code> |  |
| <a id="variable-SPRITE_COMPONENT_TYPE_NAME"></a><code>SPRITE_COMPONENT_TYPE_NAME</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code> | <code class="signature"><span class="s">"conjure.renderer.sprite"</span></code> |  |

## Macros

### `CONJURE_REFLECT_ENUM` {#symbol-conjure-reflect-enum}

```cpp
CONJURE_REFLECT_ENUM(conjure::renderer::projection_type, "conjure.renderer.projection_type", PERSPECTIVE, ORTHOGRAPHIC)
```

### `CONJURE_REFLECT_STRUCT` {#symbol-conjure-reflect-struct}

```cpp
CONJURE_REFLECT_STRUCT(conjure::renderer::camera, "conjure.renderer.camera_data", CONJURE_FIELD(projection), CONJURE_FIELD(verticalFovRadians, .min = 0.01, .max = 3.1), CONJURE_FIELD(orthographicHeight, .min = 0.001), CONJURE_FIELD(nearPlane, .min = 0.0001), CONJURE_FIELD(farPlane, .min = 0.001))
```

## Functions

### Public

#### `make_camera_view` {#function-make-camera-view}

<div class="function-declaration" id="function-make-camera-view-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="render-scene/render-view.html">render_view</a> make_camera_view(
    <span class="k">const</span> <a href="components/camera-component.html">camera_component</a> &amp;camera,
    <span class="k">const</span> <a href="../core/math/glm-types.html#symbol-mat4">math::mat4</a> &amp;worldFromCamera,
    <a href="gpu-ids.html#symbol-render-target-id">render_target_id</a> target = {}) <span class="k">noexcept</span>;</code></pre></div>

<p>The view a camera component at a world matrix renders: the view matrix is the rigid inverse of the matrix (its scale is dropped, so a scaled parent does not warp the projection), with the camera&#x27;s position, camera data and layer mask, <code>NO_POST_PROCESS</code> unless <code>postProcess</code>, into <code>target</code> (invalid = the swapchain). The engine&#x27;s extraction builds the scene cameras&#x27; views with it and the editor&#x27;s screenshots through a scene camera do too, so both show the same picture.</p>

#### `register_components` {#function-register-components}

<div class="function-declaration" id="function-register-components-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; register_components(
    <a href="../core/reflect/type-registry/reflect-type-registry.html">core::reflect::type_registry</a> &amp;registry);</code></pre></div>

<p>Registers the enums, the nested camera and light structs and the five components with a reflection registry. Registering twice is harmless (<code>ALREADY_EXISTS</code> is skipped); any other registry error is returned.</p>

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

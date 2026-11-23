---
kind: header
---

[Engine](../../engine.md) → [Renderer](../renderer.md)

# gpu_ids.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/renderer/gpu_ids.h` |
| API ↗ | [Rendering](../../../api/renderer/index.md), [Rendering a frame](../../../api/renderer/rendering-a-frame.md) |

<p>Handles to GPU mirrors of assets and to render targets. Each id is a distinct <code><a href="../core/containers/slot-map/slot-map.html">core::slot_map</a></code> key type with the compact 32-bit index + 16-bit generation layout, so a slot retires after 65535 reuses. <code>A</code> stale id resolves to the fallback object (unit cube, white texture, magenta material, grey environment) and logs a <code>WARN</code> once.</p>

## Structs

| File | Description |
|---|---|
| [`detail::gpu_id`](gpu-ids/detail-gpu-id.md) | Common body of every gpu id: the slot index and the generation the id was issued with. Generation 0 is never issued, so a default id is invalid. |
| [`gpu_environment_tag`](gpu-ids/gpu-environment-tag.md) |  |
| [`gpu_material_tag`](gpu-ids/gpu-material-tag.md) |  |
| [`gpu_mesh_tag`](gpu-ids/gpu-mesh-tag.md) |  |
| [`gpu_texture_tag`](gpu-ids/gpu-texture-tag.md) |  |
| [`render_target_tag`](gpu-ids/render-target-tag.md) |  |
| [`std::hash<conjure::renderer::detail::gpu_id<Tag>>`](gpu-ids/std-hash-conjure-renderer-detail-gpu-id-tag.md) |  |

## Type Aliases

### `gpu_environment_id` {#symbol-gpu-environment-id}

```cpp
using gpu_environment_id = detail::gpu_id<gpu_environment_tag>;
```

### `gpu_material_id` {#symbol-gpu-material-id}

```cpp
using gpu_material_id = detail::gpu_id<gpu_material_tag>;
```

### `gpu_mesh_id` {#symbol-gpu-mesh-id}

```cpp
using gpu_mesh_id = detail::gpu_id<gpu_mesh_tag>;
```

### `gpu_texture_id` {#symbol-gpu-texture-id}

```cpp
using gpu_texture_id = detail::gpu_id<gpu_texture_tag>;
```

### `render_target_id` {#symbol-render-target-id}

```cpp
using render_target_id = detail::gpu_id<render_target_tag>;
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
<a id="classes"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="detailgpu_id"></a>
<a id="detailgpu_idfrom_u64"></a>
<a id="detailgpu_idgeneration"></a>
<a id="detailgpu_idindex"></a>
<a id="detailgpu_idis_valid"></a>
<a id="detailgpu_idoperator"></a>
<a id="detailgpu_idoperator_1"></a>
<a id="detailgpu_idto_u64"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="gpu_environment_id"></a>
<a id="gpu_environment_tag"></a>
<a id="gpu_id"></a>
<a id="gpu_idfrom_u64"></a>
<a id="gpu_idgeneration"></a>
<a id="gpu_idindex"></a>
<a id="gpu_idis_valid"></a>
<a id="gpu_idoperator"></a>
<a id="gpu_idoperator_1"></a>
<a id="gpu_idto_u64"></a>
<a id="gpu_material_id"></a>
<a id="gpu_material_tag"></a>
<a id="gpu_mesh_id"></a>
<a id="gpu_mesh_tag"></a>
<a id="gpu_texture_id"></a>
<a id="gpu_texture_tag"></a>
<a id="internals-and-algorithms"></a>
<a id="other-declarations"></a>
<a id="render_target_id"></a>
<a id="render_target_tag"></a>
<a id="std"></a>
<a id="stdhashconjurerendererdetailgpu_idtag"></a>
<a id="stdhashconjurerendererdetailgpu_idtagoperator"></a>
<a id="stdoperator"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->

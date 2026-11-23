---
kind: header
---

[Engine](../../engine.md) → [Scene](../scene.md)

# scene_serializer.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/scene/scene_serializer.h` |
| API ↗ | [Scenes, hierarchy and prefabs](../../../api/scene/index.md), [Scene files, prefabs and snapshots](../../../api/scene/scene-files-and-prefabs.md) |

<p>Stateless file and archive operations over a scene.</p>

## Structs

| File | Description |
|---|---|
| [`prefab_document`](scene-serializer/prefab-document.md) | <code>A</code> parsed prefab file: entry 0 of the archive is the single root. |
| [`scene_document`](scene-serializer/scene-document.md) | <code>A</code> parsed scene file; also the published payload of the <code>conjure.scene.scene</code> asset type. |

## Classes

| File | Description |
|---|---|
| [`scene_serializer`](scene-serializer/scene-serializer.md) | Stateless file and archive operations over a scene. |

## Type Aliases

### `scene_asset` {#symbol-scene-asset}

```cpp
using scene_asset = scene_document;
```

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-PREFAB_ASSET_TYPE_NAME"></a><code>PREFAB_ASSET_TYPE_NAME</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code> | <code class="signature"><span class="s">"conjure.scene.prefab"</span></code> |  |
| <a id="variable-PREFAB_FILE_EXTENSION"></a><code>PREFAB_FILE_EXTENSION</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code> | <code class="signature"><span class="s">".prefab.json"</span></code> |  |
| <a id="variable-PREFAB_FORMAT_NAME"></a><code>PREFAB_FORMAT_NAME</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code> | <code class="signature"><span class="s">"conjure.prefab"</span></code> |  |
| <a id="variable-SCENE_ASSET_TYPE_NAME"></a><code>SCENE_ASSET_TYPE_NAME</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code> | <code class="signature"><span class="s">"conjure.scene.scene"</span></code> |  |
| <a id="variable-SCENE_FILE_EXTENSION"></a><code>SCENE_FILE_EXTENSION</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code> | <code class="signature"><span class="s">".scene.json"</span></code> |  |
| <a id="variable-SCENE_FORMAT_NAME"></a><code>SCENE_FORMAT_NAME</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code> | <code class="signature"><span class="s">"conjure.scene"</span></code> |  |

## Macros

### `CONJURE_DECLARE_ASSET_TYPE` {#symbol-conjure-declare-asset-type}

```cpp
CONJURE_DECLARE_ASSET_TYPE(conjure::scene::scene_asset, "conjure.scene.scene")
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
<a id="conjure_declare_asset_type"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="other-declarations"></a>
<a id="prefab_asset_type_name"></a>
<a id="prefab_document"></a>
<a id="prefab_documentarchive"></a>
<a id="prefab_documentname"></a>
<a id="prefab_file_extension"></a>
<a id="prefab_format_name"></a>
<a id="scene_asset"></a>
<a id="scene_asset_type_name"></a>
<a id="scene_document"></a>
<a id="scene_documentarchive"></a>
<a id="scene_documentname"></a>
<a id="scene_documentroots"></a>
<a id="scene_documentsettings"></a>
<a id="scene_file_extension"></a>
<a id="scene_format_name"></a>
<a id="scene_serializer"></a>
<a id="scene_serializerbind_asset_refs"></a>
<a id="scene_serializerbind_entity"></a>
<a id="scene_serializercapture_subtree"></a>
<a id="scene_serializercollect_hierarchy_order"></a>
<a id="scene_serializerinstantiate_subtree"></a>
<a id="scene_serializerload_scene"></a>
<a id="scene_serializerload_scene_file"></a>
<a id="scene_serializerparse_document"></a>
<a id="scene_serializerparse_prefab"></a>
<a id="scene_serializersave_scene"></a>
<a id="scene_serializersave_scene_file"></a>
<a id="scene_serializersave_subtree"></a>
<a id="scene_serializerwrite_document"></a>
<a id="scene_serializerwrite_prefab"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->

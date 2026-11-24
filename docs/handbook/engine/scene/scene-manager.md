---
kind: header
---

[Engine](../../engine.md) → [Scene](../scene.md)

# scene_manager.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/scene/scene_manager.h` |
| API ↗ | [Scenes, hierarchy and prefabs](../../../api/scene/index.md), [Scene files, prefabs and snapshots](../../../api/scene/scene-files-and-prefabs.md) |

<p>Scene residency (4.7, S11): the one scene over the world, the loaded scene records with their <code><a href="../assets/asset-manager/asset-load-batch.html">asset_load_batch</a></code>, synchronous and deferred loads (replace or additive), unloads that cascade through the scene and release the assets, the layer names and the transform system the engine registers. Main thread only.</p>

## Structs

| File | Description |
|---|---|
| [`load_request_id`](scene-manager/load-request-id.md) | Handle of a deferred load request (0 is invalid). |
| [`loaded_scene_info`](scene-manager/loaded-scene-info.md) | One loaded scene asset. |
| [`scene_manager::loaded_record`](scene-manager/scene-manager-loaded-record.md) |  |
| [`scene_manager::pending_load`](scene-manager/scene-manager-pending-load.md) |  |

## Classes

| File | Description |
|---|---|
| [`layer_table`](scene-manager/layer-table.md) | Names of the 32 layers, filled by the engine from <code>project.json</code>. |
| [`scene_manager`](scene-manager/scene-manager.md) |  |

## Enumerations

### `load_mode` {#symbol-load-mode}

```cpp
enum class load_mode : u8 {
    REPLACE,
    ADDITIVE
};
```

Whether a load replaces the current content or adds to it. Every load (load, <code>load_file</code>, <code>request_load</code> and <code>C</code># <code>Scene.Load</code>) replaces unless <code>ADDITIVE</code> is asked for.

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="layer_table"></a>
<a id="layer_tablefind"></a>
<a id="layer_tablelayer_count"></a>
<a id="layer_tablelayer_table"></a>
<a id="layer_tablelayer_table_1"></a>
<a id="layer_tablemnames"></a>
<a id="layer_tablename"></a>
<a id="layer_tablenames"></a>
<a id="layer_tableset_name"></a>
<a id="load_mode"></a>
<a id="load_modeadditive"></a>
<a id="load_modereplace"></a>
<a id="load_request_id"></a>
<a id="load_request_idis_valid"></a>
<a id="load_request_idoperator"></a>
<a id="load_request_idvalue"></a>
<a id="loaded_scene_info"></a>
<a id="loaded_scene_infoasset"></a>
<a id="loaded_scene_infoid"></a>
<a id="loaded_scene_infoname"></a>
<a id="loaded_scene_infopath"></a>
<a id="loaded_scene_inforoots"></a>
<a id="other-declarations"></a>
<a id="scene_manager"></a>
<a id="scene_manageractive"></a>
<a id="scene_managerallocate_record"></a>
<a id="scene_managerapply_document"></a>
<a id="scene_managerapply_pending_loads"></a>
<a id="scene_managerassets"></a>
<a id="scene_managercollect_owned_roots"></a>
<a id="scene_managerfind_loaded"></a>
<a id="scene_managerfind_record"></a>
<a id="scene_managerinfo"></a>
<a id="scene_manageris_load_pending"></a>
<a id="scene_manageris_loaded"></a>
<a id="scene_managerlayers"></a>
<a id="scene_managerload"></a>
<a id="scene_managerload_file"></a>
<a id="scene_managerloaded_record"></a>
<a id="scene_managerloaded_recordalive"></a>
<a id="scene_managerloaded_recordbatch"></a>
<a id="scene_managerloaded_recordinfo"></a>
<a id="scene_managerloaded_scenes"></a>
<a id="scene_managermassets"></a>
<a id="scene_managermlayers"></a>
<a id="scene_managermloadorder"></a>
<a id="scene_managermnextgeneration"></a>
<a id="scene_managermnextrequest"></a>
<a id="scene_managermonbeforeunload"></a>
<a id="scene_managermonloaded"></a>
<a id="scene_managermonloadfailed"></a>
<a id="scene_managermpending"></a>
<a id="scene_managermpendingunloads"></a>
<a id="scene_managermrecords"></a>
<a id="scene_managermscene"></a>
<a id="scene_managermtransforms"></a>
<a id="scene_managermworld"></a>
<a id="scene_manageron_before_unload"></a>
<a id="scene_manageron_load_failed"></a>
<a id="scene_manageron_loaded"></a>
<a id="scene_manageroperator"></a>
<a id="scene_managerpending_count"></a>
<a id="scene_managerpending_load"></a>
<a id="scene_managerpending_loadasset"></a>
<a id="scene_managerpending_loadhandle"></a>
<a id="scene_managerpending_loadid"></a>
<a id="scene_managerpending_loadmode"></a>
<a id="scene_managerrequest_load"></a>
<a id="scene_managerrequest_unload"></a>
<a id="scene_managerscene_manager"></a>
<a id="scene_managerscene_manager_1"></a>
<a id="scene_managertransforms"></a>
<a id="scene_managerunload"></a>
<a id="scene_managerunload_all"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->

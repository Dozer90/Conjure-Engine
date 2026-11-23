---
kind: header
---

[Engine](../../engine.md) → [Scene](../scene.md)

# scene.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/scene/scene.h` |
| API ↗ | [Scenes, hierarchy and prefabs](../../../api/scene/index.md), [Scenes and the hierarchy](../../../api/scene/scenes-and-hierarchy.md) |

<p>One authored <code><a href="scene/scene.html">scene</a></code> over one <code><a href="../ecs/world/world.html">ecs::world</a></code>: roots in order, the hierarchy and transform operations, names, tags, layers, guids, enabled state, the destroy queue and the archive instantiation shared by <code><a href="scene/scene.html">scene</a></code> files, prefabs, duplication and snapshots. Every method is owner-thread only (the world&#x27;s rules apply); observers registered in the constructor keep the children cache, the guid index and the transform companions consistent when scripts reach the raw ECS.</p>

## Structs

| File | Description |
|---|---|
| [`instantiate_options`](scene/instantiate-options.md) | Placement and guid handling of an archive instantiation. |
| [`instantiate_report`](scene/instantiate-report.md) | What an instantiation produced. |
| [`root_entry`](scene/root-entry.md) | One root of the <code><a href="scene/scene.html">scene</a></code> and the loaded <code><a href="scene/scene.html">scene</a></code> asset it belongs to. |
| [`scene::repair_entry`](scene/scene-repair-entry.md) | One hierarchy repair scheduled for <code><a href="scene/scene.html">scene</a>.apply_hierarchy_changes</code> (3.7). <code>A</code> <code>deferred_set_parent</code> entry carries the parent the caller&#x27;s command buffer installs at its flush (<code>hasIntent</code>): the repair waits until the live parent component agrees, since the flush may land after the <code>TRANSFORM</code> phase that first sees the entry. |
| [`scene_id`](scene/scene-id.md) | Identity of a loaded <code><a href="scene/scene.html">scene</a></code> asset inside a <code><a href="scene-manager/scene-manager.html">scene_manager</a></code>. Generation 0 means &quot;none&quot;. |

## Classes

| File | Description |
|---|---|
| [`scene`](scene/scene.md) |  |
| [`scene::operation_scope`](scene/scene-operation-scope.md) | Marks a <code><a href="scene/scene.html">scene</a></code> operation: observers skip their bookkeeping while one is running. |

## Enumerations

### `guid_policy` {#symbol-guid-policy}

```cpp
enum class guid_policy : u8 {
    KEEP,                     // every guid kept (a null one stays null); a collision is DUPLICATE_GUID
    REGENERATE_ON_CONFLICT,   // colliding entries get fresh guids (one WARN per call), null guids too
    REGENERATE_ALL            // every entity gets a fresh guid (prefabs, duplicates, count > 1)
};
```

What happens to the guids of an archive when it is instantiated.

### `reparent_mode` {#symbol-reparent-mode}

```cpp
enum class reparent_mode : u8 {
    KEEP_WORLD,               // the local transform is recomputed so the world pose stays where it is
    KEEP_LOCAL                // the local transform is kept: the world pose follows the new parent
};
```

Which transform a reparent preserves (<code>scene::set_parent</code>, <code>scene::deferred_set_parent</code>). An enum rather than a bool so a sibling index cannot bind to the mode by mistake.

## Forward Declarations

### `detail::archive_shape` {#symbol-detail-archive-shape}

```cpp
struct archive_shape;
```

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-SCENE_ID_AUTHORED"></a><code>SCENE_ID_AUTHORED</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="scene/scene-id.html">scene_<wbr>id</a></code> | <code class="signature"><a href="scene/scene-id.html">scene_id</a>{<span class="mi">0</span>, <span class="mi">1</span>}</code> | The implicit owner of entities created by hand (not loaded from a <code><a href="scene/scene.html">scene</a></code> asset). |

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="archive_shape"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="detailarchive_shape"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="guid_policy"></a>
<a id="guid_policykeep"></a>
<a id="guid_policyregenerate_all"></a>
<a id="guid_policyregenerate_on_conflict"></a>
<a id="instantiate_options"></a>
<a id="instantiate_optionsbindassets"></a>
<a id="instantiate_optionsguids"></a>
<a id="instantiate_optionsindex"></a>
<a id="instantiate_optionsowner"></a>
<a id="instantiate_optionsparent"></a>
<a id="instantiate_optionsrootname"></a>
<a id="instantiate_optionsroottransform"></a>
<a id="instantiate_report"></a>
<a id="instantiate_reportassetids"></a>
<a id="instantiate_reportentities"></a>
<a id="instantiate_reportentitycount"></a>
<a id="instantiate_reportunknowncomponents"></a>
<a id="internals-and-algorithms"></a>
<a id="other-declarations"></a>
<a id="root_entry"></a>
<a id="root_entryentity"></a>
<a id="root_entryoperator"></a>
<a id="root_entryowner"></a>
<a id="scene"></a>
<a id="scene_id"></a>
<a id="scene_id_authored"></a>
<a id="scene_idgeneration"></a>
<a id="scene_idindex"></a>
<a id="scene_idis_valid"></a>
<a id="scene_idoperator"></a>
<a id="sceneadd_companions"></a>
<a id="sceneadd_dirty"></a>
<a id="sceneadd_transform"></a>
<a id="sceneapply_hierarchy_changes"></a>
<a id="sceneassets"></a>
<a id="scenebump_structure"></a>
<a id="scenecheck_reparent"></a>
<a id="sceneclear"></a>
<a id="scenecontains"></a>
<a id="scenecreate_entities"></a>
<a id="scenecreate_entity"></a>
<a id="scenecreate_entity_unannounced"></a>
<a id="scenedeferred_set_parent"></a>
<a id="scenedestroy"></a>
<a id="scenedestroy_now"></a>
<a id="scenedestroy_pending"></a>
<a id="scenedestroy_unannounced"></a>
<a id="sceneduplicate_subtree"></a>
<a id="sceneensure_guid"></a>
<a id="sceneerase_root"></a>
<a id="scenefind_by_layer"></a>
<a id="scenefind_by_name"></a>
<a id="scenefind_by_tag"></a>
<a id="scenefind_guid"></a>
<a id="scenefind_path"></a>
<a id="sceneflush_scratch"></a>
<a id="sceneguid_of"></a>
<a id="sceneguids"></a>
<a id="sceneids"></a>
<a id="sceneinsert_root"></a>
<a id="sceneinstantiate_archive"></a>
<a id="sceneis_active"></a>
<a id="sceneis_enabled"></a>
<a id="scenelayer_of"></a>
<a id="scenelink_child"></a>
<a id="scenelink_instantiated"></a>
<a id="scenelocal_transform_of"></a>
<a id="scenemassets"></a>
<a id="scenemax_depth_in_use"></a>
<a id="scenemdestroyqueue"></a>
<a id="scenemexpectedrepairs"></a>
<a id="scenemguids"></a>
<a id="scenemids"></a>
<a id="sceneminoperation"></a>
<a id="scenemmaxdepthinuse"></a>
<a id="scenemname"></a>
<a id="scenemobservers"></a>
<a id="scenemonsettingschanged"></a>
<a id="scenemonstructurechanged"></a>
<a id="scenemrawedits"></a>
<a id="scenemrepairqueue"></a>
<a id="scenemrepairspare"></a>
<a id="scenemroots"></a>
<a id="scenemscratch"></a>
<a id="scenemscratchentities"></a>
<a id="scenemsettings"></a>
<a id="scenemstructureversion"></a>
<a id="scenemtree"></a>
<a id="scenemworld"></a>
<a id="scenename"></a>
<a id="scenename_of"></a>
<a id="sceneon_entity_destroyed"></a>
<a id="sceneon_guid_added"></a>
<a id="sceneon_guid_removed"></a>
<a id="sceneon_local_transform_added"></a>
<a id="sceneon_parent_added"></a>
<a id="sceneon_parent_removed"></a>
<a id="sceneon_settings_changed"></a>
<a id="sceneon_structure_changed"></a>
<a id="sceneoperation_scope"></a>
<a id="sceneoperation_scopemowner"></a>
<a id="sceneoperation_scopemprevious"></a>
<a id="sceneoperation_scopeoperation_scope"></a>
<a id="sceneoperation_scopeoperation_scope_1"></a>
<a id="sceneoperation_scopeoperator"></a>
<a id="sceneoperator"></a>
<a id="sceneowner_of_tree"></a>
<a id="sceneparent_chain_inactive"></a>
<a id="scenepending_destroy_count"></a>
<a id="scenepending_repair_count"></a>
<a id="scenequeue_repair"></a>
<a id="scenerefresh_inactive_subtree"></a>
<a id="sceneregister_observers"></a>
<a id="scenerenumber_children"></a>
<a id="scenerepair_entity"></a>
<a id="scenerepair_entry"></a>
<a id="scenerepair_entryentity"></a>
<a id="scenerepair_entryhasintent"></a>
<a id="scenerepair_entryintendedparent"></a>
<a id="scenerepair_entryowner"></a>
<a id="scenerepair_entrypreviousparent"></a>
<a id="scenerepair_entryrootindex"></a>
<a id="sceneresolve_guid"></a>
<a id="sceneretag_subtree"></a>
<a id="sceneroot_index_of"></a>
<a id="sceneroot_owner"></a>
<a id="sceneroots"></a>
<a id="scenescene"></a>
<a id="scenescene_1"></a>
<a id="sceneset_enabled"></a>
<a id="sceneset_guid"></a>
<a id="sceneset_layer"></a>
<a id="sceneset_local_transform"></a>
<a id="sceneset_name"></a>
<a id="sceneset_parent"></a>
<a id="sceneset_root_order"></a>
<a id="sceneset_root_owner"></a>
<a id="sceneset_settings"></a>
<a id="sceneset_sibling_index"></a>
<a id="sceneset_tag"></a>
<a id="sceneset_world_transform"></a>
<a id="scenesettings"></a>
<a id="scenestructure_version"></a>
<a id="scenesubtree_bounds"></a>
<a id="scenetag_of"></a>
<a id="scenetree"></a>
<a id="sceneunlink_child"></a>
<a id="scenevalidate"></a>
<a id="sceneworld"></a>
<a id="sceneworld_bounds"></a>
<a id="sceneworld_transform_of"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->

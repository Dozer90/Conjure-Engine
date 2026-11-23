---
kind: header
---

[Engine](../../engine.md) → [Scene](../scene.md)

# components.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/scene/components.h` |
| API ↗ | [Scenes, hierarchy and prefabs](../../../api/scene/index.md), [Scenes and the hierarchy](../../../api/scene/scenes-and-hierarchy.md) |

<p>The scene components. Plain structs with camelCase fields, named &quot;<code>conjure.scene</code>.&lt;<code><a href="components/name.html">name</a></code>&gt;&quot; and registered in a fixed order by <code>register_components()</code> (<code>register.h</code>) so the ids agree in every world of a process. The hierarchy is <code><a href="components/parent.html">parent</a></code> (the single serialised truth) plus the <code>TRANSIENT</code> <code><a href="components/children.html">children</a></code> cache; depth is encoded as one <code><a href="components/depth-tag.html">depth_tag</a>&lt;D&gt;</code> per level so the transform pass can run one level at a time over chunks (S2). <code><a href="components/interpolated-transform.html">interpolated_transform</a></code> is defined here but added, written and removed by physics.</p>

## Structs

| File | Description |
|---|---|
| [`children`](components/children.md) | Cache of the <code><a href="components/children.html">children</a></code> in sibling order, rebuilt from <code><a href="components/parent.html">parent</a></code> on load (<code>TRANSIENT</code>, S3). |
| [`conjure::ecs::component_name<conjure::scene::depth_tag<Depth>>`](components/conjure-ecs-component-name-conjure-scene-depth-tag-depth.md) |  |
| [`depth_tag`](components/depth-tag.md) | Depth marker: entities at hierarchy depth Depth carry <code><a href="components/depth-tag.html">depth_tag</a>&lt;Depth&gt;</code>; roots carry none (<code>TRANSIENT</code>, <code>EDITOR_HIDDEN</code>). Registered as &quot;<code>conjure.scene.depth_01</code>&quot; .. &quot;<code>conjure.scene.depth_31</code>&quot;. |
| [`detail::depth_tag_name`](components/detail-depth-tag-name.md) | Static storage for the <code><a href="components/name.html">name</a></code> of one depth <code><a href="components/tag.html">tag</a></code> so a <code>string_view</code> may point at it. |
| [`disabled`](components/disabled.md) | Self state: the entity is switched off (serialised as {}). |
| [`guid`](components/guid.md) | Editor identity, indexed by <code><a href="guid-index/guid-index.html">guid_index</a></code>. |
| [`inactive`](components/inactive.md) | On every descendant of a <code><a href="components/disabled.html">disabled</a></code> entity; consumers filter <code>without&lt;<a href="components/disabled.html">disabled</a>&gt;()</code>.<code>without&lt;<a href="components/inactive.html">inactive</a>&gt;()</code> (<code>TRANSIENT</code>, <code>EDITOR_HIDDEN</code>). |
| [`interpolated_transform`](components/interpolated-transform.md) | World-space poses of the two most recent fixed steps. Scene defines and registers it; physics adds, writes and removes it. |
| [`layer`](components/layer.md) | Layer 0..31, a grouping index for <code>find_by_layer</code>, scripts and the editor; names live in <code><a href="scene-manager/layer-table.html">layer_table</a></code>. It does not decide what a camera draws: <code>mesh_renderer_component::layerMask</code> against the camera&#x27;s layerMask does (the same bit numbering), and sprites are not masked. Captures (snapshot, save, duplicate, prefab) and <code>instantiate_archive</code> refuse an index of 32 or more with <code>BAD_FORMAT</code>. |
| [`local_bounds`](components/local-bounds.md) | Local-space bounds written by engine subsystems; <code>scene::world_bounds</code> transforms them (<code>TRANSIENT</code>). |
| [`local_transform`](components/local-transform.md) | Position, rotation and scale relative to the <code><a href="components/parent.html">parent</a></code> (or to the world for roots). Byte-identical to <code><a href="../core/math/transform/transform.html">math::transform</a></code> and mirrored by the <code>C</code># LocalTransform struct. |
| [`name`](components/name.md) | Display <code><a href="components/name.html">name</a></code>, interned in every configuration. |
| [`parent`](components/parent.md) | Link to the <code><a href="components/parent.html">parent</a></code> entity plus the position among its siblings. Roots have no <code><a href="components/parent.html">parent</a></code> component. |
| [`prefab_instance`](components/prefab-instance.md) | On the root of a prefab instance: the prefab asset it was instantiated from (24 bytes, the <code>C</code># PrefabInstance mirror). |
| [`prefab_link`](components/prefab-link.md) | On every entity instantiated from a prefab: the <code><a href="components/guid.html">guid</a></code> of the matching entry in the prefab file. |
| [`render_world_transform`](components/render-world-transform.md) | What renderer extraction reads: the world matrix, or the interpolated pose for physics-driven entities and their descendants (<code>TRANSIENT</code>). |
| [`scene_component_ids`](components/scene-component-ids.md) | The component ids of every scene component in one registry, resolved once (depth[0] is <code>INVALID</code>: roots carry no <code><a href="components/tag.html">tag</a></code>). |
| [`tag`](components/tag.md) | Gameplay <code><a href="components/tag.html">tag</a></code>. |
| [`transform_dirty`](components/transform-dirty.md) | Set by structural hierarchy changes; cleared by the propagation pass (<code>TRANSIENT</code>, <code>EDITOR_HIDDEN</code>). |
| [`transform_state`](components/transform-state.md) | Bookkeeping of the propagation pass: the run that last recomputed the world matrix (<code><a href="components/children.html">children</a></code> compare it against the current run), the flags, and the pending mark the <code><a href="components/parent.html">parent</a></code>&#x27;s level pass sets on a child that must be revisited this run (its <code><a href="components/parent.html">parent</a></code> recomputed, sits in an interpolated subtree or changed flags); the child&#x27;s level clears it. The mark lets a level find its dirty rows with a sequential scan of this column instead of a <code><a href="components/parent.html">parent</a></code> lookup per row (<code>TRANSIENT</code>, <code>EDITOR_HIDDEN</code>). |
| [`world_transform`](components/world-transform.md) | Simulation truth: the world matrix computed by <code>scene.propagate_transforms</code> (<code>TRANSIENT</code>). |

## Enumerations

### `transform_state_flags` {#symbol-transform-state-flags}

```cpp
enum class transform_state_flags : u8 {
    NONE = 0,
    INTERPOLATED_SUBTREE = 1 << 0    // self or an ancestor carries interpolated_transform
};
```

Per-entity propagation flags.

## Forward Declarations

### `prefab_asset` {#symbol-prefab-asset}

```cpp
struct prefab_asset;
```

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-APPEND_CHILD"></a><code>APPEND_CHILD</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">0</span>xFFFFFFFFu</code> | Sibling index meaning &quot;after the last child&quot; (or &quot;after the last root&quot;). |
| <a id="variable-detail::DEPTH_TAG_NAME_LENGTH"></a><code>detail::DEPTH_TAG_NAME_LENGTH</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-usize">usize</a></code> | <code class="signature">DEPTH_<wbr>TAG_<wbr>PREFIX.size() + <span class="mi">2</span></code> |  |
| <a id="variable-detail::DEPTH_TAG_PREFIX"></a><code>detail::DEPTH_TAG_PREFIX</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code> | <code class="signature"><span class="s">"conjure.scene.depth_"</span></code> |  |
| <a id="variable-MAX_HIERARCHY_DEPTH"></a><code>MAX_HIERARCHY_DEPTH</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">32</span></code> | Depth 0 is a root; <code><a href="components/children.html">children</a></code> carry <code><a href="components/depth-tag.html">depth_tag</a>&lt;1&gt;</code> .. <code><a href="components/depth-tag.html">depth_tag</a>&lt;31&gt;</code>. |
| <a id="variable-SCENE_FORMAT_VERSION"></a><code>SCENE_FORMAT_VERSION</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">1</span></code> | Version written into every scene and prefab file; a newer version is <code>UNSUPPORTED_VERSION</code>. |

## Macros

### `CONJURE_COMPONENT` {#symbol-conjure-component}

```cpp
CONJURE_COMPONENT(conjure::scene::parent, "conjure.scene.parent")
```

### `CONJURE_ENUM_FLAGS` {#symbol-conjure-enum-flags}

```cpp
CONJURE_ENUM_FLAGS(transform_state_flags)
```

### `CONJURE_REFLECT_STRUCT` {#symbol-conjure-reflect-struct}

```cpp
CONJURE_REFLECT_STRUCT(conjure::scene::parent, "conjure.scene.parent", CONJURE_FIELD(value), CONJURE_FIELD(siblingIndex, .readOnly = true))
```

## Functions

### Public

#### `detail::make_depth_tag_name` {#function-detail-make-depth-tag-name}

<div class="function-declaration" id="function-detail-make-depth-tag-name-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> std::array&lt;<span class="k">char</span>, DEPTH_TAG_NAME_LENGTH&gt; make_depth_tag_name(<a href="../core/types.html#symbol-u8">u8</a> depth) <span class="k">noexcept</span>;</code></pre></div>

<p>Builds &quot;<code>conjure.scene.depth_DD</code>&quot; at compile time.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="append_child"></a>
<a id="children"></a>
<a id="childrenentities"></a>
<a id="classes"></a>
<a id="conjure"></a>
<a id="conjure_component"></a>
<a id="conjure_enum_flags"></a>
<a id="conjure_reflect_struct"></a>
<a id="conjureecscomponent_nameconjurescenedepth_tagdepth"></a>
<a id="conjureecscomponent_nameconjurescenedepth_tagdepthvalue"></a>
<a id="conjurevalue"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="depth_tag"></a>
<a id="depth_tag_name"></a>
<a id="depth_tag_name_length"></a>
<a id="depth_tag_namecharacters"></a>
<a id="depth_tag_namevalue"></a>
<a id="depth_tag_prefix"></a>
<a id="detaildepth_tag_name"></a>
<a id="detaildepth_tag_name_length"></a>
<a id="detaildepth_tag_namecharacters"></a>
<a id="detaildepth_tag_namevalue"></a>
<a id="detaildepth_tag_prefix"></a>
<a id="detailmake_depth_tag_name"></a>
<a id="direct-header-dependencies"></a>
<a id="disabled"></a>
<a id="extension-points"></a>
<a id="guid"></a>
<a id="guidoperator"></a>
<a id="guidvalue"></a>
<a id="inactive"></a>
<a id="internals-and-algorithms"></a>
<a id="interpolated_transform"></a>
<a id="interpolated_transformcurrent"></a>
<a id="interpolated_transformprevious"></a>
<a id="layer"></a>
<a id="layerindex"></a>
<a id="layeroperator"></a>
<a id="local_bounds"></a>
<a id="local_boundsbox"></a>
<a id="local_transform"></a>
<a id="local_transformfrom_transform"></a>
<a id="local_transformoperator"></a>
<a id="local_transformposition"></a>
<a id="local_transformrotation"></a>
<a id="local_transformscale"></a>
<a id="local_transformto_transform"></a>
<a id="make_depth_tag_name"></a>
<a id="max_hierarchy_depth"></a>
<a id="name"></a>
<a id="nameoperator"></a>
<a id="namevalue"></a>
<a id="other-declarations"></a>
<a id="parent"></a>
<a id="parentoperator"></a>
<a id="parentsiblingindex"></a>
<a id="parentvalue"></a>
<a id="prefab_asset"></a>
<a id="prefab_instance"></a>
<a id="prefab_instanceprefab"></a>
<a id="prefab_link"></a>
<a id="prefab_linkassetguid"></a>
<a id="prefab_linkoperator"></a>
<a id="render_world_transform"></a>
<a id="render_world_transformmatrix"></a>
<a id="scene_component_ids"></a>
<a id="scene_component_idschildren"></a>
<a id="scene_component_idsdepth"></a>
<a id="scene_component_idsdepth_of"></a>
<a id="scene_component_idsdisabled"></a>
<a id="scene_component_idsguid"></a>
<a id="scene_component_idshas_transform_companions"></a>
<a id="scene_component_idsinactive"></a>
<a id="scene_component_idsinterpolatedtransform"></a>
<a id="scene_component_idsis_complete"></a>
<a id="scene_component_idslayer"></a>
<a id="scene_component_idslocalbounds"></a>
<a id="scene_component_idslocaltransform"></a>
<a id="scene_component_idsname"></a>
<a id="scene_component_idsparent"></a>
<a id="scene_component_idsprefabinstance"></a>
<a id="scene_component_idsprefablink"></a>
<a id="scene_component_idsrenderworldtransform"></a>
<a id="scene_component_idsresolve"></a>
<a id="scene_component_idstag"></a>
<a id="scene_component_idstransformdirty"></a>
<a id="scene_component_idstransformstate"></a>
<a id="scene_component_idsworldtransform"></a>
<a id="scene_format_version"></a>
<a id="structs-and-types"></a>
<a id="tag"></a>
<a id="tagoperator"></a>
<a id="tagvalue"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="transform_dirty"></a>
<a id="transform_state"></a>
<a id="transform_state_flags"></a>
<a id="transform_state_flagsinterpolated_subtree"></a>
<a id="transform_state_flagsnone"></a>
<a id="transform_stateflags"></a>
<a id="transform_statelastupdaterun"></a>
<a id="transform_statepending"></a>
<a id="variables-and-properties"></a>
<a id="world_transform"></a>
<a id="world_transformmatrix"></a>
<!-- doc-legacy-anchors:end -->

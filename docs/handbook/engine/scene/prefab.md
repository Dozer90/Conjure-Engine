---
kind: header
---

[Engine](../../engine.md) → [Scene](../scene.md)

# prefab.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/scene/prefab.h` |
| API ↗ | [Scenes, hierarchy and prefabs](../../../api/scene/index.md), [Scene files, prefabs and snapshots](../../../api/scene/scene-files-and-prefabs.md) |

<p>Prefabs: fully materialised instances marked with <code><a href="components/prefab-instance.html">prefab_instance</a></code> on the root and <code><a href="components/prefab-link.html">prefab_link</a></code> on every entity (S9). Overrides are computed by diff, not stored; apply, revert and unpack are archive operations. <code><a href="prefab/prefab-builder.html">prefab_builder</a></code> is the type-agnostic way the engine turns a model into a prefab document.</p>

## Structs

| File | Description |
|---|---|
| [`entity_diff`](prefab/entity-diff.md) | One difference between a prefab instance and its asset. |
| [`prefab_asset`](prefab/prefab-asset.md) | The published payload of the <code>conjure.scene.prefab</code> asset type. |
| [`prefab_builder::node`](prefab/prefab-builder-node.md) |  |
| [`prefab_builder::node_component`](prefab/prefab-builder-node-component.md) |  |
| [`prefab_diff`](prefab/prefab-diff.md) | The differences of one instance. |

## Classes

| File | Description |
|---|---|
| [`prefab_builder`](prefab/prefab-builder.md) | Builds a prefab document node by node without naming any component type (3.13): the engine&#x27;s model converter adds renderer components by registered name. |

## Enumerations

### `entity_diff_kind` {#symbol-entity-diff-kind}

```cpp
enum class entity_diff_kind : u8 {
    ADDED,      // an instance entity without a matching prefab entry
    REMOVED,    // a prefab entry without a matching instance entity
    MODIFIED    // components differ (names listed)
};
```

How an instance entity differs from its prefab entry.

## Macros

### `CONJURE_DECLARE_ASSET_TYPE` {#symbol-conjure-declare-asset-type}

```cpp
CONJURE_DECLARE_ASSET_TYPE(conjure::scene::prefab_asset, "conjure.scene.prefab")
```

## Functions

### Public

#### `prefab::apply` {#function-prefab-apply}

<div class="function-declaration" id="function-prefab-apply-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<a href="scene-serializer/prefab-document.html">prefab_document</a>&gt; apply(<span class="k">const</span> <a href="scene/scene.html">scene</a> &amp;source, <a href="../ecs/entity/entity.html">ecs::entity</a> root);</code></pre></div>

<p>The instance as a new prefab document: <code><a href="components/prefab-link.html">prefab_link</a></code> guids become entry guids (fresh ones for new entities and for a link guid an earlier entity already used) and the root loses its <code><a href="components/prefab-instance.html">prefab_instance</a></code>. <code>NOT_A_PREFAB_INSTANCE</code>.</p>

#### `prefab::build_document` {#function-prefab-build-document}

<div class="function-declaration" id="function-prefab-build-document-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<a href="scene-serializer/prefab-document.html">prefab_document</a>&gt; build_document( <span class="k">const</span> <a href="scene/scene.html">scene</a> &amp;source, <a href="../ecs/entity/entity.html">ecs::entity</a> root, std::string_view name );</code></pre></div>

<p>Captures the subtree as a prefab document (external references become <code>null</code>, single root).</p>

#### `prefab::diff` {#function-prefab-diff}

<div class="function-declaration" id="function-prefab-diff-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<a href="prefab/prefab-diff.html">prefab_diff</a>&gt; diff(<span class="k">const</span> <a href="scene/scene.html">scene</a> &amp;source, <a href="../ecs/entity/entity.html">ecs::entity</a> root);</code></pre></div>

<p>Compares the instance with its loaded asset entry by entry (matched through <code><a href="components/prefab-link.html">prefab_link</a></code>). <code>NOT_A_PREFAB_INSTANCE</code>, <code>PREFAB_NOT_LOADED</code>.</p>

#### `prefab::instance_root_of` {#function-prefab-instance-root-of}

<div class="function-declaration" id="function-prefab-instance-root-of-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../ecs/entity/entity.html">ecs::entity</a> instance_root_of(<span class="k">const</span> <a href="scene/scene.html">scene</a> &amp;source, <a href="../ecs/entity/entity.html">ecs::entity</a> entity) <span class="k">noexcept</span>;</code></pre></div>

<p>The nearest ancestor (the entity included) carrying <code><a href="components/prefab-instance.html">prefab_instance</a></code>, or <code>NULL_ENTITY</code>.</p>

#### `prefab::instantiate` {#function-prefab-instantiate}

<div class="function-declaration" id="function-prefab-instantiate-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<a href="../ecs/entity/entity.html">ecs::entity</a>&gt; instantiate( <a href="scene/scene.html">scene</a> &amp;target, <span class="k">const</span> <a href="prefab/prefab-asset.html">prefab_asset</a> &amp;asset, <span class="k">const</span> <a href="scene/instantiate-options.html">instantiate_options</a> &amp;options = {} );</code></pre></div>

<p>Instantiates the prefab (every guid regenerated) and returns the root, which carries a <code><a href="components/prefab-instance.html">prefab_instance</a></code> bound to the asset; entities without a <code><a href="components/prefab-link.html">prefab_link</a></code> receive their entry&#x27;s guid.</p>

#### `prefab::instantiate_many` {#function-prefab-instantiate-many}

<div class="function-declaration" id="function-prefab-instantiate-many-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; instantiate_many( <a href="scene/scene.html">scene</a> &amp;target, <span class="k">const</span> <a href="prefab/prefab-asset.html">prefab_asset</a> &amp;asset, <a href="../core/types.html#symbol-u32">u32</a> count, <span class="k">const</span> <a href="scene/instantiate-options.html">instantiate_options</a> &amp;options, std::span&lt;<a href="../ecs/entity/entity.html">ecs::entity</a>&gt; outRoots );</code></pre></div>

<p><code>count</code> instances in one batch; <code>outRoots</code> receives the roots in order.</p>

#### `prefab::mark_instance` {#function-prefab-mark-instance}

<div class="function-declaration" id="function-prefab-mark-instance-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; mark_instance( <a href="scene/scene.html">scene</a> &amp;target, <a href="../ecs/entity/entity.html">ecs::entity</a> root, <a href="../assets/asset-ref/asset-ref.html">assets::asset_ref</a>&lt;<a href="prefab/prefab-asset.html">prefab_asset</a>&gt; asset );</code></pre></div>

<p>Marks a subtree as an instance of <code>asset</code>: <code><a href="components/prefab-instance.html">prefab_instance</a></code> on the root, <code><a href="components/prefab-link.html">prefab_link</a></code> with the current guid on every entity (guids are generated where missing).</p>

#### `prefab::refresh_instances` {#function-prefab-refresh-instances}

<div class="function-declaration" id="function-prefab-refresh-instances-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/types.html#symbol-u32">u32</a> refresh_instances( <a href="scene/scene.html">scene</a> &amp;target, <span class="k">const</span> <a href="../assets/asset-types.html#symbol-asset-id">assets::asset_id</a> &amp;prefab, <span class="k">bool</span> onlyUnmodified = true );</code></pre></div>

<p>Reverts every instance of the prefab (only the unmodified ones by default). Returns how many were reverted.</p>

#### `prefab::revert` {#function-prefab-revert}

<div class="function-declaration" id="function-prefab-revert-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; revert(<a href="scene/scene.html">scene</a> &amp;target, <a href="../ecs/entity/entity.html">ecs::entity</a> root);</code></pre></div>

<p>Replaces the instance with a fresh one at the same place, keeping the root&#x27;s guid, name and local transform. <code>NOT_A_PREFAB_INSTANCE</code>, <code>PREFAB_NOT_LOADED</code>, and <code>PREFAB_MULTIPLE_ROOTS</code> or <code>HIERARCHY_TOO_DEEP</code> when the current asset no longer fits at the instance&#x27;s place; the instance is left untouched on these errors.</p>

#### `prefab::unpack` {#function-prefab-unpack}

<div class="function-declaration" id="function-prefab-unpack-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; unpack(<a href="scene/scene.html">scene</a> &amp;target, <a href="../ecs/entity/entity.html">ecs::entity</a> root, <span class="k">bool</span> recursive = false);</code></pre></div>

<p>Removes the prefab markers of the subtree, stopping at nested instance roots unless recursive. <code>NOT_A_PREFAB_INSTANCE</code>.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="apply"></a>
<a id="build_document"></a>
<a id="conjure_declare_asset_type"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="diff"></a>
<a id="direct-header-dependencies"></a>
<a id="entity_diff"></a>
<a id="entity_diff_kind"></a>
<a id="entity_diff_kindadded"></a>
<a id="entity_diff_kindmodified"></a>
<a id="entity_diff_kindremoved"></a>
<a id="entity_diffassetguid"></a>
<a id="entity_diffcomponents"></a>
<a id="entity_diffinstanceentity"></a>
<a id="entity_diffkind"></a>
<a id="extension-points"></a>
<a id="instance_root_of"></a>
<a id="instantiate"></a>
<a id="instantiate_many"></a>
<a id="internals-and-algorithms"></a>
<a id="mark_instance"></a>
<a id="other-declarations"></a>
<a id="prefab_asset"></a>
<a id="prefab_assetarchive"></a>
<a id="prefab_assetdependencies"></a>
<a id="prefab_assetid"></a>
<a id="prefab_assetname"></a>
<a id="prefab_assetrootguid"></a>
<a id="prefab_builder"></a>
<a id="prefab_builderadd_component_json"></a>
<a id="prefab_builderadd_component_raw"></a>
<a id="prefab_builderadd_node"></a>
<a id="prefab_builderadd_tag"></a>
<a id="prefab_builderbuild"></a>
<a id="prefab_builderdestroy_images"></a>
<a id="prefab_builderis_reserved"></a>
<a id="prefab_buildermids"></a>
<a id="prefab_buildermnodes"></a>
<a id="prefab_buildermregistry"></a>
<a id="prefab_builderno_parent"></a>
<a id="prefab_buildernode"></a>
<a id="prefab_buildernode_component"></a>
<a id="prefab_buildernode_componentbytes"></a>
<a id="prefab_buildernode_componentid"></a>
<a id="prefab_buildernode_count"></a>
<a id="prefab_buildernodecomponents"></a>
<a id="prefab_buildernodename"></a>
<a id="prefab_buildernodeparentindex"></a>
<a id="prefab_buildernodetags"></a>
<a id="prefab_buildernodetransform"></a>
<a id="prefab_builderoperator"></a>
<a id="prefab_builderprefab_builder"></a>
<a id="prefab_builderprefab_builder_1"></a>
<a id="prefab_diff"></a>
<a id="prefab_diffempty"></a>
<a id="prefab_diffentries"></a>
<a id="prefabapply"></a>
<a id="prefabbuild_document"></a>
<a id="prefabdiff"></a>
<a id="prefabinstance_root_of"></a>
<a id="prefabinstantiate"></a>
<a id="prefabinstantiate_many"></a>
<a id="prefabmark_instance"></a>
<a id="prefabrefresh_instances"></a>
<a id="prefabrevert"></a>
<a id="prefabunpack"></a>
<a id="refresh_instances"></a>
<a id="revert"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="unpack"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->

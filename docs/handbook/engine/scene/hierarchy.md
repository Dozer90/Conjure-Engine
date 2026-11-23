---
kind: header
---

[Engine](../../engine.md) → [Scene](../scene.md)

# hierarchy.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/scene/hierarchy.h` |
| API ↗ | [Scenes, hierarchy and prefabs](../../../api/scene/index.md), [Scenes and the hierarchy](../../../api/scene/scenes-and-hierarchy.md) |

<p>Read-only tree queries over a world&#x27;s parent / children / <code><a href="components/depth-tag.html">depth_tag</a></code> components. Non-owning; a scene owns one and exposes it through <code>scene::tree()</code>. Every traversal is iterative (explicit stack), never recursive, and every query answers NULL / empty / 0 for a dead entity.</p>



## Classes

| File | Description |
|---|---|
| [`hierarchy`](hierarchy/hierarchy.md) |  |

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
<a id="hierarchy"></a>
<a id="hierarchychildren_of"></a>
<a id="hierarchycollect_subtree"></a>
<a id="hierarchydepth_of"></a>
<a id="hierarchyfor_each_in_subtree"></a>
<a id="hierarchyhierarchy"></a>
<a id="hierarchyhierarchy_1"></a>
<a id="hierarchyids"></a>
<a id="hierarchyis_alive"></a>
<a id="hierarchyis_ancestor_of"></a>
<a id="hierarchymids"></a>
<a id="hierarchymworld"></a>
<a id="hierarchyoperator"></a>
<a id="hierarchyparent_of"></a>
<a id="hierarchyroot_of"></a>
<a id="hierarchysibling_index"></a>
<a id="hierarchysubtree_height"></a>
<a id="hierarchysubtree_size"></a>
<a id="hierarchyworld"></a>
<a id="internals-and-algorithms"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->

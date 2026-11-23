---
kind: header
---

[Engine](../../engine.md) → [Scene](../scene.md)

# guid_index.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/scene/guid_index.h` |
| API ↗ | [Scenes, hierarchy and prefabs](../../../api/scene/index.md), [Scenes and the hierarchy](../../../api/scene/scenes-and-hierarchy.md) |

<p>The guid -&gt; entity index of a scene. Main thread only; maintained by the scene&#x27;s guid observers and link pass. <code>A</code> collision keeps the first mapping, so duplicates are prevented at instantiation (validated against the index) rather than repaired here.</p>



## Classes

| File | Description |
|---|---|
| [`guid_index`](guid-index/guid-index.md) |  |

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
<a id="guid_index"></a>
<a id="guid_indexcapacity"></a>
<a id="guid_indexclear"></a>
<a id="guid_indexcontains"></a>
<a id="guid_indexerase"></a>
<a id="guid_indexfind"></a>
<a id="guid_indexguid_index"></a>
<a id="guid_indexguid_index_1"></a>
<a id="guid_indexinitial_capacity"></a>
<a id="guid_indexinsert"></a>
<a id="guid_indexmentries"></a>
<a id="guid_indexoperator"></a>
<a id="guid_indexrebind"></a>
<a id="guid_indexreserve"></a>
<a id="guid_indexsize"></a>
<a id="internals-and-algorithms"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->

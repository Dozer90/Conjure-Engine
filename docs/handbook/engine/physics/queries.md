---
kind: header
---

[Engine](../../engine.md) → [Physics](../physics.md)

# queries.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/physics/queries.h` |
| API ↗ | [Physics](../../../api/physics/index.md), [The physics world and queries](../../../api/physics/physics-world-and-queries.md) |

<p>Query inputs and outputs. Queries return entities, never Jolt ids, and write into caller memory.</p>

## Structs

| File | Description |
|---|---|
| [`box_shape_desc`](queries/box-shape-desc.md) |  |
| [`capsule_shape_desc`](queries/capsule-shape-desc.md) |  |
| [`query_filter`](queries/query-filter.md) | What a query may hit. |
| [`raycast_hit`](queries/raycast-hit.md) | One ray hit. |
| [`shape_cast_hit`](queries/shape-cast-hit.md) | The closest hit of a shape cast. |
| [`sphere_shape_desc`](queries/sphere-shape-desc.md) |  |

## Type Aliases

### `cast_shape_desc` {#symbol-cast-shape-desc}

```cpp
using cast_shape_desc = std::variant<sphere_shape_desc, box_shape_desc, capsule_shape_desc>;
```

The shape a <code>shape_cast</code> sweeps.

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="box_shape_desc"></a>
<a id="box_shape_deschalfextents"></a>
<a id="box_shape_descoperator"></a>
<a id="capsule_shape_desc"></a>
<a id="capsule_shape_deschalfheight"></a>
<a id="capsule_shape_descoperator"></a>
<a id="capsule_shape_descradius"></a>
<a id="cast_shape_desc"></a>
<a id="classes"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="other-declarations"></a>
<a id="query_filter"></a>
<a id="query_filterignore"></a>
<a id="query_filterincludestatic"></a>
<a id="query_filterincludetriggers"></a>
<a id="query_filterlayers"></a>
<a id="query_filteroperator"></a>
<a id="raycast_hit"></a>
<a id="raycast_hitdistance"></a>
<a id="raycast_hitentity"></a>
<a id="raycast_hitfraction"></a>
<a id="raycast_hitistrigger"></a>
<a id="raycast_hitlayer"></a>
<a id="raycast_hitnormal"></a>
<a id="raycast_hitoperator"></a>
<a id="raycast_hitpoint"></a>
<a id="shape_cast_hit"></a>
<a id="shape_cast_hitdistance"></a>
<a id="shape_cast_hitentity"></a>
<a id="shape_cast_hitfraction"></a>
<a id="shape_cast_hitlayer"></a>
<a id="shape_cast_hitnormal"></a>
<a id="shape_cast_hitoperator"></a>
<a id="shape_cast_hitpenetrationdepth"></a>
<a id="shape_cast_hitpoint"></a>
<a id="sphere_shape_desc"></a>
<a id="sphere_shape_descoperator"></a>
<a id="sphere_shape_descradius"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->

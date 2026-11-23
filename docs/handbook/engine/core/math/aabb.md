---
kind: header
---

[Engine](../../../engine.md) → [Core](../../core.md)

# aabb.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/core/math/aabb.h` |
| API ↗ | [Math](../../../../api/core/math.md) |

<p>Axis-aligned bounding box. The default box is inverted (min = +<code>FLT_MAX</code>, max = -<code>FLT_MAX</code>) so that expanding it with the first point makes it valid; <code>is_valid()</code> is <code>false</code> until then. Containment and intersection tests are inclusive: touching counts.</p>

## Structs

| File | Description |
|---|---|
| [`aabb`](aabb/aabb.md) | Axis-aligned bounding box. The default box is inverted (min = +<code>FLT_MAX</code>, max = -<code>FLT_MAX</code>) so that expanding it with the first point makes it valid; <code>is_valid()</code> is <code>false</code> until then. Containment and intersection tests are inclusive: touching counts. |
| [`std::formatter<conjure::math::aabb>`](aabb/std-formatter-conjure-math-aabb.md) |  |

## Functions

### Public

#### `nearly_equal` {#function-nearly-equal}

<div class="function-declaration" id="function-nearly-equal-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">inline</span> <span class="k">bool</span> nearly_equal(
    <span class="k">const</span> <a href="aabb/aabb.html">aabb</a> &amp;a,
    <span class="k">const</span> <a href="aabb/aabb.html">aabb</a> &amp;b,
    <a href="../types.html#symbol-f32">f32</a> epsilon = EPSILON) <span class="k">noexcept</span>;</code></pre></div>

<p>True when both corners are within epsilon.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="aabb"></a>
<a id="aabbcenter"></a>
<a id="aabbclosest_point"></a>
<a id="aabbcontains"></a>
<a id="aabbcorners"></a>
<a id="aabbdistance_squared"></a>
<a id="aabbexpand"></a>
<a id="aabbextents"></a>
<a id="aabbfrom_center_extents"></a>
<a id="aabbfrom_points"></a>
<a id="aabbinflated"></a>
<a id="aabbintersection"></a>
<a id="aabbintersects"></a>
<a id="aabbis_valid"></a>
<a id="aabbmax"></a>
<a id="aabbmin"></a>
<a id="aabboperator"></a>
<a id="aabbsize"></a>
<a id="aabbsurface_area"></a>
<a id="aabbtransformed"></a>
<a id="aabbvolume"></a>
<a id="classes"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="nearly_equal"></a>
<a id="std"></a>
<a id="stdformat"></a>
<a id="stdformatterconjuremathaabb"></a>
<a id="stdformatterconjuremathaabbformat"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->

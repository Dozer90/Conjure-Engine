---
kind: header
---

[Engine](../../../engine.md) → [Core](../../core.md)

# plane.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/core/math/plane.h` |
| API ↗ | [Math](../../../../api/core/math.md) |

<p>An infinite <code><a href="plane/plane.html">plane</a></code> in the form <code>dot(normal, point)</code> + distance = 0. The positive half-space is the side the normal points to. Normalise before trusting signed distances as metric distances; frustum extraction and the factories below already do.</p>

## Structs

| File | Description |
|---|---|
| [`plane`](plane/plane.md) | An infinite <code><a href="plane/plane.html">plane</a></code> in the form <code>dot(normal, point)</code> + distance = 0. The positive half-space is the side the normal points to. Normalise before trusting signed distances as metric distances; frustum extraction and the factories below already do. |
| [`std::formatter<conjure::math::plane>`](plane/std-formatter-conjure-math-plane.md) |  |

## Functions

### Public

#### `nearly_equal` {#function-nearly-equal}

<div class="function-declaration" id="function-nearly-equal-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">inline</span> <span class="k">bool</span> nearly_equal(
    <span class="k">const</span> <a href="plane/plane.html">plane</a> &amp;a,
    <span class="k">const</span> <a href="plane/plane.html">plane</a> &amp;b,
    <a href="../types.html#symbol-f32">f32</a> epsilon = EPSILON) <span class="k">noexcept</span>;</code></pre></div>

<p>True when normal and distance are each within epsilon.</p>

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
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="nearly_equal"></a>
<a id="plane"></a>
<a id="planedistance"></a>
<a id="planefrom_point_normal"></a>
<a id="planefrom_points"></a>
<a id="planenormal"></a>
<a id="planenormalize"></a>
<a id="planenormalized"></a>
<a id="planeoperator"></a>
<a id="planeproject"></a>
<a id="planesigned_distance"></a>
<a id="std"></a>
<a id="stdformat"></a>
<a id="stdformatterconjuremathplane"></a>
<a id="stdformatterconjuremathplaneformat"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->

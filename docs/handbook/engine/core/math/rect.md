---
kind: header
---

[Engine](../../../engine.md) → [Core](../../core.md)

# rect.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/core/math/rect.h` |
| API ↗ | [Math](../../../../api/core/math.md) |

<p><code>A</code> float rectangle given by its min and max corners (y may point either way; the struct only assumes min &lt;= max). The default rectangle is empty at the origin. Containment and intersection are inclusive: touching counts.</p>

## Structs

| File | Description |
|---|---|
| [`rect`](rect/rect.md) | <code>A</code> float rectangle given by its min and max corners (y may point either way; the struct only assumes min &lt;= max). The default rectangle is empty at the origin. Containment and intersection are inclusive: touching counts. |
| [`std::formatter<conjure::math::rect>`](rect/std-formatter-conjure-math-rect.md) |  |
| [`std::formatter<conjure::math::urect>`](rect/std-formatter-conjure-math-urect.md) |  |
| [`urect`](rect/urect.md) | An unsigned pixel rectangle (viewports, scissors, texture regions): top-left origin plus size. Pixel (x, y) is covered when x &lt; <code>right()</code> and y &lt; <code>bottom()</code>, so the right and bottom edges are exclusive. Invariant: x + width and y + height must not exceed 0xFFFFFFFF (the edges must be representable); <code>right()</code> and <code>bottom()</code> assert when they would wrap. |

## Functions

### Public

#### `nearly_equal` {#function-nearly-equal}

<div class="function-declaration" id="function-nearly-equal-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">inline</span> <span class="k">bool</span> nearly_equal(
    <span class="k">const</span> <a href="rect/rect.html">rect</a> &amp;a,
    <span class="k">const</span> <a href="rect/rect.html">rect</a> &amp;b,
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
<a id="classes"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="nearly_equal"></a>
<a id="rect"></a>
<a id="rectarea"></a>
<a id="rectcenter"></a>
<a id="rectclosest_point"></a>
<a id="rectcontains"></a>
<a id="rectexpand"></a>
<a id="rectfrom_center_extents"></a>
<a id="rectfrom_points"></a>
<a id="rectfrom_position_size"></a>
<a id="rectheight"></a>
<a id="rectinflated"></a>
<a id="rectintersection"></a>
<a id="rectintersects"></a>
<a id="rectis_empty"></a>
<a id="rectis_valid"></a>
<a id="rectmax"></a>
<a id="rectmin"></a>
<a id="rectoperator"></a>
<a id="rectsize"></a>
<a id="recttranslated"></a>
<a id="rectwidth"></a>
<a id="std"></a>
<a id="stdformat"></a>
<a id="stdformatterconjuremathrect"></a>
<a id="stdformatterconjuremathrectformat"></a>
<a id="stdformatterconjuremathurect"></a>
<a id="stdformatterconjuremathurectformat"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="urect"></a>
<a id="urectarea"></a>
<a id="urectbottom"></a>
<a id="urectcontains"></a>
<a id="urectheight"></a>
<a id="urectintersection"></a>
<a id="urectintersects"></a>
<a id="urectis_empty"></a>
<a id="urectoperator"></a>
<a id="urectright"></a>
<a id="urectto_rect"></a>
<a id="urectwidth"></a>
<a id="urectx"></a>
<a id="urecty"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->

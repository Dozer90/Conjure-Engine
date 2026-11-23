---
kind: header
---

[Engine](../../../engine.md) → [Core](../../core.md)

# transform.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/core/math/transform.h` |
| API ↗ | [Math](../../../../api/core/math.md) |

<p>Position, rotation and scale (TRS). Right-handed, <code>Y</code> up, -<code>Z</code> forward. Trivially copyable and exactly 10 floats (position xyz, rotation xyzw, scale xyz) so the <code>C</code># side mirrors it. <code>A</code> TRS cannot represent shear, so composing or inverting transforms with non-uniform scale under rotation is an approximation; the documented behaviours below say exactly which identities hold.</p>

## Structs

| File | Description |
|---|---|
| [`std::formatter<conjure::math::transform>`](transform/std-formatter-conjure-math-transform.md) |  |
| [`transform`](transform/transform.md) | Position, rotation and scale (TRS). Right-handed, <code>Y</code> up, -<code>Z</code> forward. Trivially copyable and exactly 10 floats (position xyz, rotation xyzw, scale xyz) so the <code>C</code># side mirrors it. <code>A</code> TRS cannot represent shear, so composing or inverting transforms with non-uniform scale under rotation is an approximation; the documented behaviours below say exactly which identities hold. |

## Functions

### Public

#### `nearly_equal` {#function-nearly-equal}

<div class="function-declaration" id="function-nearly-equal-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">inline</span> <span class="k">bool</span> nearly_equal(
    <span class="k">const</span> <a href="transform/transform.html">transform</a> &amp;a,
    <span class="k">const</span> <a href="transform/transform.html">transform</a> &amp;b,
    <a href="../types.html#symbol-f32">f32</a> epsilon = EPSILON) <span class="k">noexcept</span>;</code></pre></div>

<p>True when position, rotation (component-wise) and scale are each within epsilon.</p>

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
<a id="std"></a>
<a id="stdformat"></a>
<a id="stdformatterconjuremathtransform"></a>
<a id="stdformatterconjuremathtransformformat"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="transform"></a>
<a id="transformforward"></a>
<a id="transformfrom_matrix"></a>
<a id="transformfrom_position"></a>
<a id="transformfrom_rotation"></a>
<a id="transformfrom_scale"></a>
<a id="transforminverse"></a>
<a id="transforminverse_transform_direction"></a>
<a id="transforminverse_transform_point"></a>
<a id="transformlerp"></a>
<a id="transformlook_at"></a>
<a id="transformoperator"></a>
<a id="transformoperator_1"></a>
<a id="transformposition"></a>
<a id="transformright"></a>
<a id="transformrotation"></a>
<a id="transformscale"></a>
<a id="transformto_matrix"></a>
<a id="transformtransform_direction"></a>
<a id="transformtransform_point"></a>
<a id="transformup"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->

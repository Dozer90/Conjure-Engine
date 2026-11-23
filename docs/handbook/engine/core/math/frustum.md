---
kind: header
---

[Engine](../../../engine.md) → [Core](../../core.md)

# frustum.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/core/math/frustum.h` |
| API ↗ | [Math](../../../../api/core/math.md) |

<p>Six inward-facing, normalised planes describing a view volume. Points with a non-negative signed distance to every plane are inside. Built from a view-projection matrix with forward 0..1 depth (near plane at depth 0).</p>

## Structs

| File | Description |
|---|---|
| [`frustum`](frustum/frustum.md) | Six inward-facing, normalised planes describing a view volume. Points with a non-negative signed distance to every plane are inside. Built from a view-projection matrix with forward 0..1 depth (near plane at depth 0). |

## Enumerations

### `frustum_plane` {#symbol-frustum-plane}

```cpp
enum class frustum_plane : u8 {
    LEFT = 0,
    RIGHT,
    BOTTOM,
    TOP,
    NEAR_PLANE,
    FAR_PLANE,
    COUNT
};
```

Index of each <code><a href="frustum/frustum.html">frustum</a></code> plane in <code>frustum::planes</code>. NEAR and FAR are Windows macros, hence the <code>_PLANE</code> suffix.

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-FRUSTUM_PLANE_COUNT"></a><code>FRUSTUM_PLANE_COUNT</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../types.html#symbol-usize">usize</a></code> | <code class="signature">static_<wbr>cast&lt;<wbr><a href="../types.html#symbol-usize">usize</a>&gt;(frustum_<wbr>plane::<wbr>COUNT)</code> |  |

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
<a id="frustum"></a>
<a id="frustum_plane"></a>
<a id="frustum_plane_count"></a>
<a id="frustum_planebottom"></a>
<a id="frustum_planecount"></a>
<a id="frustum_planefar_plane"></a>
<a id="frustum_planeleft"></a>
<a id="frustum_planenear_plane"></a>
<a id="frustum_planeright"></a>
<a id="frustum_planetop"></a>
<a id="frustumcontains"></a>
<a id="frustumcorners"></a>
<a id="frustumfrom_view_projection"></a>
<a id="frustumget"></a>
<a id="frustumintersects"></a>
<a id="frustumintersects_sphere"></a>
<a id="frustumoperator"></a>
<a id="frustumplanes"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="other-declarations"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->

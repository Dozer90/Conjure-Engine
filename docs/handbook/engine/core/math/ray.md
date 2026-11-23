---
kind: header
---

[Engine](../../../engine.md) → [Core](../../core.md)

# ray.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/core/math/ray.h` |
| API ↗ | [Math](../../../../api/core/math.md) |

<p><code>A</code> half-line origin + t * direction for t &gt;= 0. Callers keep direction normalised (the factories do) so that t is a distance. Every intersection returns the smallest t &gt;= 0 at which the <code><a href="ray/ray.html">ray</a></code> is on or inside the shape, or nullopt when it misses; a <code><a href="ray/ray.html">ray</a></code> starting inside a closed volume therefore hits at t = 0.</p>

## Structs

| File | Description |
|---|---|
| [`ray`](ray/ray.md) | <code>A</code> half-line origin + t * direction for t &gt;= 0. Callers keep direction normalised (the factories do) so that t is a distance. Every intersection returns the smallest t &gt;= 0 at which the <code><a href="ray/ray.html">ray</a></code> is on or inside the shape, or nullopt when it misses; a <code><a href="ray/ray.html">ray</a></code> starting inside a closed volume therefore hits at t = 0. |

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
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="ray"></a>
<a id="rayat"></a>
<a id="raydirection"></a>
<a id="rayfrom_points"></a>
<a id="rayfrom_viewport"></a>
<a id="rayintersect"></a>
<a id="rayintersect_plane"></a>
<a id="rayintersect_sphere"></a>
<a id="rayintersect_triangle"></a>
<a id="rayoperator"></a>
<a id="rayorigin"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->

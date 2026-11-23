---
kind: header
---

[Engine](../../engine.md) → [Physics](../physics.md)

# physics_debug_draw.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/physics/physics_debug_draw.h` |
| API ↗ | [Physics](../../../api/physics/index.md), [The physics world and queries](../../../api/physics/physics-world-and-queries.md) |

<p>Receives the primitives of <code>physics_world::debug_draw</code>.</p>



## Classes

| File | Description |
|---|---|
| [`physics_debug_draw`](physics-debug-draw/physics-debug-draw.md) | Receives the primitives of <code>physics_world::debug_draw</code>. |

## Enumerations

### `physics_debug_draw_flags` {#symbol-physics-debug-draw-flags}

```cpp
enum class physics_debug_draw_flags : u16 {
    NONE = 0,
    SHAPES = 1 << 0,
    WIREFRAME = 1 << 1,
    BOUNDING_BOXES = 1 << 2,
    CENTER_OF_MASS = 1 << 3,
    VELOCITIES = 1 << 4,
    SLEEP_STATE = 1 << 5,
    CHARACTERS = 1 << 6,
    CONTACTS = 1 << 7
};
```

What <code><a href="../renderer/debug-draw/debug-draw.html">debug_draw</a></code> renders.

## Macros

### `CONJURE_ENUM_FLAGS` {#symbol-conjure-enum-flags}

```cpp
CONJURE_ENUM_FLAGS(physics_debug_draw_flags)
```

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="conjure_enum_flags"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="other-declarations"></a>
<a id="physics_debug_draw"></a>
<a id="physics_debug_draw_flags"></a>
<a id="physics_debug_draw_flagsbounding_boxes"></a>
<a id="physics_debug_draw_flagscenter_of_mass"></a>
<a id="physics_debug_draw_flagscharacters"></a>
<a id="physics_debug_draw_flagscontacts"></a>
<a id="physics_debug_draw_flagsnone"></a>
<a id="physics_debug_draw_flagsshapes"></a>
<a id="physics_debug_draw_flagssleep_state"></a>
<a id="physics_debug_draw_flagsvelocities"></a>
<a id="physics_debug_draw_flagswireframe"></a>
<a id="physics_debug_drawline"></a>
<a id="physics_debug_drawoperator"></a>
<a id="physics_debug_drawphysics_debug_draw"></a>
<a id="physics_debug_drawphysics_debug_draw_1"></a>
<a id="physics_debug_drawtext"></a>
<a id="physics_debug_drawtriangle"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->

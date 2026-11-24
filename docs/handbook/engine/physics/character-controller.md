---
kind: header
---

[Engine](../../engine.md) → [Physics](../physics.md)

# character_controller.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/physics/character_controller.h` |
| API ↗ | [Bodies, colliders and characters](../../../api/physics/bodies-and-colliders.md), [Physics](../../../api/physics/index.md) |

<p>&quot;<code>conjure.physics.<a href="character-controller/character-controller.html">character_controller</a></code>&quot;: a capsule of the given radius and total height whose feet sit at the entity origin. 68 bytes (entity fields are 4-aligned, reflection rule).</p>

## Structs

| File | Description |
|---|---|
| [`character_controller`](character-controller/character-controller.md) | &quot;<code>conjure.physics.<a href="character-controller/character-controller.html">character_controller</a></code>&quot;: a capsule of the given radius and total height whose feet sit at the entity origin. 68 bytes (entity fields are 4-aligned, reflection rule). |

## Enumerations

### `character_flags` {#symbol-character-flags}

```cpp
enum class character_flags : u8 {
    NONE = 0,
    STICK_TO_FLOOR = 1 << 0,     // ExtendedUpdateSettings::mStickToFloorStepDown = (0, -stepHeight, 0)
    WALK_STAIRS = 1 << 1,        // ExtendedUpdateSettings::mWalkStairsStepUp = (0, stepHeight, 0)
    APPLY_GRAVITY = 1 << 2       // integrate settings.gravity into velocity while not ON_GROUND
};
```

Character behaviour switches.

### `ground_state` {#symbol-ground-state}

```cpp
enum class ground_state : u8 {
    ON_GROUND,
    ON_STEEP_GROUND,
    NOT_SUPPORTED,
    IN_AIR
};
```

Where the character stands (<code>CharacterBase::EGroundState</code>).

## Macros

### `CONJURE_COMPONENT` {#symbol-conjure-component}

```cpp
CONJURE_COMPONENT(conjure::physics::character_controller, "conjure.physics.character_controller")
```

### `CONJURE_ENUM_FLAGS` {#symbol-conjure-enum-flags}

```cpp
CONJURE_ENUM_FLAGS(character_flags)
```

### `CONJURE_REFLECT_ENUM` {#symbol-conjure-reflect-enum}

```cpp
CONJURE_REFLECT_ENUM(conjure::physics::ground_state, "conjure.physics.ground_state", ON_GROUND, ON_STEEP_GROUND, NOT_SUPPORTED, IN_AIR)
```

### `CONJURE_REFLECT_FLAGS` {#symbol-conjure-reflect-flags}

```cpp
CONJURE_REFLECT_FLAGS(conjure::physics::character_flags, "conjure.physics.character_flags", NONE, STICK_TO_FLOOR, WALK_STAIRS, APPLY_GRAVITY)
```

### `CONJURE_REFLECT_STRUCT` {#symbol-conjure-reflect-struct}

```cpp
CONJURE_REFLECT_STRUCT(conjure::physics::character_controller, "conjure.physics.character_controller", CONJURE_FIELD(radius, .min = 0.0), CONJURE_FIELD(height, .min = 0.0), CONJURE_FIELD(maxSlopeAngleRadians, .min = 0.0, .max = std::numbers::pi / 2.0, .unit = conjure::core::reflect::field_unit::RADIANS), CONJURE_FIELD(stepHeight, .min = 0.0), CONJURE_FIELD(mass, .min = 0.0), CONJURE_FIELD(maxStrength, .min = 0.0), CONJURE_FIELD(padding, .min = 0.0), CONJURE_FIELD(layer, .min = 0.0, .max = 31.0), CONJURE_FIELD(flags), CONJURE_FIELD(velocity), CONJURE_FIELD(ground, .readOnly = true), CONJURE_FIELD(groundNormal, .readOnly = true), CONJURE_FIELD(groundEntity, .readOnly = true))
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
<a id="character_controller"></a>
<a id="character_controllerflags"></a>
<a id="character_controllerground"></a>
<a id="character_controllergroundentity"></a>
<a id="character_controllergroundnormal"></a>
<a id="character_controllerheight"></a>
<a id="character_controllerlayer"></a>
<a id="character_controllermass"></a>
<a id="character_controllermaxslopeangledegrees"></a>
<a id="character_controllermaxstrength"></a>
<a id="character_controlleroperator"></a>
<a id="character_controllerpadding"></a>
<a id="character_controllerradius"></a>
<a id="character_controllerstepheight"></a>
<a id="character_controllervelocity"></a>
<a id="character_flags"></a>
<a id="character_flagsapply_gravity"></a>
<a id="character_flagsnone"></a>
<a id="character_flagsstick_to_floor"></a>
<a id="character_flagswalk_stairs"></a>
<a id="classes"></a>
<a id="conjure_component"></a>
<a id="conjure_enum_flags"></a>
<a id="conjure_reflect_enum"></a>
<a id="conjure_reflect_flags"></a>
<a id="conjure_reflect_struct"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="ground_state"></a>
<a id="ground_statein_air"></a>
<a id="ground_statenot_supported"></a>
<a id="ground_stateon_ground"></a>
<a id="ground_stateon_steep_ground"></a>
<a id="internals-and-algorithms"></a>
<a id="other-declarations"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->

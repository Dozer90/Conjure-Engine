---
kind: header
---

[Engine](../../engine.md) → [Physics](../physics.md)

# components.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/physics/components.h` |
| API ↗ | [Bodies, colliders and characters](../../../api/physics/bodies-and-colliders.md), [Physics](../../../api/physics/index.md) |

<p>The body components. Plain reflected structs with camelCase fields named &quot;<code>conjure.physics</code>.&lt;struct name&gt;&quot;; registered by <code>physics::register_components</code> (<code>register.h</code>). Jolt ids live only inside <code><a href="physics-world/physics-world.html">physics_world</a></code>, so components copy into prefabs, archives and <code>world_hash</code> without Jolt knowledge. Fields marked [runtime] are read-only in the inspector but serialised (a saved scene reopens with its velocities); the <code>C</code># mirrors of restate these field lists <code><a href="../core/types.html#symbol-byte">byte</a></code> for <code><a href="../core/types.html#symbol-byte">byte</a></code>.</p>

## Structs

| File | Description |
|---|---|
| [`box_collider`](components/box-collider.md) | Axis-aligned box in entity space. &quot;<code>conjure.physics.<a href="components/box-collider.html">box_collider</a></code>&quot;. |
| [`capsule_collider`](components/capsule-collider.md) | Capsule along the entity&#x27;s <code>Y</code> axis; total height 2 * (<code>halfHeight</code> + radius). &quot;<code>conjure.physics.<a href="components/capsule-collider.html">capsule_collider</a></code>&quot;. |
| [`convex_collider`](components/convex-collider.md) | Convex hull of a collision mesh asset. &quot;<code>conjure.physics.<a href="components/convex-collider.html">convex_collider</a></code>&quot;. |
| [`mesh_collider`](components/mesh-collider.md) | Triangle mesh of a collision mesh asset; <code>STATIC</code> and <code>KINEMATIC</code> bodies only. &quot;<code>conjure.physics.<a href="components/mesh-collider.html">mesh_collider</a></code>&quot;. |
| [`physics_material`](components/physics-material.md) | Optional surface properties; combined as <code>sqrt(f1 * f2)</code> and <code>max(r1, r2)</code>. &quot;<code>conjure.physics.<a href="components/physics-material.html">physics_material</a></code>&quot;. |
| [`rigid_body`](components/rigid-body.md) | <code>A</code> rigid body. &quot;<code>conjure.physics.<a href="components/rigid-body.html">rigid_body</a></code>&quot;, 52 bytes, 4-aligned; mirrored by <code>C</code># RigidBodyData. |
| [`sphere_collider`](components/sphere-collider.md) | Sphere in entity space. &quot;<code>conjure.physics.<a href="components/sphere-collider.html">sphere_collider</a></code>&quot;. |

## Enumerations

### `motion_type` {#symbol-motion-type}

```cpp
enum class motion_type : u8 {
    STATIC,
    KINEMATIC,
    DYNAMIC
};
```

How a body moves. <code>STATIC</code> never moves, <code>KINEMATIC</code> follows its transform or velocity and pushes dynamic bodies, <code>DYNAMIC</code> is simulated.

### `rigid_body_flags` {#symbol-rigid-body-flags}

```cpp
enum class rigid_body_flags : u16 {
    NONE = 0,
    CONTINUOUS_COLLISION = 1 << 0,            // EMotionQuality::LinearCast
    KINEMATIC_MOVES_WITH_TRANSFORM = 1 << 1,  // kinematic follows local_transform; else velocity driven
    ALLOW_SLEEPING = 1 << 2,
    START_ASLEEP = 1 << 3,                    // created deactivated
    LOCK_ROTATION = 1 << 4,                   // translation only
    LOCK_ROTATION_X = 1 << 5,
    LOCK_ROTATION_Y = 1 << 6,
    LOCK_ROTATION_Z = 1 << 7,
    LOCK_TRANSLATION_X = 1 << 8,
    LOCK_TRANSLATION_Y = 1 << 9,
    LOCK_TRANSLATION_Z = 1 << 10,
    USE_GYROSCOPIC_FORCE = 1 << 11
};
```

Per-body switches.

## Forward Declarations

### `collision_mesh_asset` {#symbol-collision-mesh-asset}

```cpp
class collision_mesh_asset;
```

## Macros

### `CONJURE_COMPONENT` {#symbol-conjure-component}

```cpp
CONJURE_COMPONENT(conjure::physics::rigid_body, "conjure.physics.rigid_body")
```

### `CONJURE_ENUM_FLAGS` {#symbol-conjure-enum-flags}

```cpp
CONJURE_ENUM_FLAGS(rigid_body_flags)
```

### `CONJURE_REFLECT_ENUM` {#symbol-conjure-reflect-enum}

```cpp
CONJURE_REFLECT_ENUM(conjure::physics::motion_type, "conjure.physics.motion_type", STATIC, KINEMATIC, DYNAMIC)
```

### `CONJURE_REFLECT_FLAGS` {#symbol-conjure-reflect-flags}

```cpp
CONJURE_REFLECT_FLAGS(conjure::physics::rigid_body_flags, "conjure.physics.rigid_body_flags", NONE, CONTINUOUS_COLLISION, KINEMATIC_MOVES_WITH_TRANSFORM, ALLOW_SLEEPING, START_ASLEEP, LOCK_ROTATION, LOCK_ROTATION_X, LOCK_ROTATION_Y, LOCK_ROTATION_Z, LOCK_TRANSLATION_X, LOCK_TRANSLATION_Y, LOCK_TRANSLATION_Z, USE_GYROSCOPIC_FORCE)
```

### `CONJURE_REFLECT_STRUCT` {#symbol-conjure-reflect-struct}

```cpp
CONJURE_REFLECT_STRUCT(conjure::physics::rigid_body, "conjure.physics.rigid_body", CONJURE_FIELD(motionType), CONJURE_FIELD(layer, .min = 0.0, .max = 31.0), CONJURE_FIELD(flags), CONJURE_FIELD(mass, .min = 0.0), CONJURE_FIELD(linearDamping, .min = 0.0), CONJURE_FIELD(angularDamping, .min = 0.0), CONJURE_FIELD(gravityFactor), CONJURE_FIELD(maxLinearVelocity, .min = 0.0), CONJURE_FIELD(linearVelocity, .readOnly = true), CONJURE_FIELD(angularVelocity, .readOnly = true), CONJURE_FIELD(isSleeping, .readOnly = true))
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
<a id="box_collider"></a>
<a id="box_colliderhalfextents"></a>
<a id="box_collideristrigger"></a>
<a id="box_collideroffset"></a>
<a id="box_collideroperator"></a>
<a id="box_colliderrotation"></a>
<a id="capsule_collider"></a>
<a id="capsule_colliderhalfheight"></a>
<a id="capsule_collideristrigger"></a>
<a id="capsule_collideroffset"></a>
<a id="capsule_collideroperator"></a>
<a id="capsule_colliderradius"></a>
<a id="capsule_colliderrotation"></a>
<a id="classes"></a>
<a id="collision_mesh_asset"></a>
<a id="conjure_component"></a>
<a id="conjure_enum_flags"></a>
<a id="conjure_reflect_enum"></a>
<a id="conjure_reflect_flags"></a>
<a id="conjure_reflect_struct"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="convex_collider"></a>
<a id="convex_colliderconvexradius"></a>
<a id="convex_collideristrigger"></a>
<a id="convex_collidermesh"></a>
<a id="convex_collideroffset"></a>
<a id="convex_colliderrotation"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="mesh_collider"></a>
<a id="mesh_collideristrigger"></a>
<a id="mesh_collidermesh"></a>
<a id="motion_type"></a>
<a id="motion_typedynamic"></a>
<a id="motion_typekinematic"></a>
<a id="motion_typestatic"></a>
<a id="other-declarations"></a>
<a id="physics_material"></a>
<a id="physics_materialfriction"></a>
<a id="physics_materialoperator"></a>
<a id="physics_materialrestitution"></a>
<a id="rigid_body"></a>
<a id="rigid_body_flags"></a>
<a id="rigid_body_flagsallow_sleeping"></a>
<a id="rigid_body_flagscontinuous_collision"></a>
<a id="rigid_body_flagskinematic_moves_with_transform"></a>
<a id="rigid_body_flagslock_rotation"></a>
<a id="rigid_body_flagslock_rotation_x"></a>
<a id="rigid_body_flagslock_rotation_y"></a>
<a id="rigid_body_flagslock_rotation_z"></a>
<a id="rigid_body_flagslock_translation_x"></a>
<a id="rigid_body_flagslock_translation_y"></a>
<a id="rigid_body_flagslock_translation_z"></a>
<a id="rigid_body_flagsnone"></a>
<a id="rigid_body_flagsstart_asleep"></a>
<a id="rigid_body_flagsuse_gyroscopic_force"></a>
<a id="rigid_bodyangulardamping"></a>
<a id="rigid_bodyangularvelocity"></a>
<a id="rigid_bodyflags"></a>
<a id="rigid_bodygravityfactor"></a>
<a id="rigid_bodyissleeping"></a>
<a id="rigid_bodylayer"></a>
<a id="rigid_bodylineardamping"></a>
<a id="rigid_bodylinearvelocity"></a>
<a id="rigid_bodymass"></a>
<a id="rigid_bodymaxlinearvelocity"></a>
<a id="rigid_bodymotiontype"></a>
<a id="rigid_bodyoperator"></a>
<a id="sphere_collider"></a>
<a id="sphere_collideristrigger"></a>
<a id="sphere_collideroffset"></a>
<a id="sphere_collideroperator"></a>
<a id="sphere_colliderradius"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->

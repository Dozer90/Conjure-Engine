#pragma once

#include "conjure/assets/asset_ref.h"
#include "conjure/core/enum_flags.h"
#include "conjure/core/math/glm_types.h"
#include "conjure/core/reflect/reflect.h"
#include "conjure/core/types.h"
#include "conjure/ecs/component.h"

#include <cstddef>
#include <type_traits>

// The body components (docs/design/14_physics.md 2.3, P7). Plain reflected structs with camelCase fields
// (R31) named "conjure.physics.<struct name>"; registered by physics::register_components (register.h). Jolt ids
// live only inside physics_world, so components copy into prefabs, archives and world_hash without Jolt
// knowledge. Fields marked [runtime] are read-only in the inspector but serialised (a saved scene reopens
// with its velocities); the C# mirrors of 15_bindings.md 2.6 restate these field lists byte for byte (R48).

namespace conjure::physics {

    class collision_mesh_asset;

    /// <summary>
    /// How a body moves. STATIC never moves, KINEMATIC follows its transform or velocity and pushes
    /// dynamic bodies, DYNAMIC is simulated.
    /// </summary>
    enum class motion_type : u8 {
        STATIC,
        KINEMATIC,
        DYNAMIC
    };

    /// <summary>
    /// Per-body switches.
    /// </summary>
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
    CONJURE_ENUM_FLAGS(rigid_body_flags);

    /// <summary>
    /// A rigid body. "conjure.physics.rigid_body", 52 bytes, 4-aligned; mirrored by C# RigidBodyData.
    /// </summary>
    struct rigid_body {
        motion_type motionType = motion_type::DYNAMIC;
        u8 layer = 0;                                     // index into physics_layer_table
        rigid_body_flags flags = rigid_body_flags::ALLOW_SLEEPING;
        f32 mass = 1.0f;                                  // kg; 0 = derive from the shape volume
        f32 linearDamping = 0.05f;                        // 1/s
        f32 angularDamping = 0.05f;
        f32 gravityFactor = 1.0f;
        f32 maxLinearVelocity = 500.0f;                   // m/s
        math::vec3 linearVelocity = math::vec3(0.0f);     // [runtime] pulled each step; written before a step sets it
        math::vec3 angularVelocity = math::vec3(0.0f);    // [runtime] radians/s
        bool isSleeping = false;                          // [runtime]

        [[nodiscard]] bool operator==(const rigid_body &) const = default;
    };

    /// <summary>
    /// Axis-aligned box in entity space. "conjure.physics.box_collider".
    /// </summary>
    struct box_collider {
        math::vec3 halfExtents = math::vec3(0.5f);
        math::vec3 offset = math::vec3(0.0f);
        math::quat rotation = math::quat(1.0f, 0.0f, 0.0f, 0.0f);
        bool isTrigger = false;

        [[nodiscard]] bool operator==(const box_collider &) const = default;
    };

    /// <summary>
    /// Sphere in entity space. "conjure.physics.sphere_collider".
    /// </summary>
    struct sphere_collider {
        f32 radius = 0.5f;
        math::vec3 offset = math::vec3(0.0f);
        bool isTrigger = false;

        [[nodiscard]] bool operator==(const sphere_collider &) const = default;
    };

    /// <summary>
    /// Capsule along the entity's Y axis; total height 2 * (halfHeight + radius).
    /// "conjure.physics.capsule_collider".
    /// </summary>
    struct capsule_collider {
        f32 radius = 0.5f;
        f32 halfHeight = 0.5f;
        math::vec3 offset = math::vec3(0.0f);
        math::quat rotation = math::quat(1.0f, 0.0f, 0.0f, 0.0f);
        bool isTrigger = false;

        [[nodiscard]] bool operator==(const capsule_collider &) const = default;
    };

    /// <summary>
    /// Convex hull of a collision mesh asset. "conjure.physics.convex_collider".
    /// </summary>
    struct convex_collider {
        assets::asset_ref<collision_mesh_asset> mesh = {};
        math::vec3 offset = math::vec3(0.0f);
        math::quat rotation = math::quat(1.0f, 0.0f, 0.0f, 0.0f);
        f32 convexRadius = 0.05f;                         // ConvexHullShapeSettings inMaxConvexRadius
        bool isTrigger = false;
    };

    /// <summary>
    /// Triangle mesh of a collision mesh asset; STATIC and KINEMATIC bodies only.
    /// "conjure.physics.mesh_collider".
    /// </summary>
    struct mesh_collider {
        assets::asset_ref<collision_mesh_asset> mesh = {};
        bool isTrigger = false;
    };

    /// <summary>
    /// Optional surface properties; combined as sqrt(f1 * f2) and max(r1, r2). "conjure.physics.physics_material".
    /// </summary>
    struct physics_material {
        f32 friction = 0.5f;
        f32 restitution = 0.0f;

        [[nodiscard]] bool operator==(const physics_material &) const = default;
    };

    static_assert(sizeof(rigid_body) == 52, "rigid_body is the 52-byte C# RigidBodyData mirror (R48)");
    static_assert(alignof(rigid_body) == 4, "rigid_body is 4-aligned");
    static_assert(std::is_trivially_copyable_v<rigid_body>, "rigid_body must be trivially copyable");
    static_assert(sizeof(box_collider) == 44, "box_collider is the 44-byte BoxColliderData mirror");
    static_assert(sizeof(sphere_collider) == 20, "sphere_collider is the 20-byte SphereColliderData mirror");
    static_assert(sizeof(capsule_collider) == 40, "capsule_collider is the 40-byte CapsuleColliderData mirror");
    static_assert(sizeof(convex_collider) == 64, "convex_collider is the 64-byte ConvexColliderData mirror");
    static_assert(sizeof(mesh_collider) == 32, "mesh_collider is the 32-byte MeshColliderData mirror");
    static_assert(sizeof(physics_material) == 8, "physics_material is two floats");
    static_assert(std::is_trivially_copyable_v<convex_collider>, "convex_collider must be trivially copyable");
    static_assert(std::is_trivially_copyable_v<mesh_collider>, "mesh_collider must be trivially copyable");
}

CONJURE_REFLECT_ENUM(conjure::physics::motion_type, "conjure.physics.motion_type", STATIC, KINEMATIC, DYNAMIC);
CONJURE_REFLECT_FLAGS(conjure::physics::rigid_body_flags, "conjure.physics.rigid_body_flags",
    NONE,
    CONTINUOUS_COLLISION,
    KINEMATIC_MOVES_WITH_TRANSFORM,
    ALLOW_SLEEPING,
    START_ASLEEP,
    LOCK_ROTATION,
    LOCK_ROTATION_X,
    LOCK_ROTATION_Y,
    LOCK_ROTATION_Z,
    LOCK_TRANSLATION_X,
    LOCK_TRANSLATION_Y,
    LOCK_TRANSLATION_Z,
    USE_GYROSCOPIC_FORCE);

CONJURE_REFLECT_STRUCT(conjure::physics::rigid_body, "conjure.physics.rigid_body",
    CONJURE_FIELD(motionType),
    CONJURE_FIELD(layer, .min = 0.0, .max = 31.0),
    CONJURE_FIELD(flags),
    CONJURE_FIELD(mass, .min = 0.0),
    CONJURE_FIELD(linearDamping, .min = 0.0),
    CONJURE_FIELD(angularDamping, .min = 0.0),
    CONJURE_FIELD(gravityFactor),
    CONJURE_FIELD(maxLinearVelocity, .min = 0.0),
    CONJURE_FIELD(linearVelocity, .readOnly = true),
    CONJURE_FIELD(angularVelocity, .readOnly = true),
    CONJURE_FIELD(isSleeping, .readOnly = true));
CONJURE_REFLECT_STRUCT(conjure::physics::box_collider, "conjure.physics.box_collider",
    CONJURE_FIELD(halfExtents),
    CONJURE_FIELD(offset),
    CONJURE_FIELD(rotation),
    CONJURE_FIELD(isTrigger));
CONJURE_REFLECT_STRUCT(conjure::physics::sphere_collider, "conjure.physics.sphere_collider",
    CONJURE_FIELD(radius, .min = 0.0),
    CONJURE_FIELD(offset),
    CONJURE_FIELD(isTrigger));
CONJURE_REFLECT_STRUCT(conjure::physics::capsule_collider, "conjure.physics.capsule_collider",
    CONJURE_FIELD(radius, .min = 0.0),
    CONJURE_FIELD(halfHeight, .min = 0.0),
    CONJURE_FIELD(offset),
    CONJURE_FIELD(rotation),
    CONJURE_FIELD(isTrigger));
CONJURE_REFLECT_STRUCT(conjure::physics::convex_collider, "conjure.physics.convex_collider",
    CONJURE_FIELD(mesh),
    CONJURE_FIELD(offset),
    CONJURE_FIELD(rotation),
    CONJURE_FIELD(convexRadius, .min = 0.0),
    CONJURE_FIELD(isTrigger));
CONJURE_REFLECT_STRUCT(conjure::physics::mesh_collider, "conjure.physics.mesh_collider",
    CONJURE_FIELD(mesh),
    CONJURE_FIELD(isTrigger));
CONJURE_REFLECT_STRUCT(conjure::physics::physics_material, "conjure.physics.physics_material",
    CONJURE_FIELD(friction, .min = 0.0),
    CONJURE_FIELD(restitution, .min = 0.0));

CONJURE_COMPONENT(conjure::physics::rigid_body, "conjure.physics.rigid_body");
CONJURE_COMPONENT(conjure::physics::box_collider, "conjure.physics.box_collider");
CONJURE_COMPONENT(conjure::physics::sphere_collider, "conjure.physics.sphere_collider");
CONJURE_COMPONENT(conjure::physics::capsule_collider, "conjure.physics.capsule_collider");
CONJURE_COMPONENT(conjure::physics::convex_collider, "conjure.physics.convex_collider");
CONJURE_COMPONENT(conjure::physics::mesh_collider, "conjure.physics.mesh_collider");
CONJURE_COMPONENT(conjure::physics::physics_material, "conjure.physics.physics_material");

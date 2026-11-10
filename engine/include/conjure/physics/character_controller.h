#pragma once

#include "conjure/core/enum_flags.h"
#include "conjure/core/math/glm_types.h"
#include "conjure/core/reflect/reflect.h"
#include "conjure/core/types.h"
#include "conjure/ecs/component.h"
#include "conjure/ecs/entity.h"

#include <cstddef>
#include <type_traits>

// The character controller component (docs/design/14_physics.md 2.3, P13): a JPH::CharacterVirtual with
// an inner kinematic body, updated before the body step. Scripts write `velocity` in FIXED_UPDATE (or
// call physics_world::character_move); the pull writes the achieved velocity and the ground state back.
// A character_controller excludes rigid_body (CONFLICTING_BODY_COMPONENTS, the character wins).

namespace conjure::physics {

    /// <summary>
    /// Where the character stands (CharacterBase::EGroundState).
    /// </summary>
    enum class ground_state : u8 {
        ON_GROUND,
        ON_STEEP_GROUND,
        NOT_SUPPORTED,
        IN_AIR
    };

    /// <summary>
    /// Character behaviour switches.
    /// </summary>
    enum class character_flags : u8 {
        NONE = 0,
        STICK_TO_FLOOR = 1 << 0,     // ExtendedUpdateSettings::mStickToFloorStepDown = (0, -stepHeight, 0)
        WALK_STAIRS = 1 << 1,        // ExtendedUpdateSettings::mWalkStairsStepUp = (0, stepHeight, 0)
        APPLY_GRAVITY = 1 << 2       // integrate settings.gravity into velocity while not ON_GROUND
    };
    CONJURE_ENUM_FLAGS(character_flags);

    /// <summary>
    /// "conjure.physics.character_controller": a capsule of the given radius and total height whose
    /// feet sit at the entity origin. 68 bytes (entity fields are 4-aligned, 01_ecs.md reflection rule).
    /// </summary>
    struct character_controller {
        f32 radius = 0.3f;
        f32 height = 1.8f;                                // total capsule height
        f32 maxSlopeAngleDegrees = 45.0f;
        f32 stepHeight = 0.4f;
        f32 mass = 70.0f;                                 // push on dynamic bodies
        f32 maxStrength = 100.0f;
        f32 padding = 0.02f;                              // CharacterVirtualSettings::mCharacterPadding
        u8 layer = 0;
        character_flags flags = character_flags::STICK_TO_FLOOR | character_flags::WALK_STAIRS
          | character_flags::APPLY_GRAVITY;
        math::vec3 velocity = math::vec3(0.0f);           // input: desired velocity; output: achieved velocity
        ground_state ground = ground_state::IN_AIR;       // [runtime]
        math::vec3 groundNormal = math::vec3(0.0f, 1.0f, 0.0f);   // [runtime]
        ecs::entity groundEntity = ecs::NULL_ENTITY;      // [runtime]

        [[nodiscard]] bool operator==(const character_controller &) const = default;
    };

    static_assert(sizeof(character_controller) == 68, "character_controller layout drives the C# mirror (R48)");
    static_assert(
      std::is_trivially_copyable_v<character_controller>,
      "character_controller must be trivially copyable"
    );
}

CONJURE_REFLECT_ENUM(conjure::physics::ground_state, "conjure.physics.ground_state",
    ON_GROUND,
    ON_STEEP_GROUND,
    NOT_SUPPORTED,
    IN_AIR);
CONJURE_REFLECT_FLAGS(conjure::physics::character_flags, "conjure.physics.character_flags",
    NONE,
    STICK_TO_FLOOR,
    WALK_STAIRS,
    APPLY_GRAVITY);

CONJURE_REFLECT_STRUCT(conjure::physics::character_controller, "conjure.physics.character_controller",
    CONJURE_FIELD(radius, .min = 0.0),
    CONJURE_FIELD(height, .min = 0.0),
    CONJURE_FIELD(maxSlopeAngleDegrees, .min = 0.0, .max = 90.0),
    CONJURE_FIELD(stepHeight, .min = 0.0),
    CONJURE_FIELD(mass, .min = 0.0),
    CONJURE_FIELD(maxStrength, .min = 0.0),
    CONJURE_FIELD(padding, .min = 0.0),
    CONJURE_FIELD(layer, .min = 0.0, .max = 31.0),
    CONJURE_FIELD(flags),
    CONJURE_FIELD(velocity),
    CONJURE_FIELD(ground, .readOnly = true),
    CONJURE_FIELD(groundNormal, .readOnly = true),
    CONJURE_FIELD(groundEntity, .readOnly = true));

CONJURE_COMPONENT(conjure::physics::character_controller, "conjure.physics.character_controller");

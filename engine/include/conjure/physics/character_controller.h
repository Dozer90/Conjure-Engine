#pragma once

#include "conjure/core/enum_flags.h"
#include "conjure/core/math/glm_types.h"
#include "conjure/core/reflect/reflect.h"
#include "conjure/core/types.h"
#include "conjure/ecs/component.h"
#include "conjure/ecs/entity.h"

#include <cstddef>
#include <numbers>
#include <type_traits>

// The character controller component (docs/design/14_physics.md 2.3, P13): a JPH::CharacterVirtual with
// an inner kinematic body, updated before the body step. Scripts write `velocity` in FIXED_UPDATE (or
// call physics_world::character_move); the pull writes the achieved velocity and the ground state back.
// A character_controller excludes rigid_body (CONFLICTING_BODY_COMPONENTS, the character wins).
//
// `velocity` is the character's own velocity (P20, code sweep F421). A step that starts with the character
// ON_GROUND adds the horizontal velocity of that ground, so a moving platform carries the character, and the
// pull takes the same amount out again: on a platform a character that stands still reads zero, and what a
// script writes is relative to the platform. IN_AIR the step keeps adding what the last ground gave, so a
// jump keeps the platform's speed until the character touches ground again; there `velocity` is relative to
// that motion, not to the world. Ground that does not carry it (ON_STEEP_GROUND, NOT_SUPPORTED) ends the
// carrying, and so does a teleport to a new position (the command, or a position written into
// local_transform, which is also how a character parented to a moving entity follows it): the step after a
// teleport adds no horizontal ground velocity. A rotation written into local_transform turns the character
// and leaves the carrying alone. The character is moved, never turned, by ground that rotates.
//
// Only the horizontal part is carried and taken out again. With APPLY_GRAVITY the vertical part of a grounded
// character is set to the ground's unless it is a jump (at least 0.1 m/s faster upwards than the ground), and
// reads so after the pull: on a lift that rises, or sinks slower than 0.1 m/s, `velocity.y` is the lift's. On
// a lift that sinks faster a character standing still is itself that much faster upwards than its ground, so
// the rule leaves it alone: STICK_TO_FLOOR steps it down after the lift, one step of the lift's motion behind
// and with `velocity.y` unchanged; without the flag it falls after the lift. A teleport does not end the
// vertical part: the step after it still takes the character for standing on the ground it left, so a
// character teleported off a rising lift keeps the lift's vertical velocity and falls from there, and a
// character parented to a rising platform it stands on gets that velocity on top of its parent's motion and
// hops (a parent that moves sideways or sinks moves it once). Let a lift carry the character; do not parent it.

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
        // steepest walkable ground, radians (45 degrees), shown in degrees by the editor; files that stored the
        // older maxSlopeAngleDegrees are migrated on load (scene_serializer::parse_document)
        f32 maxSlopeAngleRadians = std::numbers::pi_v<f32> / 4.0f;
        f32 stepHeight = 0.4f;
        f32 mass = 70.0f;                                 // push on dynamic bodies
        f32 maxStrength = 100.0f;
        f32 padding = 0.02f;                              // CharacterVirtualSettings::mCharacterPadding
        u8 layer = 0;
        character_flags flags = character_flags::STICK_TO_FLOOR | character_flags::WALK_STAIRS
          | character_flags::APPLY_GRAVITY;
        // input: desired velocity; output: achieved velocity. The character's own: without the horizontal
        // velocity of the ground that carries it (see the file comment)
        math::vec3 velocity = math::vec3(0.0f);
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
    CONJURE_FIELD(maxSlopeAngleRadians, .min = 0.0, .max = std::numbers::pi / 2.0,
      .unit = conjure::core::reflect::field_unit::RADIANS),
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

#include "physics/jolt_conversions.h"
#include "physics/physics_world_internal.h"

#include "conjure/physics/log_category.h"

#include "conjure/core/log.h"
#include "conjure/core/math/math_utils.h"
#include "conjure/scene/components.h"

#include <Jolt/Geometry/Plane.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/ShapeFilter.h>

#include <algorithm>
#include <cmath>

namespace conjure::physics {

namespace {

    constexpr f32 MIN_CAPSULE_DIMENSION = 0.01f;

    /// <summary>
    /// The capsule of a character with its feet at the origin: Jolt positions a CharacterVirtual at
    /// its base, so the capsule is lifted by half its total height.
    /// </summary>
    [[nodiscard]] JPH::RefConst<JPH::Shape> make_character_shape(const character_controller &controller) {
        const f32 radius = std::max(std::abs(controller.radius), MIN_CAPSULE_DIMENSION);
        const f32 halfHeight = std::max(std::abs(controller.height) * 0.5f - radius, MIN_CAPSULE_DIMENSION);
        const JPH::RefConst<JPH::Shape> capsule = JPH::RefConst<JPH::Shape>(
          JPH::CapsuleShapeSettings(halfHeight, radius).Create().Get()
        );
        const JPH::Vec3 lift(0.0f, halfHeight + radius, 0.0f);
        return JPH::RefConst<JPH::Shape>(
          JPH::RotatedTranslatedShapeSettings(lift, JPH::Quat::sIdentity(), capsule.GetPtr()).Create().Get()
        );
    }

    [[nodiscard]] ground_state to_ground_state(JPH::CharacterBase::EGroundState state) noexcept {
        switch (state) {
            case JPH::CharacterBase::EGroundState::OnGround:
                return ground_state::ON_GROUND;
            case JPH::CharacterBase::EGroundState::OnSteepGround:
                return ground_state::ON_STEEP_GROUND;
            case JPH::CharacterBase::EGroundState::NotSupported:
                return ground_state::NOT_SUPPORTED;
            case JPH::CharacterBase::EGroundState::InAir:
                return ground_state::IN_AIR;
        }
        return ground_state::IN_AIR;
    }

    /// <summary>
    /// True when a field that shapes the character changed (runtime fields excluded).
    /// </summary>
    [[nodiscard]] bool authored_fields_differ(const character_controller &a, const character_controller &b) noexcept {
        return a.radius != b.radius || a.height != b.height || a.maxSlopeAngleDegrees != b.maxSlopeAngleDegrees
          || a.stepHeight != b.stepHeight || a.mass != b.mass || a.maxStrength != b.maxStrength
          || a.padding != b.padding || a.layer != b.layer || a.flags != b.flags;
    }

    void fill_settings(JPH::CharacterVirtualSettings &settings, const character_controller &controller, u8 layer) {
        const f32 radius = std::max(std::abs(controller.radius), MIN_CAPSULE_DIMENSION);
        settings.mShape = make_character_shape(controller);
        settings.mInnerBodyShape = settings.mShape;
        settings.mInnerBodyLayer = jolt::object_layer(layer, motion_type::KINEMATIC);
        settings.mMaxSlopeAngle = math::radians(math::clamp(controller.maxSlopeAngleDegrees, 0.0f, 90.0f));
        settings.mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), -radius);
        settings.mMass = std::max(controller.mass, 0.0f);
        settings.mMaxStrength = std::max(controller.maxStrength, 0.0f);
        settings.mCharacterPadding = std::max(controller.padding, 0.0f);
    }
}

bool physics_world::state::create_character(
  ecs::entity target,
  const character_controller &controller,
  ecs::command_buffer &deferred
) {
    if (characterCount >= settings.maxCharacters) {
        ++stats.bodiesWithoutShape;
        if (!bodyLimitLoggedThisStep) {
            bodyLimitLoggedThisStep = true;
            CONJURE_LOG_ERROR(
              LOG_PHYSICS,
              "character limit reached (BODY_LIMIT_REACHED, maxCharacters = {}); entity {} stays pending",
              settings.maxCharacters,
              target
            );
        }
        return false;
    }
    const u8 layer = resolve_layer(target, controller.layer);
    const math::transform pose = compose_world_pose(target);

    JPH::CharacterVirtualSettings characterSettings = {};
    fill_settings(characterSettings, controller, layer);
    JPH::Ref<JPH::CharacterVirtual> character = jolt::make_jolt_ref<JPH::CharacterVirtual>(
      &characterSettings,
      jolt::to_jolt(pose.position),
      jolt::to_jolt(pose.rotation),
      target.to_u64(),
      physicsSystem.get()
    );
    const JPH::BodyID inner = character->GetInnerBodyID();
    if (inner.IsInvalid()) {
        ++stats.bodiesWithoutShape;
        if (!bodyLimitLoggedThisStep) {
            bodyLimitLoggedThisStep = true;
            CONJURE_LOG_ERROR(
              LOG_PHYSICS,
              "body limit reached (BODY_LIMIT_REACHED); character {} stays pending",
              target
            );
        }
        return false;
    }
    character->SetLinearVelocity(jolt::to_jolt(controller.velocity));

    body_record &row = claim_row(inner, target);
    row.kind = record_kind::CHARACTER;
    row.motion = motion_type::KINEMATIC;
    row.layer = layer;
    row.hasParent = world->has_raw(target, ids.parent);
    row.characterMirror = controller;
    row.lastPulled = pose;
    row.lastScale = pose.scale;
    row.character = character;
    awake[inner.GetIndex()].store(1, std::memory_order_relaxed);
    ++characterCount;
    add_interpolated(target, deferred);
    return true;
}

void physics_world::state::rebuild_character(body_record &row, const character_controller &controller) {
    if (!authored_fields_differ(row.characterMirror, controller)) {
        return;
    }
    const u8 layer = resolve_layer(row.entity, controller.layer);
    JPH::CharacterVirtualSettings characterSettings = {};
    fill_settings(characterSettings, controller, layer);

    // The inner body keeps its id so the row (and every contact pair keyed on it) stays valid: the old
    // character's destructor frees the id and the new one is created with it.
    const JPH::RVec3 position = row.character->GetPosition();
    const JPH::Quat rotation = row.character->GetRotation();
    const JPH::Vec3 velocity = row.character->GetLinearVelocity();
    characterSettings.mInnerBodyIDOverride = row.bodyId;
    row.character = nullptr;

    JPH::Ref<JPH::CharacterVirtual> character = jolt::make_jolt_ref<JPH::CharacterVirtual>(
      &characterSettings,
      position,
      rotation,
      row.entity.to_u64(),
      physicsSystem.get()
    );
    character->SetLinearVelocity(velocity);
    row.character = character;
    row.layer = layer;
    row.characterMirror = controller;
}

void physics_world::state::step_characters(f32 fixedDeltaTime) {
    const JPH::Vec3 joltGravity = jolt::to_jolt(gravity);
    for (body_record &row : records) {
        if (row.is_free() || row.kind != record_kind::CHARACTER || row.character == nullptr) {
            continue;
        }
        const character_controller *controller = read<character_controller>(row.entity);
        if (controller == nullptr) {
            continue;
        }
        JPH::CharacterVirtual &character = *row.character;
        const f32 stepHeight = std::max(controller->stepHeight, 0.0f);
        JPH::CharacterVirtual::ExtendedUpdateSettings extended = {};
        extended.mStickToFloorStepDown = has_flag(controller->flags, character_flags::STICK_TO_FLOOR)
          ? JPH::Vec3(0.0f, -stepHeight, 0.0f)
          : JPH::Vec3::sZero();
        extended.mWalkStairsStepUp = has_flag(controller->flags, character_flags::WALK_STAIRS)
          ? JPH::Vec3(0.0f, stepHeight, 0.0f)
          : JPH::Vec3::sZero();

        // A queued character_move replaces the component's velocity for this step (the push never
        // writes the component, which the push system declares as a read).
        JPH::Vec3 velocity = jolt::to_jolt(row.hasDesiredVelocity ? row.desiredVelocity : controller->velocity);
        row.hasDesiredVelocity = false;

        // The ground state is the one the last update left: where the character stands as this step starts.
        const JPH::CharacterBase::EGroundState groundState = character.GetGroundState();
        const bool grounded = groundState == JPH::CharacterBase::EGroundState::OnGround;
        const JPH::Vec3 up = character.GetUp();
        if (row.skipGroundCarry) {
            // Moved to a new position since its last step: that ground state describes the place it left, so
            // this step carries nothing (the teleport cleared carriedVelocity) and the next one looks again.
            row.skipGroundCarry = false;
        } else if (grounded) {
            // The ground may have changed its velocity since the last update (a platform the push just
            // moved): ask its body again, as Jolt's own character sample does, and carry the character with
            // the horizontal part (code sweep F421).
            character.UpdateGroundVelocity();
            const JPH::Vec3 groundVelocity = character.GetGroundVelocity();
            row.carriedVelocity = jolt::from_jolt(groundVelocity - groundVelocity.Dot(up) * up);
        } else if (groundState != JPH::CharacterBase::EGroundState::InAir) {
            // It touches ground that does not carry it (too steep, or under its edge only): what the last
            // ground gave it ends here. Kept, it would push the character along a slope it slides down, and
            // where CancelVelocityTowardsSteepSlopes below takes that push out again, the pull would still
            // subtract it and report a velocity the character does not have. In the air it stays: a jump
            // keeps the speed of the platform it left.
            row.carriedVelocity = math::vec3(0.0f);
        }
        if (has_flag(controller->flags, character_flags::APPLY_GRAVITY)) {
            if (grounded) {
                // Grounded: the vertical component follows the ground (what Jolt's own character sample
                // does) unless the script set an upward velocity this step, which is a jump.
                const f32 vertical = velocity.Dot(up);
                const f32 groundVertical = character.GetGroundVelocity().Dot(up);
                if (vertical - groundVertical < 0.1f) {
                    velocity += (groundVertical - vertical) * up;
                }
            } else {
                velocity += joltGravity * fixedDeltaTime;
            }
        }
        // The pull subtracts the same vector again, so nothing compounds from step to step.
        velocity += jolt::to_jolt(row.carriedVelocity);
        velocity = character.CancelVelocityTowardsSteepSlopes(velocity);
        character.SetLinearVelocity(velocity);

        const JPH::ObjectLayer objectLayer = jolt::object_layer(row.layer, motion_type::KINEMATIC);
        character.ExtendedUpdate(
          fixedDeltaTime,
          joltGravity,
          extended,
          physicsSystem->GetDefaultBroadPhaseLayerFilter(objectLayer),
          physicsSystem->GetDefaultLayerFilter(objectLayer),
          JPH::BodyFilter(),
          JPH::ShapeFilter(),
          *tempAllocator
        );
    }
}

void physics_world::state::pull_characters() {
    for (body_record &row : records) {
        if (row.is_free() || row.kind != record_kind::CHARACTER || row.character == nullptr) {
            continue;
        }
        character_controller *controller = write<character_controller>(row.entity);
        if (controller == nullptr) {
            continue;
        }
        const JPH::CharacterVirtual &character = *row.character;
        // The capsule is never scaled; lastScale is the entity's world scale as the last push that looked at
        // its transform saw it, and the render pose carries it.
        math::transform current = {};
        current.position = jolt::from_jolt(character.GetPosition());
        current.rotation = jolt::from_jolt(character.GetRotation());
        current.scale = row.lastScale;

        scene::interpolated_transform *interpolated = write<scene::interpolated_transform>(row.entity);
        if (interpolated != nullptr) {
            interpolated->previous = row.firstPull ? current : interpolated->current;
            interpolated->current = current;
            interpolated->previous.scale = row.lastScale;
        }
        row.firstPull = false;
        row.awaitingPull = false;
        row.lastPulled = current;

        // Jolt keeps the velocity the step gave it; without the ground's share it is the character's own
        // again: relative to the ground it stands on, or to the ground it last stood on while it is in the air.
        controller->velocity = jolt::from_jolt(character.GetLinearVelocity()) - row.carriedVelocity;
        controller->ground = to_ground_state(character.GetGroundState());
        controller->groundNormal = controller->ground == ground_state::IN_AIR
          ? math::vec3(0.0f, 1.0f, 0.0f)
          : jolt::from_jolt(character.GetGroundNormal());
        controller->groundEntity = controller->ground == ground_state::IN_AIR
          ? ecs::NULL_ENTITY
          : ecs::entity::from_u64(character.GetGroundUserData());
        row.characterMirror.velocity = controller->velocity;
        row.characterMirror.ground = controller->ground;
        row.characterMirror.groundNormal = controller->groundNormal;
        row.characterMirror.groundEntity = controller->groundEntity;
        write_local_pose(row.entity, current);
    }
}
}

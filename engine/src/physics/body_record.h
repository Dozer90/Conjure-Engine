#pragma once

#include "physics/jolt_include.h"

#include "conjure/physics/character_controller.h"
#include "conjure/physics/components.h"

#include "conjure/core/enum_flags.h"
#include "conjure/core/math/transform.h"
#include "conjure/core/types.h"
#include "conjure/core/uuid.h"
#include "conjure/ecs/entity.h"

#include <Jolt/Core/Reference.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>

// The entity <-> JPH::BodyID table (docs/design/14_physics.md 2.6, P7). Rows are indexed by
// BodyID::GetIndex(); the awake flags live in a separate atomic array because Jolt's activation listener
// writes them from job threads while the rows themselves move when the table grows.

namespace conjure::physics {

    enum class record_kind : u8 {
        BODY,
        CHARACTER
    };

    enum class record_dirty_flags : u8 {
        NONE = 0,
        SHAPE = 1 << 0,
        PROPERTIES = 1 << 1,
        LAYER = 1 << 2,
        MOTION = 1 << 3
    };
    CONJURE_ENUM_FLAGS(record_dirty_flags);

    /// <summary>
    /// One body or character known to the world. A free row has an invalid bodyId.
    /// </summary>
    struct body_record {
        ecs::entity entity = {};
        JPH::BodyID bodyId = {};                     // invalid when the row is free
        record_kind kind = record_kind::BODY;
        motion_type motion = motion_type::STATIC;
        u8 layer = 0;
        bool isTrigger = false;
        bool hasShape = false;
        bool hasParent = false;                      // scene::parent present: the pull writes it serially
        bool firstPull = true;                       // next pull writes previous = current = pose
        bool awaitingPull = false;                   // a teleport happened; push ignores local_transform until pulled
        bool movesWithTransform = false;             // KINEMATIC_MOVES_WITH_TRANSFORM
        bool implicitStatic = false;                 // colliders without a rigid_body
        bool hasDesiredVelocity = false;             // CHARACTER rows: a character_move waits in desiredVelocity
        record_dirty_flags dirty = record_dirty_flags::NONE;
        u64 shapeKey = 0;                            // shape_cache key of the leaf (0 = per-entity compound)
        rigid_body mirror = {};                      // last pushed / pulled component value (field diffing)
        character_controller characterMirror = {};   // CHARACTER rows: last pushed authored fields
        math::transform lastPulled = {};             // world pose written by the last pull (teleport detection)
        math::vec3 lastScale = math::vec3(1.0f);     // world scale the shape was built with
        // CHARACTER rows: the velocity queued by character_move, consumed by the next step so the push
        // never writes character_controller (the push system declares the component as a read).
        math::vec3 desiredVelocity = math::vec3(0.0f);
        core::uuid convexAsset = {};                 // asset the convex collider was built from (reload tracking)
        core::uuid meshAsset = {};                   // asset the mesh collider was built from
        JPH::Ref<JPH::CharacterVirtual> character = {};   // CHARACTER rows only

        [[nodiscard]] bool is_free() const noexcept {
            return bodyId.IsInvalid();
        }
    };
}

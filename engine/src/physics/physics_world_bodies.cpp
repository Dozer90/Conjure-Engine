#include "physics/jolt_conversions.h"
#include "physics/physics_world_internal.h"

#include "conjure/physics/components.h"
#include "conjure/physics/log_category.h"

#include "conjure/core/assert.h"
#include "conjure/core/log.h"
#include "conjure/scene/components.h"

#include <Jolt/Physics/Body/AllowedDOFs.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/BodyLockInterface.h>
#include <Jolt/Physics/Body/MotionProperties.h>
#include <Jolt/Physics/Body/MotionQuality.h>
#include <Jolt/Physics/Body/MotionType.h>
#include <Jolt/Physics/EActivation.h>

#include <Jolt/Physics/Body/MassProperties.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>

#include <algorithm>
#include <span>
#include <utility>

namespace conjure::physics {

namespace {

    constexpr u32 HIERARCHY_WALK_LIMIT = 64;

    [[nodiscard]] JPH::EMotionType to_jolt(motion_type motion) noexcept {
        switch (motion) {
            case motion_type::STATIC:
                return JPH::EMotionType::Static;
            case motion_type::KINEMATIC:
                return JPH::EMotionType::Kinematic;
            case motion_type::DYNAMIC:
                return JPH::EMotionType::Dynamic;
        }
        return JPH::EMotionType::Static;
    }

    /// <summary>
    /// True for a static trigger (code sweep F408). To Jolt it is a kinematic sensor that stays active, so
    /// it goes on seeing a body that fell asleep inside it; a static sensor loses that contact (the comment
    /// on JPH::Body::SetIsSensor). To the engine it stays a static body in every other respect.
    /// </summary>
    [[nodiscard]] constexpr bool is_static_sensor(motion_type motion, bool isTrigger) noexcept {
        return motion == motion_type::STATIC && isTrigger;
    }

    /// <summary>
    /// The motion type Jolt holds for a body: the engine's, except for a static trigger (is_static_sensor).
    /// </summary>
    [[nodiscard]] JPH::EMotionType to_jolt(motion_type motion, bool isTrigger) noexcept {
        return is_static_sensor(motion, isTrigger) ? JPH::EMotionType::Kinematic : to_jolt(motion);
    }

    /// <summary>
    /// The degrees of freedom the lock flags leave; everything when they would leave none.
    /// </summary>
    [[nodiscard]] JPH::EAllowedDOFs allowed_dofs(rigid_body_flags flags, ecs::entity entity) noexcept {
        using JPH::EAllowedDOFs;
        JPH::uint8 bits = static_cast<JPH::uint8>(EAllowedDOFs::All);
        const auto clear = [&bits](EAllowedDOFs dof) {
            bits = static_cast<JPH::uint8>(bits & static_cast<JPH::uint8>(~static_cast<JPH::uint8>(dof)));
        };
        if (has_flag(flags, rigid_body_flags::LOCK_ROTATION) || has_flag(flags, rigid_body_flags::LOCK_ROTATION_X)) {
            clear(EAllowedDOFs::RotationX);
        }
        if (has_flag(flags, rigid_body_flags::LOCK_ROTATION) || has_flag(flags, rigid_body_flags::LOCK_ROTATION_Y)) {
            clear(EAllowedDOFs::RotationY);
        }
        if (has_flag(flags, rigid_body_flags::LOCK_ROTATION) || has_flag(flags, rigid_body_flags::LOCK_ROTATION_Z)) {
            clear(EAllowedDOFs::RotationZ);
        }
        if (has_flag(flags, rigid_body_flags::LOCK_TRANSLATION_X)) {
            clear(EAllowedDOFs::TranslationX);
        }
        if (has_flag(flags, rigid_body_flags::LOCK_TRANSLATION_Y)) {
            clear(EAllowedDOFs::TranslationY);
        }
        if (has_flag(flags, rigid_body_flags::LOCK_TRANSLATION_Z)) {
            clear(EAllowedDOFs::TranslationZ);
        }
        if (bits == static_cast<JPH::uint8>(EAllowedDOFs::None)) {
            CONJURE_LOG_WARN(LOG_PHYSICS, "entity {}: every degree of freedom locked; locks ignored", entity);
            return EAllowedDOFs::All;
        }
        return static_cast<EAllowedDOFs>(bits);
    }

    /// <summary>
    /// The rigid_body a collider-only entity behaves as: an implicit static body (P8) that never
    /// changes motion type, so a rebuild compares against STATIC instead of the DYNAMIC default.
    /// </summary>
    [[nodiscard]] rigid_body implicit_static_defaults() noexcept {
        rigid_body defaults = {};
        defaults.motionType = motion_type::STATIC;
        defaults.flags = rigid_body_flags::NONE;
        return defaults;
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Observers

void physics_world::state::install_observers() {
    const auto added = [this](ecs::world &, ecs::entity target, ecs::component_id id, ecs::command_buffer &deferred) {
        on_component_added(target, id, deferred);
    };
    const auto removed = [this](ecs::world &, ecs::entity target, ecs::component_id id, ecs::command_buffer &deferred) {
        on_component_removed(target, id, deferred);
    };
    const ecs::component_id watched[] = {
        ids.rigidBody, ids.box, ids.sphere, ids.capsule, ids.convex, ids.mesh, ids.material, ids.character, ids.parent
    };
    for (const ecs::component_id id : watched) {
        observers.push_back(world->observe(id, ecs::component_event::ADDED, added));
        observers.push_back(world->observe(id, ecs::component_event::REMOVED, removed));
    }
    observers.push_back(world->observe_destroyed([this](
      ecs::world &,
      ecs::entity target,
      ecs::command_buffer &deferred
    ) {
        on_entity_destroyed(target, deferred);
    }));
}

void physics_world::state::remove_observers() {
    for (const ecs::observer_id id : observers) {
        world->unobserve(id);
    }
    observers.clear();
}

void physics_world::state::on_component_added(ecs::entity target, ecs::component_id id, ecs::command_buffer &deferred) {
    body_record *row = find_row(target);
    if (id == ids.parent) {
        if (row != nullptr) {
            row->hasParent = true;
        }
        return;
    }
    if (id == ids.material) {
        if (row != nullptr) {
            mark_rebuild(target, record_dirty_flags::PROPERTIES);
        }
        return;
    }
    if (row != nullptr) {
        if (id == ids.character && row->kind == record_kind::BODY) {
            // 2.3: the character wins over the rigid_body / colliders that already made a body, so
            // that body goes and the next push creates the character (and logs the conflict).
            destroy_body(static_cast<u32>(row - records.data()), nullptr, true);
            mark_pending_create(target);
            add_interpolated(target, deferred);
            return;
        }
        if (id == ids.rigidBody && row->kind == record_kind::CHARACTER) {
            CONJURE_LOG_WARN(
              LOG_PHYSICS,
              "entity {}: rigid_body added to a character (CONFLICTING_BODY_COMPONENTS); the character wins",
              target
            );
        }
        mark_rebuild(target, record_dirty_flags::SHAPE | record_dirty_flags::PROPERTIES | record_dirty_flags::MOTION);
        return;
    }
    mark_pending_create(target);
    if (id == ids.character) {
        add_interpolated(target, deferred);
        return;
    }
    if (id == ids.rigidBody) {
        const rigid_body *body = read<rigid_body>(target);
        if (body != nullptr && body->motionType != motion_type::STATIC) {
            add_interpolated(target, deferred);
        }
    }
}

void physics_world::state::on_component_removed(
  ecs::entity target,
  ecs::component_id id,
  ecs::command_buffer &deferred
) {
    body_record *row = find_row(target);
    if (id == ids.parent) {
        if (row != nullptr) {
            row->hasParent = false;
        }
        return;
    }
    if (id == ids.material) {
        if (row != nullptr) {
            mark_rebuild(target, record_dirty_flags::PROPERTIES);
        }
        return;
    }
    ecs::component_mask remaining = world->mask_of(target);
    remaining.clear(id);
    const bool bodyRemains = remaining.test(ids.rigidBody) || remaining.test(ids.character) || remaining.test(ids.box)
      || remaining.test(ids.sphere) || remaining.test(ids.capsule) || remaining.test(ids.convex)
      || remaining.test(ids.mesh);

    if (row == nullptr) {
        if (!bodyRemains && is_pending(target)) {
            withdraw_pending(target);
            remove_interpolated(target, deferred);
        }
        return;
    }
    if (id == ids.character && bodyRemains) {
        // The character goes; the remaining rigid_body / colliders become an ordinary body at the next
        // push. interpolated_transform stays unless that body is static.
        destroy_body(static_cast<u32>(row - records.data()), nullptr, true);
        mark_pending_create(target);
        const rigid_body *body = read<rigid_body>(target);
        if (body == nullptr || body->motionType == motion_type::STATIC) {
            remove_interpolated(target, deferred);
        }
        return;
    }
    if (id == ids.character || !bodyRemains) {
        destroy_body(static_cast<u32>(row - records.data()), &deferred, true);
        return;
    }
    mark_rebuild(target, record_dirty_flags::SHAPE | record_dirty_flags::PROPERTIES | record_dirty_flags::MOTION);
}

void physics_world::state::on_entity_destroyed(ecs::entity target, ecs::command_buffer &deferred) {
    CONJURE_UNUSED(deferred);
    body_record *row = find_row(target);
    if (row != nullptr) {
        // The entity dies right after this callback, so no interpolated_transform edit is queued.
        destroy_body(static_cast<u32>(row - records.data()), nullptr, true);
        return;
    }
    withdraw_pending(target);
}

void physics_world::state::mark_pending_create(ecs::entity target) {
    const auto found = rowOfEntity.find(target);
    if (found != rowOfEntity.end()) {
        if (found->second == WITHDRAWN_ROW) {
            // The pendingCreate entry is still there: it becomes live again.
            found->second = PENDING_ROW;
        }
        return;
    }
    rowOfEntity.try_emplace(target, PENDING_ROW);
    pendingCreate.push_back(target);
}

void physics_world::state::withdraw_pending(ecs::entity target) {
    const auto found = rowOfEntity.find(target);
    if (found != rowOfEntity.end() && found->second == PENDING_ROW) {
        found->second = WITHDRAWN_ROW;
    }
}

void physics_world::state::mark_rebuild(ecs::entity target, record_dirty_flags flags) {
    body_record *row = find_row(target);
    if (row == nullptr) {
        return;
    }
    if (row->dirty == record_dirty_flags::NONE) {
        pendingRebuild.push_back(target);
    }
    row->dirty |= flags;
}

void physics_world::state::add_interpolated(ecs::entity target, ecs::command_buffer &deferred) {
    if (world->has_raw(target, ids.interpolatedTransform)) {
        return;
    }
    // Seeded with the current world pose: scene propagation renders an entity that carries the component
    // from it alone, so a default value would show the body at the origin until its first pull.
    scene::interpolated_transform initial = {};
    initial.current = compose_world_pose(target);
    initial.previous = initial.current;
    deferred.add_raw(
      target,
      ids.interpolatedTransform,
      std::as_bytes(std::span<const scene::interpolated_transform>(&initial, 1))
    );
}

void physics_world::state::remove_interpolated(ecs::entity target, ecs::command_buffer &deferred) {
    if (!world->has_raw(target, ids.interpolatedTransform)) {
        return;
    }
    deferred.remove_raw(target, ids.interpolatedTransform);
}

void physics_world::state::queue_interpolated_removals(ecs::command_buffer &deferred) {
    // The entities physics gave the component (R47): live characters and non-static bodies, and every
    // pending entity with a rigid_body or character_controller (create_body would take it off a static
    // one anyway). A live static body keeps one its owner added.
    for (const body_record &row : records) {
        if (row.is_free() || !world->is_alive(row.entity)) {
            continue;
        }
        if (row.kind == record_kind::CHARACTER || row.motion != motion_type::STATIC) {
            remove_interpolated(row.entity, deferred);
        }
    }
    for (const ecs::entity target : pendingCreate) {
        if (!is_pending(target) || !world->is_alive(target)) {
            continue;
        }
        if (world->has_raw(target, ids.character) || world->has_raw(target, ids.rigidBody)) {
            remove_interpolated(target, deferred);
        }
    }
}


/*===========================================================================*/
/*===========================================================================*/
// Pending creation

void physics_world::state::flush_pending_creates(ecs::command_buffer &deferred) {
    if (pendingCreate.empty()) {
        return;
    }
    bodyLimitLoggedThisStep = false;
    stats.bodiesWithoutShape = 0;
    pendingScratch.clear();
    activateBatch.clear();
    sleepBatch.clear();

    for (const ecs::entity target : pendingCreate) {
        const auto marker = rowOfEntity.find(target);
        if (marker == rowOfEntity.end() || marker->second != PENDING_ROW) {
            // Withdrawn before this push (its body components went away) or already created.
            if (marker != rowOfEntity.end() && marker->second == WITHDRAWN_ROW) {
                rowOfEntity.erase(marker);
            }
            continue;
        }
        if (!world->is_alive(target)) {
            rowOfEntity.erase(marker);
            continue;
        }
        if (create_body(target, deferred)) {
            continue;
        }
        // Still pending: either waiting for colliders / assets or refused by the body limit.
        if (is_pending(target)) {
            pendingScratch.push_back(target);
        }
    }
    pendingCreate.swap(pendingScratch);
    pendingScratch.clear();

    JPH::BodyInterface &bodies = physicsSystem->GetBodyInterface();
    if (!activateBatch.empty()) {
        const JPH::BodyInterface::AddState added = bodies.AddBodiesPrepare(
          activateBatch.data(),
          static_cast<int>(activateBatch.size())
        );
        bodies.AddBodiesFinalize(
          activateBatch.data(),
          static_cast<int>(activateBatch.size()),
          added,
          JPH::EActivation::Activate
        );
        for (const JPH::BodyID &bodyId : activateBatch) {
            awake[bodyId.GetIndex()].store(1, std::memory_order_relaxed);
        }
    }
    if (!sleepBatch.empty()) {
        const JPH::BodyInterface::AddState added = bodies.AddBodiesPrepare(
          sleepBatch.data(),
          static_cast<int>(sleepBatch.size())
        );
        bodies.AddBodiesFinalize(
          sleepBatch.data(),
          static_cast<int>(sleepBatch.size()),
          added,
          JPH::EActivation::DontActivate
        );
    }
    addedSinceOptimise += static_cast<u32>(activateBatch.size() + sleepBatch.size());
    const u32 total = static_cast<u32>(physicsSystem->GetNumBodies());
    if (addedSinceOptimise > std::max<u32>(64, total / 20)) {
        physicsSystem->OptimizeBroadPhase();
        addedSinceOptimise = 0;
    }
    activateBatch.clear();
    sleepBatch.clear();
}

body_record &physics_world::state::claim_row(const JPH::BodyID &bodyId, ecs::entity target) {
    const u32 index = bodyId.GetIndex();
    if (index >= records.size()) {
        const usize grown = std::min<usize>(
          settings.maxBodies,
          ((static_cast<usize>(index) / ROW_BLOCK) + 1) * ROW_BLOCK
        );
        records.resize(std::max<usize>(grown, static_cast<usize>(index) + 1));
    }
    body_record &row = records[index];
    CONJURE_ASSERT(row.is_free(), "physics_world: Jolt handed out the index of a live row");
    row = body_record();
    row.entity = target;
    row.bodyId = bodyId;
    rowOfEntity.insert_or_assign(target, index);
    if (target.index >= liveRowByEntityIndex.size()) {
        // Grown in blocks like the rows; body creation allocates in Jolt anyway.
        const usize grown = ((static_cast<usize>(target.index) / ROW_BLOCK) + 1) * ROW_BLOCK;
        liveRowByEntityIndex.resize(grown, NO_ROW);
    }
    liveRowByEntityIndex[target.index] = index;
    awake[index].store(0, std::memory_order_relaxed);
    ++liveRows;
    return row;
}

shape_build_input physics_world::state::gather_colliders(
  ecs::entity target,
  motion_type motion,
  const math::vec3 &scale
) const {
    shape_build_input input = {};
    input.entity = target;
    input.motion = motion;
    input.scale = scale;
    input.box = read<box_collider>(target);
    input.sphere = read<sphere_collider>(target);
    input.capsule = read<capsule_collider>(target);
    input.convex = read<convex_collider>(target);
    input.mesh = read<mesh_collider>(target);
    return input;
}

u8 physics_world::state::resolve_layer(ecs::entity target, u8 requested) const {
    if (requested < settings.layers.count()) {
        return requested;
    }
    CONJURE_LOG_WARN(
      LOG_PHYSICS,
      "entity {}: layer {} does not exist (UNKNOWN_LAYER); using layer 0",
      target,
      requested
    );
    return 0;
}

math::transform physics_world::state::compose_world_pose(ecs::entity target) const noexcept {
    math::transform pose = {};
    const scene::local_transform *local = read<scene::local_transform>(target);
    if (local != nullptr) {
        pose = local->to_transform();
    }
    const scene::parent *link = read<scene::parent>(target);
    u32 depth = 0;
    while (link != nullptr && !link->value.is_null() && depth < HIERARCHY_WALK_LIMIT) {
        const ecs::entity ancestor = link->value;
        if (!world->is_alive(ancestor)) {
            break;
        }
        const scene::local_transform *ancestorLocal = read<scene::local_transform>(ancestor);
        if (ancestorLocal != nullptr) {
            pose = ancestorLocal->to_transform() * pose;
        }
        link = read<scene::parent>(ancestor);
        ++depth;
    }
    return pose;
}

bool physics_world::state::create_body(ecs::entity target, ecs::command_buffer &deferred) {
    const character_controller *controller = read<character_controller>(target);
    const rigid_body *body = read<rigid_body>(target);
    if (controller != nullptr) {
        if (body != nullptr) {
            CONJURE_LOG_WARN(
              LOG_PHYSICS,
              "entity {}: rigid_body and character_controller together (CONFLICTING_BODY_COMPONENTS); "
              "the character wins",
              target
            );
        }
        return create_character(target, *controller, deferred);
    }

    const motion_type motion = body == nullptr ? motion_type::STATIC : body->motionType;
    const math::transform pose = compose_world_pose(target);
    const shape_build_input input = gather_colliders(target, motion, pose.scale);
    const shape_build_result built = build_shape(input, shapes);
    if (!built.hasCollider) {
        if (body == nullptr) {
            // Nothing defines a body any more: the pending entry is dropped.
            rowOfEntity.erase(target);
            return false;
        }
        ++stats.bodiesWithoutShape;
        return false;
    }
    if (built.shape == nullptr) {
        // Every collider failed, or one references an asset that is not loaded yet (pendingAsset): the
        // entity stays pending and the next flush tries again.
        ++stats.bodiesWithoutShape;
        return false;
    }

    const rigid_body defaults = implicit_static_defaults();
    const rigid_body &authored = body == nullptr ? defaults : *body;
    const u8 layer = resolve_layer(target, authored.layer);
    const physics_material *material = read<physics_material>(target);
    const physics_material surface = material == nullptr ? physics_material() : *material;

    // A static trigger is a kinematic body to Jolt but keeps the static object layer: the layer table never
    // pairs two static layers (jolt_layers.cpp), so it meets neither static geometry nor another static
    // trigger, and queries with includeStatic off go on skipping it.
    const bool staticSensor = is_static_sensor(motion, built.isTrigger);
    JPH::BodyCreationSettings creation(
      built.shape.GetPtr(),
      jolt::to_jolt(pose.position),
      jolt::to_jolt(pose.rotation),
      to_jolt(motion, built.isTrigger),
      jolt::object_layer(layer, motion)
    );
    creation.mUserData = target.to_u64();
    creation.mLinearDamping = std::max(authored.linearDamping, 0.0f);
    creation.mAngularDamping = std::max(authored.angularDamping, 0.0f);
    creation.mGravityFactor = authored.gravityFactor;
    creation.mMaxLinearVelocity = std::max(authored.maxLinearVelocity, 0.0f);
    creation.mMotionQuality = has_flag(authored.flags, rigid_body_flags::CONTINUOUS_COLLISION)
      ? JPH::EMotionQuality::LinearCast
      : JPH::EMotionQuality::Discrete;
    creation.mAllowSleeping = has_flag(authored.flags, rigid_body_flags::ALLOW_SLEEPING);
    creation.mIsSensor = built.isTrigger;
    creation.mFriction = surface.friction;
    creation.mRestitution = surface.restitution;
    creation.mAllowedDOFs = allowed_dofs(authored.flags, target);
    creation.mApplyGyroscopicForce = has_flag(authored.flags, rigid_body_flags::USE_GYROSCOPIC_FORCE);
    // Jolt would move a kinematic body by the velocities a static rigid_body may still carry in its component.
    creation.mLinearVelocity = staticSensor ? JPH::Vec3::sZero() : jolt::to_jolt(authored.linearVelocity);
    creation.mAngularVelocity = staticSensor ? JPH::Vec3::sZero() : jolt::to_jolt(authored.angularVelocity);
    // The kinematic trigger of the engine also reports static geometry. A static trigger does not: the flag
    // stays off and its layer keeps it from static bodies.
    creation.mCollideKinematicVsNonDynamic = built.isTrigger && motion == motion_type::KINEMATIC;
    // A body authored through rigid_body may change its motion type later; implicit statics never do.
    creation.mAllowDynamicOrKinematic = body != nullptr;
    // Jolt builds motion properties, which need a mass, for every body a rigid_body authors (static ones
    // included) and for every static trigger (a kinematic body). The static trigger made of colliders alone
    // does not reach the last branch today: implicit_static_defaults() carries the default mass of 1 kg, so
    // the first one takes it. The term stays so that a default without a mass would not hand Jolt a
    // kinematic body whose shape (a mesh_collider) has none either.
    const bool hasMotionProperties = body != nullptr || staticSensor;
    if (authored.mass > 0.0f) {
        creation.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
        creation.mMassPropertiesOverride.mMass = authored.mass;
    } else if (!hasMotionProperties || built.shape->GetMassProperties().mMass > 0.0f) {
        creation.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateMassAndInertia;
    } else {
        // A shape without a volume (a mesh_collider) derives no mass: 1 kg. Its zero inertia makes Jolt
        // fall back to the inertia of a unit sphere.
        creation.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
        creation.mMassPropertiesOverride.mMass = 1.0f;
    }

    JPH::Body *created = physicsSystem->GetBodyInterface().CreateBody(creation);
    if (created == nullptr) {
        ++stats.bodiesWithoutShape;
        if (!bodyLimitLoggedThisStep) {
            bodyLimitLoggedThisStep = true;
            CONJURE_LOG_ERROR(
              LOG_PHYSICS,
              "body limit reached (BODY_LIMIT_REACHED, maxBodies = {}); entity {} stays pending",
              settings.maxBodies,
              target
            );
        }
        return false;
    }

    body_record &row = claim_row(created->GetID(), target);
    row.kind = record_kind::BODY;
    row.motion = motion;
    row.layer = layer;
    row.isTrigger = built.isTrigger;
    row.hasParent = world->has_raw(target, ids.parent);
    row.movesWithTransform = motion == motion_type::KINEMATIC
      && has_flag(authored.flags, rigid_body_flags::KINEMATIC_MOVES_WITH_TRANSFORM);
    row.implicitStatic = body == nullptr;
    row.shapeKey = built.leafKey;
    row.mirror = authored;
    row.lastPulled = pose;
    row.lastScale = pose.scale;
    row.convexAsset = built.convexAsset;
    row.meshAsset = built.meshAsset;
    row.staticSensor = staticSensor;
    if (staticSensor) {
        ++staticSensors;
    }

    // A static trigger is added active (START_ASLEEP means nothing on a static body): only an active sensor
    // sees sleeping bodies.
    const bool startAsleep = !staticSensor
      && (motion == motion_type::STATIC || has_flag(authored.flags, rigid_body_flags::START_ASLEEP));
    if (startAsleep) {
        sleepBatch.push_back(row.bodyId);
    } else {
        activateBatch.push_back(row.bodyId);
    }
    // A static body never carries interpolated_transform (3.3 step 6), also when the component was queued
    // while the entity was pending as a dynamic body (the default rigid_body, later switched to STATIC or
    // removed so that the colliders make an implicit static body).
    if (motion == motion_type::STATIC) {
        remove_interpolated(target, deferred);
    } else {
        add_interpolated(target, deferred);
    }
    return true;
}

void physics_world::state::destroy_body(u32 rowIndex, ecs::command_buffer *deferred, bool synthesiseExits) {
    body_record &row = records[rowIndex];
    if (row.is_free()) {
        return;
    }
    if (synthesiseExits) {
        synthesise_exits(row);
    } else {
        pairScratch.clear();
        const u32 bodyValue = row.bodyId.GetIndexAndSequenceNumber();
        for (const auto &pair : activePairs) {
            const u32 bodyA = static_cast<u32>(pair.first >> 32);
            const u32 bodyB = static_cast<u32>(pair.first & 0xFFFFFFFFu);
            if (bodyA == bodyValue || bodyB == bodyValue) {
                pairScratch.push_back(pair.first);
            }
        }
        for (const u64 key : pairScratch) {
            activePairs.erase(key);
        }
    }

    JPH::BodyInterface &bodies = physicsSystem->GetBodyInterface();
    if (row.kind == record_kind::CHARACTER) {
        // The character destroys its inner body in its destructor.
        row.character = nullptr;
        if (characterCount > 0) {
            --characterCount;
        }
    } else {
        if (bodies.IsAdded(row.bodyId)) {
            bodies.RemoveBody(row.bodyId);
        }
        bodies.DestroyBody(row.bodyId);
    }
    if (row.staticSensor && staticSensors > 0) {
        --staticSensors;
    }

    const ecs::entity target = row.entity;
    rowOfEntity.erase(target);
    if (target.index < liveRowByEntityIndex.size() && liveRowByEntityIndex[target.index] == rowIndex) {
        liveRowByEntityIndex[target.index] = NO_ROW;
    }
    awake[rowIndex].store(0, std::memory_order_relaxed);
    // A pendingRebuild entry for this entity stays in the list: the row is the truth (dirty is reset
    // here), so apply_rebuilds skips it, or rebuilds the entity's next body only if that one is dirty.
    row = body_record();
    --liveRows;

    if (deferred != nullptr && world->is_alive(target)) {
        remove_interpolated(target, *deferred);
    }
}

void physics_world::state::destroy_all_bodies() {
    for (usize index = 0; index < records.size(); ++index) {
        if (!records[index].is_free()) {
            destroy_body(static_cast<u32>(index), nullptr, false);
        }
    }
    records.clear();
    rowOfEntity.clear();
    liveRowByEntityIndex.clear();
    pendingCreate.clear();
    pendingRebuild.clear();
    activePairs.clear();
    characterCount = 0;
    staticSensors = 0;
    liveRows = 0;
}


/*===========================================================================*/
/*===========================================================================*/
// Rebuilds and property sync

void physics_world::state::apply_body_properties(body_record &row, const rigid_body &body) {
    JPH::BodyInterface &bodies = physicsSystem->GetBodyInterface();
    const physics_material *material = read<physics_material>(row.entity);
    const physics_material surface = material == nullptr ? physics_material() : *material;
    bodies.SetFriction(row.bodyId, surface.friction);
    bodies.SetRestitution(row.bodyId, surface.restitution);
    if (row.motion == motion_type::STATIC) {
        return;
    }
    bodies.SetGravityFactor(row.bodyId, body.gravityFactor);
    bodies.SetMotionQuality(
      row.bodyId,
      has_flag(
        body.flags,
        rigid_body_flags::CONTINUOUS_COLLISION
      ) ? JPH::EMotionQuality::LinearCast : JPH::EMotionQuality::Discrete
    );
    const JPH::BodyLockWrite lock(physicsSystem->GetBodyLockInterface(), row.bodyId);
    if (!lock.Succeeded()) {
        return;
    }
    JPH::Body &joltBody = lock.GetBody();
    JPH::MotionProperties *motion = joltBody.GetMotionPropertiesUnchecked();
    if (motion == nullptr) {
        return;
    }
    motion->SetLinearDamping(std::max(body.linearDamping, 0.0f));
    motion->SetAngularDamping(std::max(body.angularDamping, 0.0f));
    motion->SetMaxLinearVelocity(std::max(body.maxLinearVelocity, 0.0f));
    joltBody.SetAllowSleeping(has_flag(body.flags, rigid_body_flags::ALLOW_SLEEPING));
    joltBody.SetApplyGyroscopicForce(has_flag(body.flags, rigid_body_flags::USE_GYROSCOPIC_FORCE));
    // Mass and the lock flags (allowed degrees of freedom) live in the same Jolt call; the body is
    // locked already, so the shared helper's lock would be a second one on this thread.
    JPH::MassProperties massProperties = joltBody.GetShape()->GetMassProperties();
    if (body.mass > 0.0f) {
        massProperties.ScaleToMass(body.mass);
    }
    if (massProperties.mMass > 0.0f) {
        motion->SetMassProperties(allowed_dofs(body.flags, row.entity), massProperties);
    }
}

void physics_world::state::apply_mass_properties(body_record &row, const rigid_body &body) {
    if (row.motion == motion_type::STATIC) {
        return;
    }
    const JPH::BodyLockWrite lock(physicsSystem->GetBodyLockInterface(), row.bodyId);
    if (!lock.Succeeded()) {
        return;
    }
    JPH::Body &joltBody = lock.GetBody();
    JPH::MotionProperties *motion = joltBody.GetMotionPropertiesUnchecked();
    if (motion == nullptr) {
        return;
    }
    // The derivation of BodyCreationSettings::GetMassProperties: the shape's own mass and inertia,
    // scaled to the authored mass when one is given (CalculateInertia), else taken as is (mass 0). A
    // shape without a volume (a kinematic mesh) reports no mass and keeps what it has.
    JPH::MassProperties massProperties = joltBody.GetShape()->GetMassProperties();
    if (body.mass > 0.0f) {
        massProperties.ScaleToMass(body.mass);
    }
    if (massProperties.mMass > 0.0f) {
        motion->SetMassProperties(allowed_dofs(body.flags, row.entity), massProperties);
    }
}

void physics_world::state::apply_motion_type(body_record &row, const rigid_body &body, ecs::command_buffer &deferred) {
    JPH::BodyInterface &bodies = physicsSystem->GetBodyInterface();
    const motion_type previous = row.motion;
    const u8 layer = resolve_layer(row.entity, body.layer);
    if (previous != body.motionType) {
        if (row.implicitStatic) {
            // A collider-only body was created without motion properties; it must be rebuilt.
            const u32 index = static_cast<u32>(&row - records.data());
            const ecs::entity target = row.entity;
            // The replacement is never static (an implicit static is STATIC and the motion changed), so
            // interpolated_transform stays; create_body adds it when missing.
            destroy_body(index, nullptr, true);
            mark_pending_create(target);
            // Called from push_changed_components, this runs after the push's first flush of pending
            // creations; push_transforms flushes again so that no fixed step runs without the body.
            replacedInPush = true;
            return;
        }
        row.motion = body.motionType;
        if (body.motionType == motion_type::STATIC) {
            remove_interpolated(row.entity, deferred);
        } else if (previous == motion_type::STATIC) {
            add_interpolated(row.entity, deferred);
            row.firstPull = true;
        }
    }
    // A row that reaches this line was authored through rigid_body or kept its motion type, so its body can
    // take whatever the row asks of it and the call cannot fail (see sync_jolt_motion).
    const bool synced = sync_jolt_motion(row);
    CONJURE_ASSERT(synced, "physics_world: a body with a rigid_body has no motion properties");
    CONJURE_UNUSED(synced);
    bodies.SetObjectLayer(row.bodyId, jolt::object_layer(layer, row.motion));
    row.layer = layer;
    row.movesWithTransform = row.motion == motion_type::KINEMATIC
      && has_flag(body.flags, rigid_body_flags::KINEMATIC_MOVES_WITH_TRANSFORM);
}

bool physics_world::state::sync_jolt_motion(body_record &row) {
    const bool staticSensor = is_static_sensor(row.motion, row.isTrigger);
    const JPH::EMotionType wanted = to_jolt(row.motion, row.isTrigger);
    JPH::EMotionType current = JPH::EMotionType::Static;
    {
        const JPH::BodyLockWrite lock(physicsSystem->GetBodyLockInterface(), row.bodyId);
        if (!lock.Succeeded()) {
            return true;
        }
        JPH::Body &joltBody = lock.GetBody();
        if (wanted != JPH::EMotionType::Static && !joltBody.CanBeKinematicOrDynamic()) {
            return false;
        }
        current = joltBody.GetMotionType();
        joltBody.SetIsSensor(row.isTrigger);
        joltBody.SetCollideKinematicVsNonDynamic(row.isTrigger && row.motion == motion_type::KINEMATIC);
    }
    JPH::BodyInterface &bodies = physicsSystem->GetBodyInterface();
    // SetMotionType puts a body that becomes static to sleep itself. A body that starts to move wakes, and a
    // static trigger is active always; a body that keeps its motion type keeps its sleep state.
    const bool activate = staticSensor || (wanted != current && wanted != JPH::EMotionType::Static);
    bodies.SetMotionType(
      row.bodyId,
      wanted,
      activate ? JPH::EActivation::Activate : JPH::EActivation::DontActivate
    );
    if (staticSensor && !row.staticSensor) {
        // Jolt moves a kinematic body by its velocity, and the body may have been moving until now.
        bodies.SetLinearAndAngularVelocity(row.bodyId, JPH::Vec3::sZero(), JPH::Vec3::sZero());
    }
    if (staticSensor != row.staticSensor) {
        staticSensors = staticSensor ? staticSensors + 1 : staticSensors - 1;
        row.staticSensor = staticSensor;
    }
    return true;
}

void physics_world::state::apply_rebuilds(ecs::command_buffer &deferred) {
    if (pendingRebuild.empty()) {
        return;
    }
    pendingScratch.assign(pendingRebuild.begin(), pendingRebuild.end());
    pendingRebuild.clear();

    for (const ecs::entity target : pendingScratch) {
        body_record *row = find_row(target);
        if (row == nullptr || !world->is_alive(target)) {
            continue;
        }
        const record_dirty_flags dirty = row->dirty;
        if (dirty == record_dirty_flags::NONE) {
            // The entry outlived the body it was made for (destroy_body resets the row).
            continue;
        }
        row->dirty = record_dirty_flags::NONE;

        if (row->kind == record_kind::CHARACTER) {
            const character_controller *controller = read<character_controller>(target);
            if (controller != nullptr) {
                rebuild_character(*row, *controller);
            }
            continue;
        }

        const rigid_body *body = read<rigid_body>(target);
        const rigid_body defaults = implicit_static_defaults();
        const rigid_body &authored = body == nullptr ? defaults : *body;
        if (body == nullptr && !row->implicitStatic) {
            // The rigid_body went away but colliders remain: the body becomes an implicit static one.
            const u32 index = static_cast<u32>(row - records.data());
            destroy_body(index, &deferred, true);
            mark_pending_create(target);
            continue;
        }
        if (has_flag(dirty, record_dirty_flags::MOTION) || has_flag(dirty, record_dirty_flags::LAYER)) {
            apply_motion_type(*row, authored, deferred);
            row = find_row(target);
            if (row == nullptr) {
                continue;
            }
        }
        if (has_flag(dirty, record_dirty_flags::SHAPE)) {
            const math::transform pose = compose_world_pose(target);
            const shape_build_input input = gather_colliders(target, row->motion, pose.scale);
            const shape_build_result built = build_shape(input, shapes);
            if (!built.hasCollider) {
                const u32 index = static_cast<u32>(row - records.data());
                destroy_body(index, &deferred, true);
                if (body != nullptr) {
                    mark_pending_create(target);
                }
                continue;
            }
            if (built.pendingAsset) {
                // The body keeps its current shape until every asset is loaded; the next push retries. The
                // row's dirty flags were reset above and pendingScratch is the list walked here, so the entry
                // lands in the next push's pendingRebuild.
                mark_rebuild(target, record_dirty_flags::SHAPE);
            }
            if (built.shape != nullptr) {
                // The sensor flags and, for a static body that became or stopped being a trigger, the motion
                // type Jolt holds (code sweep F408).
                row->isTrigger = built.isTrigger;
                if (!sync_jolt_motion(*row)) {
                    // An implicit static body was created without motion properties and cannot turn into the
                    // kinematic sensor of a static trigger: it is replaced, and the flush that follows in
                    // this push creates the trigger. Its contacts end with EXIT and start anew.
                    const u32 index = static_cast<u32>(row - records.data());
                    destroy_body(index, nullptr, true);
                    mark_pending_create(target);
                    continue;
                }
                JPH::BodyInterface &bodies = physicsSystem->GetBodyInterface();
                // Mass properties follow separately: SetShape alone would either keep the stale inertia
                // (false) or discard the authored mass (true).
                bodies.SetShape(row->bodyId, built.shape.GetPtr(), false, JPH::EActivation::Activate);
                apply_mass_properties(*row, authored);
                row->shapeKey = built.leafKey;
                row->lastScale = pose.scale;
                row->convexAsset = built.convexAsset;
                row->meshAsset = built.meshAsset;
            }
        }
        if (has_flag(dirty, record_dirty_flags::PROPERTIES)) {
            apply_body_properties(*row, authored);
        }
        row->mirror = authored;
    }
    pendingScratch.clear();
}
}

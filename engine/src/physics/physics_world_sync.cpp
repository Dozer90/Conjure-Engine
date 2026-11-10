#include "physics/jolt_conversions.h"
#include "physics/physics_world_internal.h"

#include "conjure/physics/components.h"
#include "conjure/physics/log_category.h"

#include "conjure/core/assert.h"
#include "conjure/core/clock.h"
#include "conjure/core/log.h"
#include "conjure/ecs/chunk_view.h"
#include "conjure/ecs/query.h"
#include "conjure/ecs/view.h"
#include "conjure/scene/components.h"

#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/EActivation.h>

#include <utility>

namespace conjure::physics {

namespace {

    [[nodiscard]] bool velocities_differ(const rigid_body &a, const rigid_body &b) noexcept {
        return a.linearVelocity != b.linearVelocity || a.angularVelocity != b.angularVelocity;
    }

    [[nodiscard]] bool properties_differ(const rigid_body &a, const rigid_body &b) noexcept {
        return a.mass != b.mass || a.linearDamping != b.linearDamping || a.angularDamping != b.angularDamping
          || a.gravityFactor != b.gravityFactor || a.maxLinearVelocity != b.maxLinearVelocity;
    }

    [[nodiscard]] bool pose_differs(const math::transform &a, const math::transform &b, f32 epsilon) noexcept {
        return !math::nearly_equal(
          a.position,
          b.position,
          epsilon
        ) || !math::same_rotation(a.rotation, b.rotation, epsilon);
    }

    [[nodiscard]] u64 microseconds_since(u64 startTicks) noexcept {
        return static_cast<u64>(core::ticks_to_seconds(core::ticks_now() - startTicks) * 1.0e6);
    }
}

/*===========================================================================*/
/*===========================================================================*/
// physics_world: step pipeline entry points

void physics_world::push_transforms(f32 fixedDeltaTime, ecs::command_buffer *deferred) {
    state &s = *mState;
    CONJURE_ASSERT(s.is_owner_thread(), "physics_world::push_transforms is owner-thread only");
    if (!s.is_owner_thread() || s.world == nullptr) {
        return;
    }
    const u64 start = core::ticks_now();
    // The row tables change below (creation, rebuilds, destruction through the flush), so queries
    // from other threads are refused until the end (STEP_IN_PROGRESS).
    s.busy.store(true, std::memory_order_release);

    // R6: the previous step's span dies here.
    s.events.clear();
    s.eventKeys.clear();
    s.stats.contactEvents = 0;
    s.contacts.clear();

    ecs::command_buffer &buffer = deferred != nullptr ? *deferred : *s.ownDeferred;
    s.flush_pending_creates(buffer);
    s.apply_rebuilds(buffer);
    s.push_changed_components(fixedDeltaTime, buffer);
    s.push_hierarchy_rows(fixedDeltaTime);
    s.apply_commands(fixedDeltaTime);

    if (deferred == nullptr && !s.ownDeferred->empty() && !s.world->is_locked()) {
        const ecs::flush_report report = s.world->flush(*s.ownDeferred);
        CONJURE_UNUSED(report);
    }
    s.busy.store(false, std::memory_order_release);
    s.stats.pushMicroseconds = microseconds_since(start);
}

void physics_world::step(f32 fixedDeltaTime) {
    state &s = *mState;
    CONJURE_ASSERT(s.is_owner_thread(), "physics_world::step is owner-thread only");
    if (!s.is_owner_thread() || fixedDeltaTime <= 0.0f) {
        return;
    }
    const u64 start = core::ticks_now();
    s.contacts.ensure_threads(s.jobs->worker_count() + 1);
    s.busy.store(true, std::memory_order_release);

    if (s.world != nullptr) {
        s.step_characters(fixedDeltaTime);
    }
    JPH::EPhysicsUpdateError error = JPH::EPhysicsUpdateError::None;
    s.jobSystem.run_parallel([&]() {
        error = s.physicsSystem->Update(
          fixedDeltaTime,
          static_cast<int>(s.settings.collisionSteps),
          s.tempAllocator.get(),
          &s.jobSystem
        );
    });

    s.busy.store(false, std::memory_order_release);
    if (error != JPH::EPhysicsUpdateError::None) {
        ++s.stats.updateErrors;
        if (s.updateErrorWarnings.should_log()) {
            const JPH::uint32 bits = static_cast<JPH::uint32>(error);
            const auto flagged = [bits](JPH::EPhysicsUpdateError flag) {
                return (bits & static_cast<JPH::uint32>(flag)) != 0;
            };
            CONJURE_LOG_WARN(
              LOG_PHYSICS,
              "PhysicsSystem::Update reported{}{}{}; raise maxContactConstraints / maxBodyPairs",
              flagged(JPH::EPhysicsUpdateError::ManifoldCacheFull) ? " ManifoldCacheFull" : "",
              flagged(JPH::EPhysicsUpdateError::BodyPairCacheFull) ? " BodyPairCacheFull" : "",
              flagged(JPH::EPhysicsUpdateError::ContactConstraintsFull) ? " ContactConstraintsFull" : ""
            );
        }
    }
    s.stats.stepMicroseconds = microseconds_since(start);
}

void physics_world::pull_transforms(ecs::system_context *context) {
    state &s = *mState;
    if (s.world == nullptr) {
        return;
    }
    const u64 start = core::ticks_now();
    // Rows are written from the chunk pass (worker threads) and the serial tail: no query meanwhile.
    s.busy.store(true, std::memory_order_release);
    if (context != nullptr) {
        ecs::view<rigid_body, scene::interpolated_transform, scene::local_transform> bodies =
          context->view<rigid_body, scene::interpolated_transform, scene::local_transform>();
        bodies.parallel_for_each_chunk(*context, [&s](ecs::chunk_view &chunk, ecs::command_buffer &) {
            s.pull_chunk(chunk);
        });
    } else {
        s.pullQuery->for_each_chunk([&s](ecs::chunk_view &chunk) {
            s.pull_chunk(chunk);
        });
    }
    s.pull_parented_rows();
    s.pull_characters();
    s.busy.store(false, std::memory_order_release);
    s.stats.pullMicroseconds = microseconds_since(start);
}


/*===========================================================================*/
/*===========================================================================*/
// state: push

void physics_world::state::push_changed_components(f32 fixedDeltaTime, ecs::command_buffer &deferred) {
    // A non-const access stamps its chunk with the world version current at that moment and the
    // changed filter keeps chunks stamped strictly after 'since'. The version only moves on structural
    // changes and scheduler runs, so the push bumps it itself: writes made after this push (including
    // the pull's own, which the pose diff below ignores) carry a version above the one remembered.
    const u64 since = sinceVersion;
    sinceVersion = world->version();
    world->bump_version();
    JPH::BodyInterface &bodies = physicsSystem->GetBodyInterface();

    const auto visitBody = [&](ecs::entity target, const rigid_body &body) {
        body_record *row = find_row(target);
        if (row == nullptr || row->kind != record_kind::BODY) {
            return;
        }
        if (body != row->mirror) {
            if (velocities_differ(body, row->mirror) && row->motion != motion_type::STATIC) {
                bodies.SetLinearAndAngularVelocity(
                  row->bodyId,
                  jolt::to_jolt(body.linearVelocity),
                  jolt::to_jolt(body.angularVelocity)
                );
            }
            record_dirty_flags dirty = record_dirty_flags::NONE;
            if (body.motionType != row->mirror.motionType || body.flags != row->mirror.flags) {
                dirty |= record_dirty_flags::MOTION | record_dirty_flags::PROPERTIES;
            }
            if (body.layer != row->mirror.layer) {
                dirty |= record_dirty_flags::LAYER;
            }
            if (properties_differ(body, row->mirror)) {
                dirty |= record_dirty_flags::PROPERTIES;
            }
            if (dirty != record_dirty_flags::NONE) {
                if (has_flag(dirty, record_dirty_flags::MOTION) || has_flag(dirty, record_dirty_flags::LAYER)) {
                    apply_motion_type(*row, body, deferred);
                    row = find_row(target);
                    if (row == nullptr) {
                        return;
                    }
                }
                if (has_flag(dirty, record_dirty_flags::PROPERTIES)) {
                    // Mass, damping, lock flags and gyroscopic force reach Jolt here, no shape rebuild.
                    apply_body_properties(*row, body);
                }
            }
            row->mirror = body;
        }

        if (row->movesWithTransform || row->hasParent) {
            // Kinematic rows that follow their transform and parented rows are re-posed by
            // push_hierarchy_rows every step.
            return;
        }
        const math::transform pose = compose_world_pose(target);
        if (!row->awaitingPull && pose_differs(pose, row->lastPulled, TELEPORT_EPSILON)) {
            teleport_row(*row, pose);
        }
        if (!math::nearly_equal(pose.scale, row->lastScale, 1.0e-6f)) {
            mark_rebuild(target, record_dirty_flags::SHAPE);
        }
    };
    ecs::query_desc bodyDesc = bodyChangeQuery->desc();
    bodyDesc.sinceVersion = since;
    bodyChangeQuery->reset(bodyDesc);
    bodyChangeQuery->for_each_chunk([&](ecs::chunk_view &chunk) {
        const std::span<const ecs::entity> entities = chunk.entities();
        const std::span<const rigid_body> column = chunk.get<const rigid_body>();
        const u32 count = chunk.count();
        for (u32 index = 0; index < count; ++index) {
            visitBody(entities[index], column[index]);
        }
    });
    CONJURE_UNUSED(fixedDeltaTime);

    // Characters teleported or reshaped through their components.
    const auto visitCharacter = [&](ecs::entity target, const character_controller &controller) {
        body_record *row = find_row(target);
        if (row == nullptr || row->kind != record_kind::CHARACTER || row->character == nullptr) {
            return;
        }
        if (controller.radius != row->characterMirror.radius || controller.height != row->characterMirror.height
          || controller.layer != row->characterMirror.layer || controller.padding != row->characterMirror.padding
          || controller.maxSlopeAngleDegrees != row->characterMirror.maxSlopeAngleDegrees
          || controller.stepHeight != row->characterMirror.stepHeight || controller.mass != row->characterMirror.mass
          || controller.maxStrength != row->characterMirror.maxStrength
          || controller.flags != row->characterMirror.flags) {
            mark_rebuild(target, record_dirty_flags::SHAPE);
        }
        if (row->hasParent) {
            return;   // push_hierarchy_rows
        }
        const math::transform pose = compose_world_pose(target);
        if (!row->awaitingPull && pose_differs(pose, row->lastPulled, TELEPORT_EPSILON)) {
            teleport_row(*row, pose);
        }
    };
    ecs::query_desc characterDesc = characterChangeQuery->desc();
    characterDesc.sinceVersion = since;
    characterChangeQuery->reset(characterDesc);
    characterChangeQuery->for_each_chunk([&](ecs::chunk_view &chunk) {
        const std::span<const ecs::entity> entities = chunk.entities();
        const std::span<const character_controller> column = chunk.get<const character_controller>();
        const u32 count = chunk.count();
        for (u32 index = 0; index < count; ++index) {
            visitCharacter(entities[index], column[index]);
        }
    });
}

void physics_world::state::push_hierarchy_rows(f32 fixedDeltaTime) {
    JPH::BodyInterface &bodies = physicsSystem->GetBodyInterface();
    for (body_record &row : records) {
        if (row.is_free()) {
            continue;
        }
        if (row.kind == record_kind::BODY && row.movesWithTransform) {
            const math::transform pose = compose_world_pose(row.entity);
            bodies.MoveKinematic(
              row.bodyId,
              jolt::to_jolt(pose.position),
              jolt::to_jolt(pose.rotation),
              fixedDeltaTime
            );
            row.lastPulled = pose;
            if (!math::nearly_equal(pose.scale, row.lastScale, 1.0e-6f)) {
                mark_rebuild(row.entity, record_dirty_flags::SHAPE);
            }
            continue;
        }
        if (!row.hasParent) {
            continue;
        }
        // The changed filter of push_changed_components only sees the row's own columns, so a parented
        // body or character is composed every push: an ancestor moved by gameplay or by its own body
        // would otherwise never reach Jolt. Parented rows are rare, so the walk is cheap.
        const math::transform pose = compose_world_pose(row.entity);
        if (!row.awaitingPull && pose_differs(pose, row.lastPulled, TELEPORT_EPSILON)) {
            teleport_row(row, pose);
        }
        if (row.kind == record_kind::BODY && !math::nearly_equal(pose.scale, row.lastScale, 1.0e-6f)) {
            mark_rebuild(row.entity, record_dirty_flags::SHAPE);
        }
    }
}

void physics_world::state::teleport_row(body_record &row, const math::transform &pose) {
    if (row.kind == record_kind::CHARACTER) {
        if (row.character == nullptr) {
            return;
        }
        row.character->SetPosition(jolt::to_jolt(pose.position));
        row.character->SetRotation(jolt::to_jolt(pose.rotation));
    } else {
        physicsSystem->GetBodyInterface().SetPositionAndRotation(
          row.bodyId,
          jolt::to_jolt(pose.position),
          jolt::to_jolt(pose.rotation),
          row.motion == motion_type::STATIC ? JPH::EActivation::DontActivate : JPH::EActivation::Activate
        );
    }
    row.lastPulled.position = pose.position;
    row.lastPulled.rotation = pose.rotation;
    scene::interpolated_transform *interpolated = write<scene::interpolated_transform>(row.entity);
    if (interpolated != nullptr) {
        interpolated->previous.position = pose.position;
        interpolated->previous.rotation = pose.rotation;
        interpolated->current.position = pose.position;
        interpolated->current.rotation = pose.rotation;
    }
}

void physics_world::state::write_local_pose(ecs::entity target, const math::transform &worldPose) {
    scene::local_transform *local = write<scene::local_transform>(target);
    if (local == nullptr) {
        return;
    }
    const scene::parent *link = read<scene::parent>(target);
    if (link == nullptr || link->value.is_null() || !world->is_alive(link->value)) {
        local->position = worldPose.position;
        local->rotation = worldPose.rotation;
        return;
    }
    const math::transform parentWorld = compose_world_pose(link->value);
    const math::transform relative = parentWorld.inverse() * worldPose;
    local->position = relative.position;
    local->rotation = relative.rotation;
}

void physics_world::state::apply_commands(f32 fixedDeltaTime) {
    if (commands.empty()) {
        return;
    }
    JPH::BodyInterface &bodies = physicsSystem->GetBodyInterface();
    commandsScratch.clear();
    for (const body_command &command : commands) {
        if (command.type == body_command::kind::SET_GRAVITY) {
            gravity = command.vectorA;
            physicsSystem->SetGravity(jolt::to_jolt(gravity));
            continue;
        }
        body_record *row = find_row(command.target);
        if (row == nullptr) {
            if (is_pending(command.target) && !command.kept) {
                body_command carried = command;
                carried.kept = true;
                commandsScratch.push_back(carried);
            } else {
                ++stats.droppedCommands;
            }
            continue;
        }
        if (row->kind == record_kind::CHARACTER) {
            if (command.type == body_command::kind::CHARACTER_MOVE) {
                // Kept in the row: the push declares character_controller as a read.
                row->desiredVelocity = command.vectorA;
                row->hasDesiredVelocity = true;
            } else if (command.type == body_command::kind::TELEPORT && row->character != nullptr) {
                row->character->SetPosition(jolt::to_jolt(command.vectorA));
                row->character->SetRotation(jolt::to_jolt(command.rotation));
                row->lastPulled.position = command.vectorA;
                row->lastPulled.rotation = command.rotation;
                row->awaitingPull = true;
                row->firstPull = true;
            } else {
                ++stats.droppedCommands;
            }
            continue;
        }
        const JPH::BodyID id = row->bodyId;
        switch (command.type) {
            case body_command::kind::ADD_FORCE:
                bodies.AddForce(id, jolt::to_jolt(command.vectorA));
                break;
            case body_command::kind::ADD_FORCE_AT:
                bodies.AddForce(id, jolt::to_jolt(command.vectorA), jolt::to_jolt(command.vectorB));
                break;
            case body_command::kind::ADD_IMPULSE:
                bodies.AddImpulse(id, jolt::to_jolt(command.vectorA));
                break;
            case body_command::kind::ADD_IMPULSE_AT:
                bodies.AddImpulse(id, jolt::to_jolt(command.vectorA), jolt::to_jolt(command.vectorB));
                break;
            case body_command::kind::ADD_TORQUE:
                bodies.AddTorque(id, jolt::to_jolt(command.vectorA));
                break;
            case body_command::kind::ADD_ANGULAR_IMPULSE:
                bodies.AddAngularImpulse(id, jolt::to_jolt(command.vectorA));
                break;
            case body_command::kind::SET_LINEAR_VELOCITY:
                bodies.SetLinearVelocity(id, jolt::to_jolt(command.vectorA));
                row->mirror.linearVelocity = command.vectorA;
                break;
            case body_command::kind::SET_ANGULAR_VELOCITY:
                bodies.SetAngularVelocity(id, jolt::to_jolt(command.vectorA));
                row->mirror.angularVelocity = command.vectorA;
                break;
            case body_command::kind::TELEPORT: {
                bodies.SetPositionAndRotation(
                  id,
                  jolt::to_jolt(command.vectorA),
                  jolt::to_jolt(command.rotation),
                  row->motion == motion_type::STATIC ? JPH::EActivation::DontActivate : JPH::EActivation::Activate
                );
                if (row->motion != motion_type::STATIC) {
                    bodies.SetLinearAndAngularVelocity(id, JPH::Vec3::sZero(), JPH::Vec3::sZero());
                    row->mirror.linearVelocity = math::vec3(0.0f);
                    row->mirror.angularVelocity = math::vec3(0.0f);
                }
                row->lastPulled.position = command.vectorA;
                row->lastPulled.rotation = command.rotation;
                scene::interpolated_transform *interpolated = write<scene::interpolated_transform>(row->entity);
                if (interpolated != nullptr) {
                    interpolated->previous = row->lastPulled;
                    interpolated->current = row->lastPulled;
                }
                if (row->motion == motion_type::STATIC || row->movesWithTransform) {
                    // The pull never visits this row (3.7), so the transform is written now; the next
                    // push then compares later edits of it against the teleported pose.
                    write_local_pose(row->entity, row->lastPulled);
                } else {
                    row->awaitingPull = true;
                    row->firstPull = true;
                }
                break;
            }
            case body_command::kind::MOVE_KINEMATIC:
                if (row->motion == motion_type::KINEMATIC) {
                    bodies.MoveKinematic(
                      id,
                      jolt::to_jolt(command.vectorA),
                      jolt::to_jolt(command.rotation),
                      fixedDeltaTime
                    );
                    // A row driven by its transform is re-posed every push and never pulled.
                    row->awaitingPull = !row->movesWithTransform;
                } else {
                    ++stats.droppedCommands;
                }
                break;
            case body_command::kind::ACTIVATE:
                if (row->motion != motion_type::STATIC) {
                    bodies.ActivateBody(id);
                    awake[id.GetIndex()].store(1, std::memory_order_relaxed);
                }
                break;
            case body_command::kind::DEACTIVATE:
                if (row->motion != motion_type::STATIC) {
                    bodies.DeactivateBody(id);
                    awake[id.GetIndex()].store(0, std::memory_order_relaxed);
                }
                break;
            case body_command::kind::CHARACTER_MOVE:
                ++stats.droppedCommands;
                break;
            case body_command::kind::SET_GRAVITY:
                break;
        }
    }
    commands.swap(commandsScratch);
    commandsScratch.clear();
}


/*===========================================================================*/
/*===========================================================================*/
// state: pull

void physics_world::state::pull_chunk(ecs::chunk_view &chunk) {
    const std::span<const ecs::entity> entities = chunk.entities();
    const u32 count = chunk.count();

    // First pass: does any row of this chunk need writing? Acquiring a non-const span stamps the
    // chunk, so a chunk of sleeping bodies must not be touched.
    bool anyWrite = false;
    for (u32 index = 0; index < count && !anyWrite; ++index) {
        const body_record *row = find_row(entities[index]);
        if (row == nullptr || row->kind != record_kind::BODY || row->motion == motion_type::STATIC
          || row->movesWithTransform || row->hasParent) {
            continue;
        }
        anyWrite = row->firstPull || row->awaitingPull || is_awake(*row) || !row->mirror.isSleeping;
    }
    if (!anyWrite) {
        return;
    }

    const std::span<rigid_body> bodies = chunk.get<rigid_body>();
    const std::span<scene::interpolated_transform> interpolated = chunk.get<scene::interpolated_transform>();
    const std::span<scene::local_transform> locals = chunk.get<scene::local_transform>();
    const JPH::BodyInterface &joltBodies = physicsSystem->GetBodyInterfaceNoLock();
    for (u32 index = 0; index < count; ++index) {
        body_record *row = find_row(entities[index]);
        if (row == nullptr || row->kind != record_kind::BODY || row->motion == motion_type::STATIC
          || row->movesWithTransform || row->hasParent) {
            continue;
        }
        const bool awakeNow = is_awake(*row);
        if (!awakeNow && row->mirror.isSleeping && !row->firstPull && !row->awaitingPull) {
            continue;
        }
        JPH::RVec3 position = {};
        JPH::Quat rotation = {};
        joltBodies.GetPositionAndRotation(row->bodyId, position, rotation);
        math::transform current = {};
        current.position = jolt::from_jolt(position);
        current.rotation = jolt::from_jolt(rotation);
        current.scale = locals[index].scale;

        interpolated[index].previous = row->firstPull ? current : interpolated[index].current;
        interpolated[index].current = current;
        row->firstPull = false;
        row->awaitingPull = false;
        row->lastPulled = current;

        rigid_body &body = bodies[index];
        body.linearVelocity = jolt::from_jolt(joltBodies.GetLinearVelocity(row->bodyId));
        body.angularVelocity = jolt::from_jolt(joltBodies.GetAngularVelocity(row->bodyId));
        body.isSleeping = !awakeNow;
        row->mirror = body;

        locals[index].position = current.position;
        locals[index].rotation = current.rotation;
    }
}

void physics_world::state::pull_parented_rows() {
    const JPH::BodyInterface &joltBodies = physicsSystem->GetBodyInterfaceNoLock();
    for (body_record &row : records) {
        if (row.is_free() || row.kind != record_kind::BODY || !row.hasParent || row.motion == motion_type::STATIC
          || row.movesWithTransform) {
            continue;
        }
        const bool awakeNow = is_awake(row);
        if (!awakeNow && row.mirror.isSleeping && !row.firstPull && !row.awaitingPull) {
            continue;
        }
        rigid_body *body = write<rigid_body>(row.entity);
        if (body == nullptr || !world->has_raw(row.entity, ids.localTransform)) {
            continue;
        }
        JPH::RVec3 position = {};
        JPH::Quat rotation = {};
        joltBodies.GetPositionAndRotation(row.bodyId, position, rotation);
        math::transform current = {};
        current.position = jolt::from_jolt(position);
        current.rotation = jolt::from_jolt(rotation);
        current.scale = row.lastScale;

        scene::interpolated_transform *interpolated = write<scene::interpolated_transform>(row.entity);
        if (interpolated != nullptr) {
            interpolated->previous = row.firstPull ? current : interpolated->current;
            interpolated->current = current;
        }
        row.firstPull = false;
        row.awaitingPull = false;
        row.lastPulled = current;

        body->linearVelocity = jolt::from_jolt(joltBodies.GetLinearVelocity(row.bodyId));
        body->angularVelocity = jolt::from_jolt(joltBodies.GetAngularVelocity(row.bodyId));
        body->isSleeping = !awakeNow;
        row.mirror = *body;
        write_local_pose(row.entity, current);
    }
}
}

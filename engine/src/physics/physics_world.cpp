#include "conjure/physics/physics_world.h"

#include "physics/jolt_conversions.h"
#include "physics/physics_world_internal.h"

#include "conjure/physics/components.h"
#include "conjure/physics/log_category.h"
#include "conjure/physics/physics_error.h"

#include "conjure/core/assert.h"
#include "conjure/core/jobs/job_system.h"
#include "conjure/core/log.h"
#include "conjure/scene/components.h"

#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/BodyLockInterface.h>
#include <Jolt/Physics/Body/BodyType.h>
#include <Jolt/Physics/PhysicsSettings.h>

#include <algorithm>
#include <format>
#include <utility>

namespace conjure::physics {

/*===========================================================================*/
/*===========================================================================*/
// physics_component_ids

bool physics_component_ids::is_complete() const noexcept {
    return rigidBody.is_valid() && box.is_valid() && sphere.is_valid() && capsule.is_valid() && convex.is_valid()
      && mesh.is_valid() && material.is_valid() && character.is_valid() && parent.is_valid()
      && localTransform.is_valid() && interpolatedTransform.is_valid();
}

bool physics_component_ids::is_collider(ecs::component_id id) const noexcept {
    return id == box || id == sphere || id == capsule || id == convex || id == mesh;
}

bool physics_component_ids::is_body_defining(ecs::component_id id) const noexcept {
    return id == rigidBody || id == character || is_collider(id);
}


/*===========================================================================*/
/*===========================================================================*/
// state

physics_world::state::state(const physics_settings &requested, core::jobs::job_system &engineJobs) :
    settings(requested),
    jobs(&engineJobs),
    ownerThread(std::this_thread::get_id()) {
    const core::result<void> valid = settings.validate();
    if (!valid.has_value()) {
        // The contract asks callers to validate first; clamping keeps the world usable (6).
        CONJURE_LOG_ERROR(
          LOG_PHYSICS,
          "physics_settings invalid ({}); defaults substituted",
          valid.get_error().to_string()
        );
        physics_settings fallback = {};
        fallback.layers = settings.layers.validate().has_value() ? settings.layers : physics_layer_table();
        settings = fallback;
    }
    gravity = settings.gravity;

    const u32 threadCount = jobs->worker_count() + 1;
    jobSystem.init(settings.maxJobs, settings.maxBarriers, *jobs);
    tempAllocator = std::make_unique<JPH::TempAllocatorImpl>(settings.tempAllocatorBytes);
    layerTables = std::make_unique<jolt::layer_tables>(settings.layers);
    physicsSystem = std::make_unique<JPH::PhysicsSystem>();
    physicsSystem->Init(
      settings.maxBodies,
      settings.numBodyMutexes,
      settings.maxBodyPairs,
      settings.maxContactConstraints,
      layerTables->broad_phase(),
      layerTables->object_vs_broad_phase(),
      layerTables->pair_filter()
    );
    physicsSystem->SetGravity(jolt::to_jolt(gravity));

    JPH::PhysicsSettings joltSettings = {};
    joltSettings.mDeterministicSimulation = settings.deterministic;
    joltSettings.mAllowSleeping = settings.allowSleeping;
    joltSettings.mTimeBeforeSleep = settings.timeBeforeSleepSeconds;
    joltSettings.mPointVelocitySleepThreshold = settings.pointVelocitySleepThreshold;
    joltSettings.mNumVelocitySteps = settings.numVelocitySteps;
    joltSettings.mNumPositionSteps = settings.numPositionSteps;
    joltSettings.mSpeculativeContactDistance = settings.speculativeContactDistance;
    joltSettings.mPenetrationSlop = settings.penetrationSlop;
    physicsSystem->SetPhysicsSettings(joltSettings);

    awake = std::make_unique<std::atomic<u8>[]>(settings.maxBodies);
    activation.bind(awake.get(), settings.maxBodies);
    contacts.init(threadCount, settings.maxContactEventsPerStep, settings.estimateContactImpulses);
    physicsSystem->SetContactListener(&contacts);
    physicsSystem->SetBodyActivationListener(&activation);

    // P15: every container is sized once; liveRowByEntityIndex is the exception (sized by entity index,
    // grown by claim_row: physics_world_internal.h).
    const usize initialRows = std::min<usize>(settings.maxBodies, ROW_BLOCK);
    records.reserve(initialRows);
    rowOfEntity.reserve(std::max<usize>(initialRows, 64));
    pendingCreate.reserve(initialRows);
    pendingScratch.reserve(initialRows);
    pendingRebuild.reserve(initialRows);
    commands.reserve(settings.maxQueuedCommands);
    commandsScratch.reserve(settings.maxQueuedCommands);
    activateBatch.reserve(initialRows);
    sleepBatch.reserve(initialRows);
    shapes.reserve(256);
    activePairs.reserve(settings.maxContactEventsPerStep);
    events.reserve(settings.maxContactEventsPerStep);
    eventKeys.reserve(settings.maxContactEventsPerStep);
    eventScratch.reserve(settings.maxContactEventsPerStep);
    rawMerged.reserve(settings.maxContactEventsPerStep);
    sortKeys.reserve(settings.maxContactEventsPerStep);
    sortScratchKeys.reserve(settings.maxContactEventsPerStep);
    sortValues.reserve(settings.maxContactEventsPerStep);
    sortScratchValues.reserve(settings.maxContactEventsPerStep);
    pairScratch.reserve(settings.maxContactEventsPerStep);
    pendingExits.reserve(settings.maxContactEventsPerStep);
    pendingExitKeys.reserve(settings.maxContactEventsPerStep);
}

physics_world::state::~state() {
    physicsSystem->SetContactListener(nullptr);
    physicsSystem->SetBodyActivationListener(nullptr);
    // Shapes die after the bodies that reference them; the Jolt system goes before the runtime guard.
    records.clear();
    shapes.clear();
    physicsSystem.reset();
    layerTables.reset();
    tempAllocator.reset();
}

bool physics_world::state::is_owner_thread() const noexcept {
    return std::this_thread::get_id() == ownerThread;
}

// A live row is found through the dense entity-index table (one load and the generation check the
// row's entity provides) rather than the hash map: the pull resolves every body this way each step.
// Pending markers have no row, so they are correctly absent here.

const body_record *physics_world::state::find_row(ecs::entity target) const noexcept {
    if (target.index >= liveRowByEntityIndex.size()) {
        return nullptr;
    }
    const u32 rowIndex = liveRowByEntityIndex[target.index];
    if (rowIndex == NO_ROW || records[rowIndex].entity != target) {
        return nullptr;
    }
    return &records[rowIndex];
}

body_record *physics_world::state::find_row(ecs::entity target) noexcept {
    if (target.index >= liveRowByEntityIndex.size()) {
        return nullptr;
    }
    const u32 rowIndex = liveRowByEntityIndex[target.index];
    if (rowIndex == NO_ROW || records[rowIndex].entity != target) {
        return nullptr;
    }
    return &records[rowIndex];
}

bool physics_world::state::is_pending(ecs::entity target) const noexcept {
    const auto found = rowOfEntity.find(target);
    return found != rowOfEntity.end() && found->second == PENDING_ROW;
}

body_record *physics_world::state::row_of_body(u32 bodyIdValue) noexcept {
    const JPH::BodyID bodyId(bodyIdValue);
    const u32 index = bodyId.GetIndex();
    if (index >= records.size()) {
        return nullptr;
    }
    body_record &row = records[index];
    if (row.is_free() || row.bodyId != bodyId) {
        return nullptr;
    }
    return &row;
}

const body_record *physics_world::state::row_of_body(u32 bodyIdValue) const noexcept {
    const JPH::BodyID bodyId(bodyIdValue);
    const u32 index = bodyId.GetIndex();
    if (index >= records.size()) {
        return nullptr;
    }
    const body_record &row = records[index];
    if (row.is_free() || row.bodyId != bodyId) {
        return nullptr;
    }
    return &row;
}

bool physics_world::state::is_awake(const body_record &row) const noexcept {
    const u32 index = row.bodyId.GetIndex();
    if (index >= settings.maxBodies) {
        return false;
    }
    return awake[index].load(std::memory_order_relaxed) != 0;
}

command_target physics_world::state::target_of_components(ecs::entity target) const noexcept {
    if (read<character_controller>(target) != nullptr) {
        // The character wins over a rigid_body on the same entity (CONFLICTING_BODY_COMPONENTS).
        return command_target::CHARACTER;
    }
    const rigid_body *body = read<rigid_body>(target);
    if (body == nullptr) {
        return command_target::STATIC_BODY;
    }
    switch (body->motionType) {
        case motion_type::DYNAMIC:
            return command_target::DYNAMIC_BODY;
        case motion_type::KINEMATIC:
            return has_flag(body->flags, rigid_body_flags::KINEMATIC_MOVES_WITH_TRANSFORM)
              ? command_target::KINEMATIC_BODY_FOLLOWING_TRANSFORM
              : command_target::KINEMATIC_BODY;
        case motion_type::STATIC:
            break;
    }
    return command_target::STATIC_BODY;
}

command_target physics_world::state::target_of_row(const body_record &row) noexcept {
    if (row.kind == record_kind::CHARACTER) {
        return command_target::CHARACTER;
    }
    switch (row.motion) {
        case motion_type::DYNAMIC:
            return command_target::DYNAMIC_BODY;
        case motion_type::KINEMATIC:
            return row.movesWithTransform
              ? command_target::KINEMATIC_BODY_FOLLOWING_TRANSFORM
              : command_target::KINEMATIC_BODY;
        case motion_type::STATIC:
            break;
    }
    return command_target::STATIC_BODY;
}

core::result<void> physics_world::state::check_command_target(const body_command &command) const {
    if (command.type == body_command::kind::SET_GRAVITY) {
        return core::result<void>::success();
    }
    // character_move keeps the code it always reported for a target that is not a character.
    const bool characterMove = command.type == body_command::kind::CHARACTER_MOVE;
    if (find_row(command.target) == nullptr) {
        if (is_pending(command.target)) {
            // No body yet, so no kind to refuse by: the push that creates it applies or drops the command.
            return core::result<void>::success();
        }
        if (characterMove) {
            return core::make_error(physics_error::NOT_A_CHARACTER, "entity {} has no character", command.target);
        }
        return core::make_error(physics_error::NO_BODY, "entity {} has no body", command.target);
    }
    const command_target kind = target_of_components(command.target);
    if (command_applies(command.type, kind)) {
        return core::result<void>::success();
    }
    if (characterMove) {
        return core::make_error(physics_error::NOT_A_CHARACTER, "entity {} is not a character", command.target);
    }
    return core::make_error(
      physics_error::WRONG_BODY_KIND,
      "{} does not apply to entity {}, which is {}",
      command_name(command.type),
      command.target,
      command_target_name(kind)
    );
}

core::result<void> physics_world::state::queue(body_command command) {
    if (!is_owner_thread()) {
        CONJURE_ASSERT(false, "physics_world: body commands are owner-thread only");
        return core::make_error(physics_error::WRONG_THREAD, "body commands are owner-thread only");
    }
    // A NaN or infinity reaching Jolt poisons the body (and, through contacts, its neighbours) for good.
    if (!is_finite(command.vectorA) || !is_finite(command.vectorB) || !is_finite(command.rotation)) {
        return core::error(
          core::error_code::INVALID_ARGUMENT,
          std::format("non-finite value in a body command for entity {}", command.target)
        );
    }
    const core::result<void> accepted = check_command_target(command);
    if (!accepted.has_value()) {
        return accepted;
    }
    if (commands.size() >= settings.maxQueuedCommands) {
        ++stats.droppedCommands;
        if (commandQueueWarnings.should_log()) {
            CONJURE_LOG_WARN(
              LOG_PHYSICS,
              "command queue full ({} commands); raise physics_settings::maxQueuedCommands",
              settings.maxQueuedCommands
            );
        }
        return core::make_error(physics_error::COMMAND_QUEUE_FULL, "{} commands queued", commands.size());
    }
    commands.push_back(command);
    return core::result<void>::success();
}


/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

physics_world::physics_world(const physics_settings &settings, core::jobs::job_system &jobs) :
    mState(std::make_unique<state>(settings, jobs)) {}

physics_world::~physics_world() {
    if (mState->world != nullptr) {
        detach();
    }
}


/*===========================================================================*/
/*===========================================================================*/
// ECS binding

core::result<void> physics_world::attach(ecs::world &target) {
    if (!mState->is_owner_thread()) {
        CONJURE_ASSERT(false, "physics_world::attach is owner-thread only");
        return core::make_error(physics_error::WRONG_THREAD, "attach is owner-thread only");
    }
    if (mState->world != nullptr) {
        return core::make_error(physics_error::ALREADY_ATTACHED, "physics_world is already attached");
    }
    const ecs::component_registry &registry = target.registry();
    physics_component_ids ids = {};
    ids.rigidBody = registry.id_of<rigid_body>();
    ids.box = registry.id_of<box_collider>();
    ids.sphere = registry.id_of<sphere_collider>();
    ids.capsule = registry.id_of<capsule_collider>();
    ids.convex = registry.id_of<convex_collider>();
    ids.mesh = registry.id_of<mesh_collider>();
    ids.material = registry.id_of<physics_material>();
    ids.character = registry.id_of<character_controller>();
    ids.parent = registry.id_of<scene::parent>();
    ids.localTransform = registry.id_of<scene::local_transform>();
    ids.interpolatedTransform = registry.id_of<scene::interpolated_transform>();
    if (!ids.is_complete()) {
        return core::error(
          core::error_code::INVALID_STATE,
          "register the physics and scene components before physics_world::attach"
        );
    }

    mState->world = &target;
    mState->ids = ids;
    mState->ownDeferred = std::make_unique<ecs::command_buffer>(target);
    mState->sinceVersion = target.version();
    ecs::query_desc pullDesc = {};
    pullDesc.with.set(ids.rigidBody);
    pullDesc.with.set(ids.interpolatedTransform);
    pullDesc.with.set(ids.localTransform);
    mState->pullQuery.emplace(target, pullDesc);
    ecs::query_desc bodyDesc = {};
    bodyDesc.with.set(ids.rigidBody);
    bodyDesc.with.set(ids.localTransform);
    bodyDesc.changed.set(ids.rigidBody);
    bodyDesc.changed.set(ids.localTransform);
    mState->bodyChangeQuery.emplace(target, bodyDesc);
    ecs::query_desc characterDesc = {};
    characterDesc.with.set(ids.character);
    characterDesc.with.set(ids.localTransform);
    characterDesc.changed.set(ids.character);
    characterDesc.changed.set(ids.localTransform);
    mState->characterChangeQuery.emplace(target, characterDesc);
    mState->install_observers();

    // Entities that already carry body components (a scene loaded before the attach) are created at
    // the next push like everything else.
    for (ecs::archetype *archetype : target.archetypes()) {
        const ecs::component_mask &mask = archetype->mask();
        const bool defining = mask.test(ids.rigidBody) || mask.test(ids.character) || mask.test(ids.box)
          || mask.test(ids.sphere) || mask.test(ids.capsule) || mask.test(ids.convex) || mask.test(ids.mesh);
        if (!defining) {
            continue;
        }
        for (u32 chunk = 0; chunk < archetype->chunk_count(); ++chunk) {
            for (const ecs::entity entity : archetype->chunk_entities(chunk)) {
                mState->mark_pending_create(entity);
            }
        }
    }
    return core::result<void>::success();
}

void physics_world::detach() {
    if (mState->world == nullptr) {
        return;
    }
    CONJURE_ASSERT(mState->is_owner_thread(), "physics_world::detach is owner-thread only");
    mState->remove_observers();
    // Leftovers of a push that ran while the world was locked are discarded first: an add queued there
    // would otherwise bring back the component the removals below take away (remove_interpolated skips an
    // entity that does not hold it yet).
    mState->ownDeferred->clear();
    mState->queue_interpolated_removals(*mState->ownDeferred);
    mState->destroy_all_bodies();
    if (!mState->ownDeferred->empty()) {
        if (mState->world->is_locked()) {
            CONJURE_ASSERT(false, "physics_world::detach while the ECS world is locked; interpolated_transform kept");
        } else {
            // The observers are gone, so the flush does not call back into physics.
            const ecs::flush_report report = mState->world->flush(*mState->ownDeferred);
            CONJURE_UNUSED(report);
        }
    }
    mState->ownDeferred->clear();
    mState->ownDeferred.reset();
    mState->world = nullptr;
    mState->events.clear();
    mState->eventKeys.clear();
    mState->pendingExits.clear();
    mState->pendingExitKeys.clear();
    mState->pullQuery.reset();
    mState->bodyChangeQuery.reset();
    mState->characterChangeQuery.reset();
    mState->commands.clear();
}

bool physics_world::is_attached() const noexcept {
    return mState->world != nullptr;
}


/*===========================================================================*/
/*===========================================================================*/
// Queued body API

bool physics_world::has_body(ecs::entity target) const noexcept {
    return mState->find_row(target) != nullptr;
}

core::result<void> physics_world::add_force(ecs::entity target, const math::vec3 &force) {
    body_command command = {};
    command.type = body_command::kind::ADD_FORCE;
    command.target = target;
    command.vectorA = force;
    return mState->queue(command);
}

core::result<void> physics_world::add_force_at(ecs::entity target, const math::vec3 &force, const math::vec3 &point) {
    body_command command = {};
    command.type = body_command::kind::ADD_FORCE_AT;
    command.target = target;
    command.vectorA = force;
    command.vectorB = point;
    return mState->queue(command);
}

core::result<void> physics_world::add_impulse(ecs::entity target, const math::vec3 &impulse) {
    body_command command = {};
    command.type = body_command::kind::ADD_IMPULSE;
    command.target = target;
    command.vectorA = impulse;
    return mState->queue(command);
}

core::result<void> physics_world::add_impulse_at(
  ecs::entity target,
  const math::vec3 &impulse,
  const math::vec3 &point
) {
    body_command command = {};
    command.type = body_command::kind::ADD_IMPULSE_AT;
    command.target = target;
    command.vectorA = impulse;
    command.vectorB = point;
    return mState->queue(command);
}

core::result<void> physics_world::add_torque(ecs::entity target, const math::vec3 &torque) {
    body_command command = {};
    command.type = body_command::kind::ADD_TORQUE;
    command.target = target;
    command.vectorA = torque;
    return mState->queue(command);
}

core::result<void> physics_world::add_angular_impulse(ecs::entity target, const math::vec3 &impulse) {
    body_command command = {};
    command.type = body_command::kind::ADD_ANGULAR_IMPULSE;
    command.target = target;
    command.vectorA = impulse;
    return mState->queue(command);
}

core::result<void> physics_world::set_linear_velocity(ecs::entity target, const math::vec3 &velocity) {
    body_command command = {};
    command.type = body_command::kind::SET_LINEAR_VELOCITY;
    command.target = target;
    command.vectorA = velocity;
    return mState->queue(command);
}

core::result<void> physics_world::set_angular_velocity(ecs::entity target, const math::vec3 &velocity) {
    body_command command = {};
    command.type = body_command::kind::SET_ANGULAR_VELOCITY;
    command.target = target;
    command.vectorA = velocity;
    return mState->queue(command);
}

core::result<void> physics_world::teleport(ecs::entity target, const math::transform &pose) {
    body_command command = {};
    command.type = body_command::kind::TELEPORT;
    command.target = target;
    command.vectorA = pose.position;
    command.rotation = pose.rotation;
    return mState->queue(command);
}

core::result<void> physics_world::move_kinematic(ecs::entity target, const math::transform &pose) {
    body_command command = {};
    command.type = body_command::kind::MOVE_KINEMATIC;
    command.target = target;
    command.vectorA = pose.position;
    command.rotation = pose.rotation;
    return mState->queue(command);
}

core::result<void> physics_world::set_gravity(const math::vec3 &value) {
    body_command command = {};
    command.type = body_command::kind::SET_GRAVITY;
    command.vectorA = value;
    return mState->queue(command);
}

math::vec3 physics_world::gravity() const noexcept {
    return mState->gravity;
}

core::result<void> physics_world::wake(ecs::entity target) {
    body_command command = {};
    command.type = body_command::kind::ACTIVATE;
    command.target = target;
    return mState->queue(command);
}

core::result<void> physics_world::sleep(ecs::entity target) {
    body_command command = {};
    command.type = body_command::kind::DEACTIVATE;
    command.target = target;
    return mState->queue(command);
}

bool physics_world::is_sleeping(ecs::entity target) const noexcept {
    const body_record *row = mState->find_row(target);
    if (row == nullptr) {
        return false;
    }
    // A static body is never awake, also the static trigger that Jolt keeps as an active sensor (F408).
    return row->motion == motion_type::STATIC || !mState->is_awake(*row);
}

core::result<void> physics_world::character_move(ecs::entity target, const math::vec3 &velocity) {
    body_command command = {};
    command.type = body_command::kind::CHARACTER_MOVE;
    command.target = target;
    command.vectorA = velocity;
    return mState->queue(command);
}

std::optional<ground_state> physics_world::character_ground(ecs::entity target) const noexcept {
    const body_record *row = mState->find_row(target);
    if (row == nullptr || row->kind != record_kind::CHARACTER || row->character == nullptr) {
        return std::nullopt;
    }
    switch (row->character->GetGroundState()) {
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


/*===========================================================================*/
/*===========================================================================*/
// Assets, shapes, introspection

void physics_world::on_asset_reloaded(assets::asset_id id) {
    if (id.is_null()) {
        return;
    }
    for (const body_record &row : mState->records) {
        if (row.is_free()) {
            continue;
        }
        if (row.convexAsset == id || row.meshAsset == id) {
            mState->mark_rebuild(row.entity, record_dirty_flags::SHAPE);
        }
    }
}

void physics_world::collect_shapes() {
    mState->shapes.collect();
}

const physics_settings &physics_world::settings() const noexcept {
    return mState->settings;
}

const physics_layer_table &physics_world::layers() const noexcept {
    return mState->settings.layers;
}

physics_stats physics_world::stats() const noexcept {
    physics_stats snapshot = mState->stats;
    snapshot.bodies = body_count();
    snapshot.activeBodies = active_body_count();
    snapshot.characters = mState->characterCount;
    snapshot.shapeCacheEntries = mState->shapes.size();
    return snapshot;
}

u32 physics_world::body_count() const noexcept {
    return mState->liveRows;
}

u32 physics_world::active_body_count() const noexcept {
    // Jolt counts the static triggers too: each is an active kinematic sensor there (code sweep F408).
    const u32 active = mState->physicsSystem->GetNumActiveBodies(JPH::EBodyType::RigidBody);
    return active > mState->staticSensors ? active - mState->staticSensors : 0;
}

std::optional<math::transform> physics_world::body_pose(ecs::entity target) const noexcept {
    // The busy check comes first: the row tables may be changing while a push, step or pull runs.
    if (mState->busy.load(std::memory_order_acquire)) {
        return std::nullopt;
    }
    const body_record *row = mState->find_row(target);
    if (row == nullptr) {
        return std::nullopt;
    }
    if (row->kind == record_kind::CHARACTER && row->character != nullptr) {
        return jolt::to_transform(row->character->GetPosition(), row->character->GetRotation());
    }
    JPH::RVec3 position = {};
    JPH::Quat rotation = {};
    mState->physicsSystem->GetBodyInterface().GetPositionAndRotation(row->bodyId, position, rotation);
    return jolt::to_transform(position, rotation);
}

math::aabb physics_world::bounds() const noexcept {
    if (body_count() == 0) {
        return math::aabb();
    }
    return jolt::from_jolt(mState->physicsSystem->GetBounds());
}

core::result<void> physics_world::validate() const {
    const state &s = *mState;
    u32 rows = 0;
    u32 staticTriggers = 0;
    for (usize index = 0; index < s.records.size(); ++index) {
        const body_record &row = s.records[index];
        if (row.is_free()) {
            continue;
        }
        ++rows;
        if (row.bodyId.GetIndex() != index) {
            return core::error(
              core::error_code::INVALID_STATE,
              std::format("row {} holds body index {}", index, row.bodyId.GetIndex())
            );
        }
        if (s.world != nullptr && !s.world->is_alive(row.entity)) {
            return core::error(
              core::error_code::INVALID_STATE,
              std::format("row {} refers to dead entity {}", index, row.entity)
            );
        }
        const auto mapped = s.rowOfEntity.find(row.entity);
        if (mapped == s.rowOfEntity.end() || mapped->second != index) {
            return core::error(
              core::error_code::INVALID_STATE,
              std::format("entity {} does not map back to row {}", row.entity, index)
            );
        }
        if (s.find_row(row.entity) != &row) {
            return core::error(
              core::error_code::INVALID_STATE,
              std::format("entity {} is not indexed to row {} by entity index", row.entity, index)
            );
        }
        const JPH::BodyLockRead lock(s.physicsSystem->GetBodyLockInterface(), row.bodyId);
        if (!lock.Succeeded()) {
            return core::error(core::error_code::INVALID_STATE, std::format("row {} has no Jolt body", index));
        }
        if (lock.GetBody().GetUserData() != row.entity.to_u64()) {
            return core::error(core::error_code::INVALID_STATE, std::format("body {} carries another entity", index));
        }
        if (row.layer >= s.settings.layers.count()) {
            return core::make_error(physics_error::UNKNOWN_LAYER, "row {} is on layer {}", index, row.layer);
        }
        // A static trigger is an active kinematic sensor in Jolt (code sweep F408); no other row is marked.
        const bool staticTrigger = row.kind == record_kind::BODY && row.motion == motion_type::STATIC && row.isTrigger;
        if (row.staticSensor != staticTrigger) {
            return core::error(
              core::error_code::INVALID_STATE,
              std::format("row {} is marked as a static trigger wrongly", index)
            );
        }
        if (staticTrigger) {
            ++staticTriggers;
            const JPH::Body &joltBody = lock.GetBody();
            if (!joltBody.IsKinematic() || !joltBody.IsSensor() || !joltBody.IsActive()) {
                return core::error(
                  core::error_code::INVALID_STATE,
                  std::format("the static trigger of row {} is not an active kinematic sensor", index)
                );
            }
        }
        if (s.world != nullptr && row.kind == record_kind::BODY) {
            const rigid_body *body = s.read<rigid_body>(row.entity);
            if (body != nullptr && body->layer >= s.settings.layers.count()) {
                return core::make_error(
                  physics_error::UNKNOWN_LAYER,
                  "entity {} names layer {}",
                  row.entity,
                  body->layer
                );
            }
        }
    }
    for (const auto &entry : s.rowOfEntity) {
        if (state::is_marker(entry.second)) {
            continue;
        }
        if (entry.second >= s.records.size() || s.records[entry.second].is_free()) {
            return core::error(
              core::error_code::INVALID_STATE,
              std::format("entity {} maps to a free row", entry.first)
            );
        }
    }
    u32 indexedRows = 0;
    for (usize entityIndex = 0; entityIndex < s.liveRowByEntityIndex.size(); ++entityIndex) {
        const u32 rowIndex = s.liveRowByEntityIndex[entityIndex];
        if (rowIndex == state::NO_ROW) {
            continue;
        }
        ++indexedRows;
        if (rowIndex >= s.records.size() || s.records[rowIndex].is_free()
          || s.records[rowIndex].entity.index != entityIndex) {
            return core::error(
              core::error_code::INVALID_STATE,
              std::format("entity index {} is indexed to row {}, which does not hold it", entityIndex, rowIndex)
            );
        }
    }
    if (indexedRows != rows) {
        return core::error(
          core::error_code::INVALID_STATE,
          std::format("{} rows but {} entity indices point at a row", rows, indexedRows)
        );
    }
    if (rows != s.physicsSystem->GetNumBodies()) {
        return core::error(
          core::error_code::INVALID_STATE,
          std::format("{} rows but Jolt holds {} bodies", rows, s.physicsSystem->GetNumBodies())
        );
    }
    if (rows != s.liveRows) {
        return core::error(
          core::error_code::INVALID_STATE,
          std::format("{} rows but the live row counter says {}", rows, s.liveRows)
        );
    }
    if (staticTriggers != s.staticSensors) {
        return core::error(
          core::error_code::INVALID_STATE,
          std::format("{} static triggers but their counter says {}", staticTriggers, s.staticSensors)
        );
    }
    for (const auto &pair : s.activePairs) {
        const u32 bodyA = static_cast<u32>(pair.first >> 32);
        const u32 bodyB = static_cast<u32>(pair.first & 0xFFFFFFFFu);
        if (s.row_of_body(bodyA) == nullptr || s.row_of_body(bodyB) == nullptr) {
            return core::error(core::error_code::INVALID_STATE, "a contact pair references a free row");
        }
    }
    if (!s.shapes.validate()) {
        return core::error(core::error_code::INVALID_STATE, "a cached shape is unreferenced");
    }
    return core::result<void>::success();
}
}

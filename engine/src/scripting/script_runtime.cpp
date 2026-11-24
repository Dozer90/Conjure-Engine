#include "conjure/scripting/script_runtime.h"

#include "scripting/scripting_internal.h"

#include "conjure/scripting/log_category.h"
#include "conjure/scripting/script_fields.h"
#include "conjure/scripting/scripting_error.h"

#include "conjure/core/assert.h"
#include "conjure/core/jobs/main_thread.h"
#include "conjure/core/json/writer.h"
#include "conjure/core/log.h"
#include "conjure/ecs/chunk_view.h"
#include "conjure/ecs/ecs_error.h"
#include "conjure/ecs/query.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <optional>
#include <utility>

using namespace conjure;
using namespace conjure::scripting;

namespace {

    script_runtime *sActiveRuntime = nullptr;

    constexpr usize SCRATCH_ARENA_BYTES = 512 * KIB;
    constexpr usize SCRATCH_BLOCK_BYTES = 64 * KIB;
    constexpr usize INITIAL_BLOB_BUFFER_BYTES = 1024;

    [[nodiscard]] constexpr script_callback callback_of(collision_kind kind) noexcept {
        switch (kind) {
            case collision_kind::COLLISION_ENTER:
                return script_callback::COLLISION_ENTER;
            case collision_kind::COLLISION_EXIT:
                return script_callback::COLLISION_EXIT;
            case collision_kind::TRIGGER_ENTER:
                return script_callback::TRIGGER_ENTER;
            case collision_kind::TRIGGER_EXIT:
                return script_callback::TRIGGER_EXIT;
            case collision_kind::COUNT:
                break;
        }
        return script_callback::COUNT;
    }

    [[nodiscard]] conjure_vec3 to_abi(const math::vec3 &value) noexcept {
        conjure_vec3 result = {};
        result.x = value.x;
        result.y = value.y;
        result.z = value.z;
        return result;
    }
}


/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

script_runtime::script_runtime(std::unique_ptr<script_host> host) :
    mHost(std::move(host)),
    mFrameBlock(std::make_unique<conjure_frame_block>()),
    mScratch(SCRATCH_ARENA_BYTES, SCRATCH_BLOCK_BYTES) {
    CONJURE_ASSERT(mHost != nullptr, "script_runtime needs a host");
    CONJURE_ASSERT(sActiveRuntime == nullptr, "one script_runtime per process (03_scripting.md 5)");
    sActiveRuntime = this;
    *mFrameBlock = {};
    mNativeApi = make_native_api(mFrameBlock.get());
    mFrameBlock->time_scale = 1.0f;
    mFrameBlock->current_phase = static_cast<uint8_t>(script_phase::COUNT);
    mBlobBuffer.resize(INITIAL_BLOB_BUFFER_BYTES);
    // Reserved once: a scope open or close must not grow either list in steady state (08_testing.md 7.1).
    mQueryScopes.reserve(QUERY_SCOPE_RESERVE);
    mFreeQueryScopes.reserve(QUERY_SCOPE_RESERVE);
}

script_runtime::~script_runtime() {
    if (mStore.mapped_slot_count() > 0) {
        CONJURE_LOG_WARN(
          LOG_SCRIPTING,
          "script_runtime destroyed with {} live instances; destroying them without OnDestroy",
          mStore.mapped_slot_count()
        );
        destroy_all(false);
    }
    if (mStatus == script_host_status::READY) {
        mHost->shutdown();
    }
    attach_world(nullptr);
    if (sActiveRuntime == this) {
        sActiveRuntime = nullptr;
    }
}


/*===========================================================================*/
/*===========================================================================*/
// Lifetime

core::result<void> script_runtime::initialise(const script_runtime_config &config) {
    if (mStatus == script_host_status::READY) {
        return core::error(core::error_code::INVALID_STATE, "script_runtime is already initialised");
    }
    if (!core::jobs::is_main_thread()) {
        return core::make_error(scripting_error::WRONG_THREAD, "script_runtime::initialise off the main thread");
    }

    fill_core_native_entries(mNativeApi);
    mNativeApi.size = static_cast<uint32_t>(sizeof(conjure_native_api));
    mNativeApi.version = CONJURE_NATIVE_API_VERSION;
    mNativeApi.layout_hash = CONJURE_NATIVE_API_HASH;
    mNativeApi.frame = mFrameBlock.get();
    mMode = config.initialMode;
    mFrameBlock->in_play_mode = mMode == script_mode::PLAY ? 1 : 0;

    const core::result<void> validated = validate_native_api(mNativeApi);
    if (!validated.has_value()) {
        mStatus = script_host_status::DISABLED;
        return validated;
    }

    const core::result<void> hosted = mHost->initialise(config.host, mNativeApi);
    if (!hosted.has_value()) {
        mStatus = script_host_status::DISABLED;
        CONJURE_LOG_WARN(LOG_SCRIPTING, "scripting disabled: {}", hosted.get_error());
        return hosted;
    }
    const core::result<void> managedValid = validate_managed_api(mHost->managed_api());
    if (!managedValid.has_value()) {
        mHost->shutdown();
        mStatus = script_host_status::DISABLED;
        CONJURE_LOG_WARN(LOG_SCRIPTING, "scripting disabled: {}", managedValid.get_error());
        return managedValid;
    }
    mStatus = script_host_status::READY;

    if (!config.gameAssemblyPath.empty()) {
        CONJURE_TRY(load_game_assembly(config.gameAssemblyPath));
    }
    return core::result<void>::success();
}

void script_runtime::shutdown() {
    if (mStatus == script_host_status::READY) {
        destroy_all(true);
        mHost->shutdown();
    }
    mStatus = script_host_status::UNINITIALISED;
    mAssemblyLoaded = false;
    mAssemblyPath.clear();
    mClasses.clear();
    mPendingCreates.clear();
    mPendingDestroySlots.clear();
    mPendingEntityDestroys.clear();
    mQueryScopes.clear();
    mWarnedMissingClasses.clear();
    mWarnedMirrorNames.clear();
}

void script_runtime::attach_world(ecs::world *world) {
    if (mWorld != nullptr && world != nullptr && world != mWorld) {
        // Instances are bound to the ids of the world they were created on (03_scripting.md 11): none may
        // survive a swap. The owner destroys them with OnDestroy (destroy_all / set_mode(EDIT)) first.
        if (mStore.mapped_slot_count() > 0) {
            CONJURE_LOG_WARN(
              LOG_SCRIPTING,
              "attach_world swapped worlds with {} live instances; destroying them without OnDestroy",
              mStore.mapped_slot_count()
            );
        }
        destroy_all(false);
    }
    if (mWorld != nullptr) {
        mWorld->unobserve(mAddedObserver);
        mWorld->unobserve(mRemovedObserver);
        mWorld->unobserve(mDestroyedObserver);
        mAddedObserver = {};
        mRemovedObserver = {};
        mDestroyedObserver = {};
        mQueryScopes.clear();
        mQueryCache.clear();        // the cached queries point into the old world's archetypes
        drop_deferred_reservations();
        mDeferredCommands.reset();
        mEntityNames.clear();
        mWarnedMirrorNames.clear();
    }
    mWorld = world;
    mScriptComponentId = {};
    if (mWorld == nullptr) {
        return;
    }

    mDeferredCommands = std::make_unique<ecs::command_buffer>(*mWorld);
    // Every destroy path (native, Entity.Destroy, command buffers, scene unload) fires this observer, so a name
    // lives exactly as long as its entity; worlds without script_component are covered too. A reserved id that
    // dies before it was placed fires none: settle_deferred_reservations forgets its name.
    mDestroyedObserver = mWorld->observe_destroyed([this](ecs::world &, ecs::entity target, ecs::command_buffer &) {
        mEntityNames.erase(target);
    });
    mScriptComponentId = mWorld->registry().id_of<script_component>();
    if (!mScriptComponentId.is_valid()) {
        CONJURE_LOG_WARN(
          LOG_SCRIPTING,
          "world '{}' was created on a registry without script_component; call register_script_component first",
          mWorld->name()
        );
        return;
    }
    mAddedObserver = mWorld->observe(
      mScriptComponentId,
      ecs::component_event::ADDED,
      [this](ecs::world &owner, ecs::entity target, ecs::component_id, ecs::command_buffer &) {
          script_component *component = owner.get<script_component>(target);
          if (component != nullptr) {
              on_component_added(target, *component);
          }
      }
    );
    mRemovedObserver = mWorld->observe(
      mScriptComponentId,
      ecs::component_event::REMOVED,
      [this](ecs::world &owner, ecs::entity target, ecs::component_id, ecs::command_buffer &) {
          const script_component *component = std::as_const(owner).get<script_component>(target);
          if (component != nullptr) {
              on_component_removed(target, *component);
          }
      }
    );

    // Components that arrived without this runtime observing (a world copied while detached, R29) are
    // adopted now, exactly as the ADDED observer would have done.
    adopt_world_sets(mMode == script_mode::PLAY);
}

void script_runtime::attach_registry(ecs::component_registry *registry) {
    mRegistry = registry;
}

void script_runtime::set_collision_source(std::function<std::span<const collision_event>()> source) {
    mCollisionSource = std::move(source);
}


/*===========================================================================*/
/*===========================================================================*/
// Assemblies

core::result<void> script_runtime::load_game_assembly(const std::filesystem::path &assemblyPath) {
    return reload_internal(assemblyPath);
}

core::result<void> script_runtime::reload_game_assembly(const std::filesystem::path &assemblyPath) {
    return reload_internal(assemblyPath);
}


/*===========================================================================*/
/*===========================================================================*/
// Frame flow

void script_runtime::set_mode(script_mode mode) {
    if (mode == mMode) {
        return;
    }
    mMode = mode;
    mFrameBlock->in_play_mode = mode == script_mode::PLAY ? 1 : 0;
    if (mode == script_mode::EDIT) {
        destroy_all(true);
        return;
    }
    adopt_world_sets(true);
    flush_pending();
}

void script_runtime::run_phase(script_phase phase, f32 deltaTime) {
    if (!is_ready() || phase >= script_phase::COUNT) {
        return;
    }
    enter_phase(phase);
    if (phase == script_phase::FIXED_UPDATE || phase == script_phase::UPDATE) {
        // OnStart is attempted exactly once, whatever happens inside it (03_scripting.md 3.2).
        mark_started_entries();
    }

    const i32 result = mHost->managed_api().run_phase(static_cast<uint8_t>(phase), deltaTime);
    if (result < 0) {
        CONJURE_LOG_WARN(LOG_SCRIPTING, "run_phase({}) returned {}", to_string(phase), result);
    }

    // The flush still counts as the phase: OnCreate/OnDestroy of the instances it creates and destroys
    // may open query scopes like any other callback of this phase.
    force_close_query_scopes();
    flush_deferred_commands();
    flush_pending();
    leave_phase();
}

void script_runtime::flush_pending() {
    if (mInFlush) {
        return;
    }
    mInFlush = true;
    u32 iterations = 0;
    while (!mPendingDestroySlots.empty() || !mPendingEntityDestroys.empty() || !mPendingCreates.empty()) {
        if (iterations == MAX_FLUSH_ITERATIONS) {
            CONJURE_LOG_WARN(
              LOG_SCRIPTING,
              "flush_pending stopped after {} iterations with {} creates and {} destroys still queued",
              MAX_FLUSH_ITERATIONS,
              mPendingCreates.size(),
              mPendingDestroySlots.size() + mPendingEntityDestroys.size()
            );
            break;
        }
        process_pending_destroys();
        process_pending_creates();
        ++iterations;
    }
    mInFlush = false;
}

void script_runtime::dispatch_collision_events(std::span<const collision_event> events) {
    if (!is_ready() || mWorld == nullptr || events.empty()) {
        return;
    }

    // The member keeps its capacity, so a warm dispatch allocates nothing. Not re-entrant: only
    // scripting.collision_dispatch drives it, and no native entry leads back here from a callback.
    std::vector<conjure_collision_record> &records = mCollisionRecords;
    records.clear();
    records.reserve(events.size() * 2);
    const auto appendFor = [&](ecs::entity participant, ecs::entity other, const collision_event &event, f32 sign) {
        const script_component *component = std::as_const(*mWorld).get<script_component>(participant);
        if (component == nullptr) {
            return;
        }
        const script_set *set = mStore.find(component->setHandle);
        if (set == nullptr) {
            return;
        }
        const script_callback callback = callback_of(event.kind);
        for (const script_entry &entryValue : set->entries) {
            if (!entryValue.is_live()) {
                continue;
            }
            const script_class_info *info = mClasses.find(entryValue.classHash);
            if (info == nullptr || !info->has_callback(callback)) {
                continue;
            }
            conjure_collision_record record = {};
            record.slot = entryValue.managedSlot;
            record.other = other.to_u64();
            record.point = to_abi(event.point);
            record.normal = to_abi(event.normal * sign);
            record.impulse = event.impulse;
            record.kind = static_cast<uint8_t>(event.kind);
            records.push_back(record);
        }
    };
    for (const collision_event &event : events) {
        appendFor(event.a, event.b, event, 1.0f);
        appendFor(event.b, event.a, event, -1.0f);
    }
    if (records.empty()) {
        return;
    }

    // The collision callbacks run inside the FIXED_UPDATE phase (03_scripting.md 3.2): a query scope
    // opened by OnCollisionEnter is legal and closed at the end of the dispatch like at a phase end.
    const bool enteredPhase = mCurrentPhase == script_phase::COUNT;
    if (enteredPhase) {
        enter_phase(script_phase::FIXED_UPDATE);
    }
    const i32 result = mHost->managed_api().dispatch_collisions(records.data(), static_cast<i32>(records.size()));
    if (result < 0) {
        CONJURE_LOG_WARN(LOG_SCRIPTING, "dispatch_collisions returned {}", result);
    }
    force_close_query_scopes();
    flush_deferred_commands();
    if (enteredPhase) {
        leave_phase();
    }
}

void script_runtime::destroy_all(bool callOnDestroy) {
    std::vector<i32> slots = {};
    mStore.for_each_set([&slots](u32, script_set &set) {
        for (const script_entry &entryValue : set.entries) {
            if (entryValue.is_live()) {
                slots.push_back(entryValue.managedSlot);
            }
        }
    });
    slots.insert(slots.end(), mPendingDestroySlots.begin(), mPendingDestroySlots.end());
    mPendingDestroySlots.clear();
    mPendingCreates.clear();
    destroy_slots(slots, callOnDestroy);

    std::vector<ecs::entity> entities = std::exchange(mPendingEntityDestroys, {});
    if (mWorld != nullptr) {
        for (const ecs::entity target : entities) {
            if (mWorld->is_alive(target)) {
                static_cast<void>(mWorld->destroy_entity(target));
            }
        }
    }

    // The session is over: the next PLAY creates every instance afresh and runs OnCreate and OnStart again
    // (the created and started flags are for hot reload only), and a fault badge does not outlive its instance.
    mStore.for_each_set([](u32, script_set &set) {
        for (script_entry &entryValue : set.entries) {
            entryValue.flags &=
              ~(script_entry_flags::CREATED | script_entry_flags::STARTED | script_entry_flags::FAULTED);
        }
    });
}


/*===========================================================================*/
/*===========================================================================*/
// Scripts of an entity

script_entry_id script_runtime::add_script(ecs::entity entity, u64 classHash, const core::json::value &fields) {
    if (mWorld == nullptr) {
        return {};
    }
    deferred_reservation *reservation = find_reservation(entity);
    if (reservation != nullptr) {
        return stage_reserved_script(entity, *reservation, classHash, fields);
    }
    if (!mWorld->is_alive(entity)) {
        return {};
    }
    script_component *component = mWorld->get<script_component>(entity);
    if (component == nullptr) {
        if (mWorld->is_locked()) {
            // A first script needs the script_component, a structural change the iteration lock (a managed
            // ComponentQuery scope) refuses; asking the world anyway would trip its check_structural assert.
            CONJURE_LOG_WARN(
              LOG_SCRIPTING,
              "add_script: no script_component on {}: the world is locked for iteration; add the first script "
              "after the scope closes",
              entity
            );
            return {};
        }
        core::result<script_component *> added = mWorld->add<script_component>(entity);
        if (!added.has_value()) {
            CONJURE_LOG_WARN(LOG_SCRIPTING, "add_script: no script_component on {}: {}", entity, added.get_error());
            return {};
        }
        component = added.value();
    }
    if (component->setHandle == INVALID_SET_HANDLE) {
        component->setHandle = mStore.create_set(entity);
    }

    script_entry entryValue = {};
    entryValue.classHash = classHash;
    entryValue.fields = fields.is_object() ? core::json::document::from_value(fields) : core::json::document::object();
    const script_entry_id id = mStore.add_entry(component->setHandle, std::move(entryValue));
    if (id.is_valid() && mMode == script_mode::PLAY) {
        queue_create(id);
    }
    return id;
}

void script_runtime::remove_script(ecs::entity entity, script_entry_id id) {
    const script_component *component =
      mWorld == nullptr ? nullptr : std::as_const(*mWorld).get<script_component>(entity);
    if (component == nullptr || component->setHandle != id.setHandle) {
        CONJURE_LOG_DEBUG(LOG_SCRIPTING, "remove_script: the entry does not belong to {}", entity);
        return;
    }
    const script_entry *entryValue = mStore.find_entry(id);
    if (entryValue == nullptr) {
        return;
    }
    if (entryValue->is_live()) {
        const std::array<i32, 1> slots = {entryValue->managedSlot};
        destroy_slots(slots, true);
    }
    std::erase_if(mPendingCreates, [id](const pending_create &pending) {
        return pending.id == id;
    });
    // Later entries of the set shift down: pending creates pointing past the removed index follow them.
    for (pending_create &pending : mPendingCreates) {
        if (pending.id.setHandle == id.setHandle && pending.id.entryIndex > id.entryIndex) {
            --pending.id.entryIndex;
        }
    }
    mStore.remove_entry(id);
}

core::result<void> script_runtime::set_field(script_entry_id id, u64 fieldHash, const core::json::value &value) {
    script_entry *entryValue = mStore.find_entry(id);
    if (entryValue == nullptr) {
        return core::make_error(scripting_error::INSTANCE_NOT_FOUND, "no script entry for the id");
    }
    const script_class_info *info = mClasses.find(entryValue->classHash);
    if (info == nullptr) {
        return core::make_error(
          scripting_error::CLASS_NOT_FOUND,
          "class {:#018x} is not loaded",
          entryValue->classHash
        );
    }
    const script_field_info *field = info->find_field(fieldHash);
    if (field == nullptr) {
        return core::make_error(
          scripting_error::FIELD_NOT_FOUND,
          "'{}' has no field {:#018x}",
          info->fullName,
          fieldHash
        );
    }
    if (!is_value_compatible(field->kind, field->elementKind, value)) {
        return core::error(
          core::error_code::INVALID_ARGUMENT,
          "field '" + field->name + "' of '" + info->fullName + "' expects " + std::string(to_string(field->kind))
        );
    }
    entryValue->fields.set(field->name, value);
    if (!entryValue->is_live()) {
        return core::result<void>::success();
    }

    core::json::document single = core::json::document::object();
    single.set(field->name, value);
    script_class_info oneField = {};
    oneField.hash = info->hash;
    oneField.fullName = info->fullName;
    oneField.fields.push_back(*field);
    return apply_entry_fields_with(*entryValue, oneField, single.root());
}

core::result<core::json::document> script_runtime::collect_entry_fields(script_entry_id id) {
    script_entry *entryValue = mStore.find_entry(id);
    if (entryValue == nullptr) {
        return core::make_error(scripting_error::INSTANCE_NOT_FOUND, "no script entry for the id");
    }
    refresh_live_fields(*entryValue);
    return core::json::document(entryValue->fields);
}

core::result<core::json::document> script_runtime::serialize_entity_scripts(ecs::entity entity) {
    if (mWorld == nullptr || !mWorld->is_alive(entity)) {
        return core::make_error(ecs::ecs_error::ENTITY_NOT_ALIVE, "serialize_entity_scripts on a dead entity");
    }
    core::json::writer out = {};
    const script_component *component = std::as_const(*mWorld).get<script_component>(entity);
    write_set_json(component == nullptr ? INVALID_SET_HANDLE : component->setHandle, out);
    return core::json::document::parse(out.finish());
}

core::result<void> script_runtime::deserialize_entity_scripts(ecs::entity entity, const core::json::value &scripts) {
    if (mWorld == nullptr || !mWorld->is_alive(entity)) {
        return core::make_error(ecs::ecs_error::ENTITY_NOT_ALIVE, "deserialize_entity_scripts on a dead entity");
    }
    if (!scripts.is_object() || !scripts["scripts"].is_array()) {
        return core::error(core::error_code::INVALID_ARGUMENT, "expected {\"scripts\":[...]}");
    }
    script_component *component = mWorld->get<script_component>(entity);
    if (component == nullptr) {
        CONJURE_TRY_ASSIGN(script_component *added, mWorld->add<script_component>(entity));
        component = added;
    }
    if (component->setHandle == INVALID_SET_HANDLE) {
        component->setHandle = mStore.create_set(entity);
    }
    return replace_set_entries(component->setHandle, scripts);
}

void script_runtime::remap_entity_fields(ecs::entity entity, const std::function<ecs::entity(ecs::entity)> &remap) {
    if (mWorld == nullptr) {
        return;
    }
    const script_component *component = std::as_const(*mWorld).get<script_component>(entity);
    if (component == nullptr) {
        return;
    }
    script_set *set = mStore.find(component->setHandle);
    if (set == nullptr) {
        return;
    }
    for (script_entry &entryValue : set->entries) {
        const script_class_info *info = mClasses.find(entryValue.classHash);
        if (info == nullptr) {
            continue;
        }
        entryValue.fields = remap_entity_fields_json(*info, entryValue.fields.root(), remap);
        if (entryValue.is_live()) {
            const core::result<void> applied = apply_entry_fields(entryValue, entryValue.fields.root());
            if (!applied.has_value()) {
                CONJURE_LOG_WARN(LOG_SCRIPTING, "remap_entity_fields: {}", applied.get_error());
            }
        }
    }
}

std::vector<script_entry_id> script_runtime::scripts_of(ecs::entity entity) const {
    std::vector<script_entry_id> ids = {};
    if (mWorld == nullptr) {
        return ids;
    }
    const script_component *component = std::as_const(*mWorld).get<script_component>(entity);
    if (component == nullptr) {
        return ids;
    }
    const script_set *set = mStore.find(component->setHandle);
    if (set == nullptr) {
        return ids;
    }
    for (usize index = 0; index < set->entries.size(); ++index) {
        script_entry_id id = {};
        id.setHandle = component->setHandle;
        id.entryIndex = static_cast<u8>(index);
        ids.push_back(id);
    }
    return ids;
}

const script_entry *script_runtime::entry(script_entry_id id) const noexcept {
    return mStore.find_entry(id);
}

script_entry_id script_runtime::entry_of_slot(i32 slot) const noexcept {
    return mStore.resolve_slot(slot).value_or(script_entry_id{});
}

void script_runtime::on_component_removed(ecs::entity entity, const script_component &component) {
    CONJURE_UNUSED(entity);
    script_set *set = mStore.find(component.setHandle);
    if (set == nullptr) {
        return;
    }
    for (script_entry &entryValue : set->entries) {
        if (entryValue.is_live()) {
            mPendingDestroySlots.push_back(entryValue.managedSlot);
            mStore.unmap_slot(entryValue.managedSlot);
        }
    }
    std::erase_if(mPendingCreates, [&component](const pending_create &pending) {
        return pending.id.setHandle == component.setHandle;
    });
}

void script_runtime::on_component_added(ecs::entity entity, script_component &component) {
    deferred_reservation *reservation = nullptr;
    if (component.setHandle == INVALID_SET_HANDLE) {
        reservation = find_reservation(entity);
    }
    if (reservation != nullptr && reservation->scriptSet != INVALID_SET_HANDLE) {
        // The flush placed a reserved id with the script_component add_script recorded: the staged set is its set.
        component.setHandle = std::exchange(reservation->scriptSet, INVALID_SET_HANDLE);
    } else if (component.setHandle == INVALID_SET_HANDLE) {
        component.setHandle = mStore.create_set(entity);
    } else {
        script_set *set = mStore.find(component.setHandle);
        if (set == nullptr) {
            // A stale handle (data copied from another process or a reset store) becomes a fresh set.
            component.setHandle = mStore.create_set(entity);
        } else {
            set->owner = entity;
        }
    }
    if (mMode == script_mode::PLAY) {
        queue_creates_of_set(component.setHandle);
    }
}

void script_runtime::request_entity_destroy(ecs::entity entity) {
    if (mWorld == nullptr) {
        return;
    }
    if (find_reservation(entity) != nullptr) {
        // No instance exists before the flush, so nothing has to see the entity alive: the buffer's destroy wins
        // over every command on the id and hands the id back. On an instantiation root it cancels the copy
        // (world::flush): the archive's inner entities are never placed, so none outlives its root.
        deferred_commands().destroy_entity(entity);
        return;
    }
    if (!mWorld->is_alive(entity)) {
        return;
    }
    const script_component *component = std::as_const(*mWorld).get<script_component>(entity);
    const script_set *set = component == nullptr ? nullptr : mStore.find(component->setHandle);
    bool hasLive = false;
    if (set != nullptr) {
        for (const script_entry &entryValue : set->entries) {
            hasLive = hasLive || entryValue.is_live();
        }
    }
    if (hasLive) {
        // Always through the pending list, locked world or not: OnDestroy must see the entity alive
        // (03_scripting.md 3.4), which the REMOVED observer path of a command buffer cannot provide.
        const auto queued = std::find(mPendingEntityDestroys.begin(), mPendingEntityDestroys.end(), entity);
        if (queued == mPendingEntityDestroys.end()) {
            mPendingEntityDestroys.push_back(entity);
        }
        return;
    }
    if (mWorld->is_locked()) {
        deferred_commands().destroy_entity(entity);
        return;
    }
    static_cast<void>(mWorld->destroy_entity(entity));
}

void script_runtime::mark_faulted(i32 slot, script_callback callback) {
    const std::optional<script_entry_id> id = mStore.resolve_slot(slot);
    script_entry *entryValue = id.has_value() ? mStore.find_entry(*id) : nullptr;
    if (entryValue == nullptr) {
        // OnCreate and OnReload run inside create_instances, before the slot is mapped: remembered until it is.
        if (mCreatingInstances && slot >= 0) {
            faulted_slot faulted = {};
            faulted.slot = slot;
            faulted.callback = callback;
            mFaultedBeforeMapped.push_back(faulted);
        }
        return;
    }
    entryValue->flags |= script_entry_flags::FAULTED;
    log_faulted(slot, callback, *entryValue);
}

void script_runtime::set_entity_name(ecs::entity entity, std::string_view name) {
    if (name.empty()) {
        mEntityNames.erase(entity);
        return;
    }
    mEntityNames[entity] = std::string(name);
}

std::string_view script_runtime::entity_name(ecs::entity entity) const noexcept {
    const auto found = mEntityNames.find(entity);
    if (found == mEntityNames.end() || mWorld == nullptr || !mWorld->is_alive(entity)) {
        return {};
    }
    return found->second;
}

ecs::entity script_runtime::find_entity_by_name(std::string_view name) const noexcept {
    for (const auto &[entity, known] : mEntityNames) {
        if (known == name && mWorld != nullptr && mWorld->is_alive(entity)) {
            return entity;
        }
    }
    return ecs::NULL_ENTITY;
}

bool script_runtime::note_mirror_mismatch(u64 nameHash) {
    if (std::find(mWarnedMirrorNames.begin(), mWarnedMirrorNames.end(), nameHash) != mWarnedMirrorNames.end()) {
        return false;
    }
    mWarnedMirrorNames.push_back(nameHash);
    return true;
}


/*===========================================================================*/
/*===========================================================================*/
// DOD query scopes

core::result<u32> script_runtime::begin_query_scope(std::span<const u32> typeIds, conjure_query_scope &out) {
    out = {};
    if (!core::jobs::is_main_thread()) {
        return core::make_error(scripting_error::WRONG_THREAD, "query_begin off the main thread");
    }
    if (!is_ready()) {
        return core::make_error(scripting_error::NOT_INITIALISED, "query_begin without a ready runtime");
    }
    if (mCurrentPhase == script_phase::COUNT) {
        return core::make_error(scripting_error::NOT_IN_PHASE, "query_begin outside a script phase");
    }
    if (mWorld == nullptr) {
        return core::error(core::error_code::INVALID_ARGUMENT, "query_begin without an attached world");
    }
    if (typeIds.empty() || typeIds.size() > static_cast<usize>(QUERY_MAX_COMPONENTS)) {
        return core::error(core::error_code::INVALID_ARGUMENT, "query_begin needs 1..8 component types");
    }

    ecs::query_desc desc = {};
    query_cache_key key = {};
    key.fill(ecs::component_id::INVALID);
    for (usize index = 0; index < typeIds.size(); ++index) {
        const u32 typeId = typeIds[index];
        // 0 is unresolved_components: never handed out, so it means "lookup failed" (R4).
        if (typeId == 0 || typeId >= ecs::MAX_COMPONENTS) {
            return core::error(core::error_code::INVALID_ARGUMENT, "query_begin: component id out of range");
        }
        const ecs::component_id id{static_cast<u16>(typeId)};
        if (!mWorld->registry().is_registered(id)) {
            return core::error(core::error_code::INVALID_ARGUMENT, "query_begin: component id is not registered");
        }
        desc.with.set(id);
        desc.writes.set(id);
        key[index] = id.value;
    }
    // INVALID sorts behind every valid id, so the padding stays at the tail whatever the request order; a
    // repeated id ({a, a}) is the set {a}, so it collapses onto that set's key (the columns keep the request).
    std::sort(key.begin(), key.end());
    std::fill(std::unique(key.begin(), key.end()), key.end(), ecs::component_id::INVALID);

    // Like scheduler::run_system: a fresh version so the columns the scope stamps read as changed to the
    // systems that run after this phase.
    static_cast<void>(mWorld->bump_version());
    ecs::query &query = cached_query(key, desc);
    const u32 chunkCount = query.chunk_count();
    std::unique_ptr<query_scope_state> state = take_scope_state(chunkCount);
    state->id = mNextScopeId++;
    for (const u32 typeId : typeIds) {
        state->types.push_back(ecs::component_id{static_cast<u16>(typeId)});
    }
    state->lock = mWorld->lock_iteration();
    state->chunks.reserve(chunkCount);              // no-op once the state has seen this many chunks
    query.for_each_chunk([&state](ecs::chunk_view &view) {
        conjure_chunk_view chunk = {};
        chunk.entities = reinterpret_cast<const uint64_t *>(view.entities().data());
        chunk.count = static_cast<int32_t>(view.count());
        for (usize column = 0; column < state->types.size(); ++column) {
            // Conservative stamp: every requested column counts as written for change detection.
            chunk.columns[column] = view.column_data(state->types[column], true);
        }
        state->chunks.push_back(chunk);
    });

    // The views stay where they are until end_query_scope: the state is heap-owned and only its pointer moves.
    out.scope_id = state->id;
    out.chunk_count = static_cast<int32_t>(state->chunks.size());
    out.chunks = state->chunks.data();
    mQueryScopes.push_back(std::move(state));
    return out.scope_id;
}

void script_runtime::end_query_scope(u32 scopeId) {
    const auto found = std::find_if(
      mQueryScopes.begin(),
      mQueryScopes.end(),
      [scopeId](const std::unique_ptr<query_scope_state> &scope) {
          return scope->id == scopeId;
      }
    );
    if (found == mQueryScopes.end()) {
        CONJURE_LOG_DEBUG(LOG_SCRIPTING, "query_end for unknown scope {}", scopeId);
        return;
    }
    query_scope_state &finished = **found;
#if !CONJURE_DISTRIBUTION
    // Poison the view array: a conjure_chunk_view pointer that escaped the scope reads 0xDD until a later
    // begin_query_scope reuses this state (the reuse is the point of the free list, so the state is kept).
    if (!finished.chunks.empty()) {
        std::memset(finished.chunks.data(), 0xDD, finished.chunks.size() * sizeof(conjure_chunk_view));
    }
#endif
    // The state keeps its capacity on the free list: the next scope over as many chunks allocates nothing.
    finished.lock.release();
    finished.chunks.clear();
    finished.types.clear();
    finished.id = 0;
    mFreeQueryScopes.push_back(std::move(*found));
    mQueryScopes.erase(found);
    if (mQueryScopes.empty()) {
        flush_deferred_commands();
    }
}


/*===========================================================================*/
/*===========================================================================*/
// Ids reserved in deferred_commands()

void script_runtime::note_deferred_reserved(ecs::entity entity, std::span<const ecs::component_id> recorded) {
    if (entity.is_null()) {
        return;
    }
    deferred_reservation &reservation = mDeferredReserved[entity];
    for (const ecs::component_id id : recorded) {
        reservation.held.set(id);
    }
}

void script_runtime::note_deferred_instance(ecs::entity root) {
    if (root.is_null()) {
        return;
    }
    mDeferredReserved[root].heldKnown = false;
}

bool script_runtime::is_deferred_reserved(ecs::entity entity) const noexcept {
    return mDeferredReserved.contains(entity);
}

bool script_runtime::is_deferred_instance(ecs::entity entity) const noexcept {
    const auto found = mDeferredReserved.find(entity);
    return found != mDeferredReserved.end() && !found->second.heldKnown;
}

bool script_runtime::record_reserved_add(ecs::entity entity, ecs::component_id id, std::span<const std::byte> payload) {
    deferred_reservation *reservation = find_reservation(entity);
    if (reservation == nullptr || !mWorld->registry().is_registered(id)) {
        return false;
    }
    deferred_commands().add_raw(entity, id, payload);
    reservation->held.set(id);
    return true;
}

bool script_runtime::record_reserved_set(ecs::entity entity, ecs::component_id id, std::span<const std::byte> payload) {
    deferred_reservation *reservation = find_reservation(entity);
    if (reservation == nullptr || !mWorld->registry().is_registered(id)) {
        return false;
    }
    // command_buffer::set_raw records nothing for a tag (there is no value to replace); the call still succeeds,
    // as a set of a held tag does on a live entity.
    deferred_commands().set_raw(entity, id, payload);
    return true;
}

bool script_runtime::record_reserved_remove(ecs::entity entity, ecs::component_id id) {
    deferred_reservation *reservation = find_reservation(entity);
    if (reservation == nullptr || !mWorld->registry().is_registered(id)) {
        return false;
    }
    if (reservation->heldKnown && !reservation->held.test(id)) {
        return false;
    }
    deferred_commands().remove_raw(entity, id);
    reservation->held.clear(id);
    if (id == mScriptComponentId && reservation->scriptSet != INVALID_SET_HANDLE) {
        // In order: the scripts staged before this removal leave with the component.
        mStore.free_set(std::exchange(reservation->scriptSet, INVALID_SET_HANDLE));
    }
    return true;
}


/*===========================================================================*/
/*===========================================================================*/
// Accessors

ecs::command_buffer &script_runtime::deferred_commands() noexcept {
    CONJURE_ASSERT(mDeferredCommands != nullptr, "deferred_commands() needs an attached world");
    return *mDeferredCommands;
}

script_runtime_stats script_runtime::stats() const noexcept {
    script_runtime_stats result = {};
    result.liveInstances = static_cast<u32>(mStore.mapped_slot_count());
    result.pendingCreates = static_cast<u32>(mPendingCreates.size());
    result.pendingDestroys = static_cast<u32>(mPendingDestroySlots.size());
    result.pendingEntityDestroys = static_cast<u32>(mPendingEntityDestroys.size());
    result.openQueryScopes = static_cast<u32>(mQueryScopes.size());
    result.reloadCount = mReloadCount;
    result.zombieAssemblies = mZombieAssemblies;
    result.cachedQueries = static_cast<u32>(mQueryCache.size());
    return result;
}

script_runtime *script_runtime::active() noexcept {
    return sActiveRuntime;
}


/*===========================================================================*/
/*===========================================================================*/
// Component hooks

u32 script_runtime::clone_set_for_copy(u32 handle) {
    return mStore.clone_set(handle, ecs::NULL_ENTITY);
}

void script_runtime::free_set_from_destruct(u32 handle) {
    script_set *set = mStore.find(handle);
    if (set == nullptr) {
        return;
    }
    for (script_entry &entryValue : set->entries) {
        if (entryValue.is_live()) {
            mPendingDestroySlots.push_back(entryValue.managedSlot);
            mStore.unmap_slot(entryValue.managedSlot);
        }
    }
    std::erase_if(mPendingCreates, [handle](const pending_create &pending) {
        return pending.id.setHandle == handle;
    });
    mStore.free_set(handle);
}

void script_runtime::write_set_json(u32 handle, core::json::writer &out) {
    script_set *set = mStore.find(handle);
    out.begin_object();
    out.key("scripts");
    out.begin_array();
    if (set != nullptr) {
        write_set_entries_json(*set, out);
    }
    out.end_array();
    out.end_object();
}

core::result<u32> script_runtime::read_set_json(u32 handle, const core::json::value &in) {
    if (!in.is_object() || !in["scripts"].is_array()) {
        return core::error(core::error_code::INVALID_ARGUMENT, "script_component JSON must be {\"scripts\":[...]}");
    }
    u32 target = handle;
    const bool created = mStore.find(target) == nullptr;
    if (created) {
        target = mStore.create_set(ecs::NULL_ENTITY);
    }
    const core::result<void> replaced = replace_set_entries(target, in);
    if (!replaced.has_value()) {
        if (created) {
            static_cast<void>(mStore.free_set(target));
        }
        return replaced.get_error();
    }
    return target;
}

void script_runtime::remap_set_entities(u32 handle, const ecs::entity_remap &map) {
    script_set *set = mStore.find(handle);
    if (set == nullptr) {
        return;
    }
    const auto remap = [&map](ecs::entity value) -> ecs::entity {
        return map.remap(value);
    };
    for (script_entry &entryValue : set->entries) {
        const script_class_info *info = mClasses.find(entryValue.classHash);
        if (info == nullptr) {
            continue;
        }
        entryValue.fields = remap_entity_fields_json(*info, entryValue.fields.root(), remap);
        if (entryValue.is_live()) {
            const core::result<void> applied = apply_entry_fields(entryValue, entryValue.fields.root());
            if (!applied.has_value()) {
                CONJURE_LOG_WARN(LOG_SCRIPTING, "remap_set_entities: {}", applied.get_error());
            }
        }
    }
}


/*===========================================================================*/
/*===========================================================================*/
// Internals

bool script_runtime::is_ready() const noexcept {
    return mStatus == script_host_status::READY && mHost->status() == script_host_status::READY;
}

core::result<void> script_runtime::reload_internal(const std::filesystem::path &assemblyPath) {
    if (!is_ready()) {
        return core::make_error(scripting_error::NOT_INITIALISED, "load_game_assembly without a ready runtime");
    }
    const conjure_managed_api &managed = mHost->managed_api();

    // Destroys requested earlier in the frame (native entity destroys, Entity.Destroy) run now, with
    // OnDestroy and on the assembly that created the instances: a slot detached from its entry would
    // otherwise survive step 3 and keep the retired load context alive.
    process_pending_destroys();

    // 1. New load context; classes go to the staging registry. Failure changes nothing.
    mClasses.begin_staging();
    const std::string utf8Path = detail::path_to_utf8(assemblyPath);
    const i32 loaded = managed.load_game_assembly(utf8Path.c_str(), static_cast<i32>(utf8Path.size()));
    if (loaded != 0) {
        mClasses.discard_staging();
        return core::make_error(
          scripting_error::ASSEMBLY_LOAD_FAILED,
          "'{}' did not load (managed status {})",
          utf8Path,
          loaded
        );
    }

    // 2. Live fields into the JSON with the active schema; 3. destroy without OnDestroy.
    std::vector<i32> slots = {};
    mStore.for_each_set([&](u32, script_set &set) {
        for (script_entry &entryValue : set.entries) {
            if (entryValue.is_live()) {
                refresh_live_fields(entryValue);
                slots.push_back(entryValue.managedSlot);
            }
        }
    });
    mPendingCreates.clear();
    destroy_slots(slots, false);

    // 4. Unload the previous context (bounded on the managed side); a leak is a WARN.
    if (mAssemblyLoaded) {
        const i32 unloaded = managed.unload_game_assembly();
        if (unloaded != 0) {
            ++mZombieAssemblies;
            CONJURE_LOG_WARN(
              LOG_SCRIPTING,
              "the previous game assembly load context did not unload ({} zombie(s)); usual culprits: statics, "
              "timers, running threads",
              mZombieAssemblies
            );
        }
        ++mReloadCount;
    }

    // 5. Swap registries and recreate with the new schema. OnCreate runs once per entity lifetime: an entry
    // whose OnCreate returned is recreated with OnReload instead, and OnStart does not run again either; an entry
    // whose create was still queued, or whose OnCreate threw, runs OnCreate. The fault badges belong to the
    // destroyed instances: the managed DisabledMask starts clean, so does the flag.
    mClasses.commit_staging();
    mWarnedMissingClasses.clear();
    mWarnedMirrorNames.clear();         // a still-wrong mirror of the new assembly is reported again
    mAssemblyLoaded = true;
    mAssemblyPath = assemblyPath;
    mStore.for_each_set([](u32, script_set &set) {
        for (script_entry &entryValue : set.entries) {
            entryValue.flags &= ~script_entry_flags::FAULTED;
        }
    });
    if (mMode == script_mode::PLAY) {
        adopt_world_sets(true);
        flush_pending();
    }

    // 6. Observers (editor inspectors).
    CONJURE_LOG_INFO(LOG_SCRIPTING, "game assembly '{}' loaded: {} class(es)", utf8Path, mClasses.size());
    if (on_reloaded.is_bound()) {
        on_reloaded();
    }
    return core::result<void>::success();
}

void script_runtime::queue_create(script_entry_id id) {
    const auto duplicate = std::find_if(mPendingCreates.begin(), mPendingCreates.end(), [id](const pending_create &p) {
        return p.id == id;
    });
    if (duplicate != mPendingCreates.end()) {
        return;
    }
    pending_create pending = {};
    pending.id = id;
    mPendingCreates.push_back(pending);
}

void script_runtime::queue_creates_of_set(u32 handle) {
    const script_set *set = mStore.find(handle);
    if (set == nullptr || set->owner.is_null()) {
        return;
    }
    for (usize index = 0; index < set->entries.size(); ++index) {
        if (set->entries[index].is_live()) {
            continue;
        }
        script_entry_id id = {};
        id.setHandle = handle;
        id.entryIndex = static_cast<u8>(index);
        queue_create(id);
    }
}

void script_runtime::adopt_world_sets(bool queueCreates) {
    if (mWorld == nullptr || !mScriptComponentId.is_valid()) {
        return;
    }
    // Only the attached world's components take part: sets of a detached world (an editor's edit copy
    // while its play copy runs, 03_scripting.md 11) keep their JSON and get no instances.
    ecs::query_desc desc = {};
    desc.with.set(mScriptComponentId);
    std::vector<u32> handles = {};
    mWorld->create_query(desc).for_each_chunk([&](ecs::chunk_view &view) {
        const std::span<const ecs::entity> entities = view.entities();
        auto *components = static_cast<script_component *>(view.column_data(mScriptComponentId, true));
        if (components == nullptr) {
            return;
        }
        for (u32 row = 0; row < view.count(); ++row) {
            script_set *set = mStore.find(components[row].setHandle);
            if (set == nullptr) {
                // A component that arrived while this runtime was not observing (copied world): a fresh set,
                // exactly as the ADDED observer would have made.
                components[row].setHandle = mStore.create_set(entities[row]);
            } else {
                set->owner = entities[row];
            }
            handles.push_back(components[row].setHandle);
        }
    });
    if (!queueCreates) {
        return;
    }
    for (const u32 handle : handles) {
        queue_creates_of_set(handle);
    }
}

void script_runtime::enter_phase(script_phase phase) noexcept {
    mCurrentPhase = phase;
    mFrameBlock->current_phase = static_cast<uint8_t>(phase);
}

void script_runtime::leave_phase() noexcept {
    mCurrentPhase = script_phase::COUNT;
    mFrameBlock->current_phase = static_cast<uint8_t>(script_phase::COUNT);
}

void script_runtime::process_pending_destroys() {
    const std::vector<i32> detached = std::exchange(mPendingDestroySlots, {});
    if (!detached.empty()) {
        destroy_slots(detached, true);
    }

    const std::vector<ecs::entity> entities = std::exchange(mPendingEntityDestroys, {});
    for (const ecs::entity target : entities) {
        if (mWorld == nullptr || !mWorld->is_alive(target)) {
            continue;
        }
        std::vector<i32> slots = {};
        const script_component *component = std::as_const(*mWorld).get<script_component>(target);
        const script_set *set = component == nullptr ? nullptr : mStore.find(component->setHandle);
        if (set != nullptr) {
            for (const script_entry &entryValue : set->entries) {
                if (entryValue.is_live()) {
                    slots.push_back(entryValue.managedSlot);
                }
            }
        }
        // OnDestroy with the entity alive (03_scripting.md 3.4); the slots are unmapped before the entity
        // goes so the REMOVED observer finds nothing left to defer.
        destroy_slots(slots, true);
        static_cast<void>(mWorld->destroy_entity(target));
    }
}

void script_runtime::process_pending_creates() {
    if (mPendingCreates.empty()) {
        return;
    }
    const std::vector<pending_create> pending = std::exchange(mPendingCreates, {});
    if (!is_ready()) {
        return;
    }

    std::vector<script_entry_id> ids = {};
    std::vector<conjure_create_request> requests = {};
    ids.reserve(pending.size());
    requests.reserve(pending.size());
    for (const pending_create &item : pending) {
        script_entry *entryValue = mStore.find_entry(item.id);
        const script_set *set = mStore.find(item.id.setHandle);
        if (entryValue == nullptr || set == nullptr || entryValue->is_live()) {
            continue;
        }
        const script_class_info *info = mClasses.find(entryValue->classHash);
        if (info == nullptr) {
            entryValue->flags |= script_entry_flags::CLASS_MISSING;
            warn_missing_class_once(entryValue->classHash);
            continue;
        }
        entryValue->flags &= ~script_entry_flags::CLASS_MISSING;

        const core::result<usize> measured = json_to_blob(*info, entryValue->fields.root(), {});
        if (!measured.has_value()) {
            CONJURE_LOG_WARN(LOG_SCRIPTING, "fields of '{}' not encodable: {}", info->fullName, measured.get_error());
            continue;
        }
        const std::span<u8> blob = mScratch.allocate_span<u8>(measured.value());
        if (blob.size() != measured.value()) {
            CONJURE_LOG_WARN(LOG_SCRIPTING, "out of scratch memory while creating '{}'", info->fullName);
            continue;
        }
        static_cast<void>(json_to_blob(*info, entryValue->fields.root(), blob));

        conjure_create_request request = {};
        request.entity = set->owner.to_u64();
        request.class_hash = entryValue->classHash;
        request.fields = blob.data();
        request.fields_size = static_cast<int32_t>(blob.size());
        request.flags = 0;
        if (entryValue->has_flag(script_entry_flags::STARTED)) {
            request.flags |= CREATE_FLAG_ALREADY_STARTED;
        }
        if (entryValue->has_flag(script_entry_flags::CREATED)) {
            request.flags |= CREATE_FLAG_ALREADY_CREATED;
        }
        request.out_slot = INVALID_SLOT;
        requests.push_back(request);
        ids.push_back(item.id);
    }

    if (!requests.empty()) {
        mCreatingInstances = true;
        const i32 created = mHost->managed_api().create_instances(requests.data(), static_cast<i32>(requests.size()));
        mCreatingInstances = false;
        if (created < 0) {
            CONJURE_LOG_WARN(LOG_SCRIPTING, "create_instances returned {}", created);
        }
        for (usize index = 0; index < requests.size(); ++index) {
            const i32 slot = requests[index].out_slot;
            if (slot < 0) {
                CONJURE_LOG_WARN(LOG_SCRIPTING, "no instance created for class {:#018x}", requests[index].class_hash);
                continue;
            }
            // OnCreate may have destroyed the entity (its own or another of the batch) or replaced the
            // set's entries: an instance whose entry is gone is destroyed by the next flush iteration
            // instead of living on, unreachable from native, on a dead entity.
            script_entry *entryValue = mStore.find_entry(ids[index]);
            if (entryValue == nullptr || entryValue->is_live() || entryValue->classHash != requests[index].class_hash) {
                mPendingDestroySlots.push_back(slot);
                continue;
            }
            mStore.map_slot(slot, ids[index]);
            // OnCreate completed (or OnReload ran on an entry whose OnCreate had): a hot reload recreates this entry
            // with OnReload instead. An OnCreate that threw takes the flag back below.
            entryValue->flags |= script_entry_flags::CREATED;
        }
        for (const faulted_slot &faulted : mFaultedBeforeMapped) {
            const std::optional<script_entry_id> faultedId = mStore.resolve_slot(faulted.slot);
            script_entry *entryValue = faultedId.has_value() ? mStore.find_entry(*faultedId) : nullptr;
            if (entryValue != nullptr) {
                entryValue->flags |= script_entry_flags::FAULTED;
                if (faulted.callback == script_callback::CREATE) {
                    // An OnCreate that threw did not complete (owner ruling, OWN-RELOAD): once the author fixed it,
                    // the reload runs OnCreate once more, and OnReload only follows an OnCreate that returned.
                    entryValue->flags &= ~script_entry_flags::CREATED;
                }
                log_faulted(faulted.slot, faulted.callback, *entryValue);
            }
        }
        mFaultedBeforeMapped.clear();
        // A scope OnCreate left open would lock the world against the destroys of the next iteration.
        force_close_query_scopes();
    }
    mScratch.reset();
}

void script_runtime::destroy_slots(std::span<const i32> slots, bool callOnDestroy) {
    if (slots.empty()) {
        return;
    }
    if (is_ready()) {
        const i32 result = mHost->managed_api().destroy_instances(
          slots.data(),
          static_cast<i32>(slots.size()),
          callOnDestroy ? 1 : 0
        );
        if (result < 0) {
            CONJURE_LOG_WARN(LOG_SCRIPTING, "destroy_instances returned {}", result);
        }
        force_close_query_scopes();
    }
    for (const i32 slot : slots) {
        mStore.unmap_slot(slot);
    }
}

void script_runtime::refresh_live_fields(script_entry &entryValue) {
    if (!entryValue.is_live() || !is_ready()) {
        return;
    }
    const script_class_info *info = mClasses.find(entryValue.classHash);
    if (info == nullptr) {
        return;
    }
    const conjure_managed_api &managed = mHost->managed_api();
    const i32 slot = entryValue.managedSlot;
    i32 needed = managed.collect_fields(slot, mBlobBuffer.data(), static_cast<i32>(mBlobBuffer.size()));
    if (needed < 0) {
        CONJURE_LOG_WARN(LOG_SCRIPTING, "collect_fields(slot {}) returned {}", entryValue.managedSlot, needed);
        return;
    }
    if (static_cast<usize>(needed) > mBlobBuffer.size()) {
        mBlobBuffer.resize(static_cast<usize>(needed));
        needed = managed.collect_fields(slot, mBlobBuffer.data(), static_cast<i32>(mBlobBuffer.size()));
        if (needed < 0 || static_cast<usize>(needed) > mBlobBuffer.size()) {
            CONJURE_LOG_WARN(LOG_SCRIPTING, "collect_fields(slot {}) retry returned {}", slot, needed);
            return;
        }
    }
    const std::span<const u8> blob(mBlobBuffer.data(), static_cast<usize>(needed));
    core::result<core::json::document> decoded = blob_to_json(*info, blob);
    if (!decoded.has_value()) {
        CONJURE_LOG_WARN(LOG_SCRIPTING, "fields of '{}' not decodable: {}", info->fullName, decoded.get_error());
        return;
    }
    for (const core::json::member member : decoded.value().root().members()) {
        entryValue.fields.set(member.key, member.item);
    }
}

core::result<void> script_runtime::apply_entry_fields(script_entry &entryValue, const core::json::value &fields) {
    const script_class_info *info = mClasses.find(entryValue.classHash);
    if (info == nullptr) {
        return core::make_error(scripting_error::CLASS_NOT_FOUND, "class {:#018x} is not loaded", entryValue.classHash);
    }
    return apply_entry_fields_with(entryValue, *info, fields);
}

core::result<void> script_runtime::apply_entry_fields_with(
  script_entry &entryValue,
  const script_class_info &info,
  const core::json::value &fields
) {
    if (!entryValue.is_live() || !is_ready()) {
        return core::result<void>::success();
    }
    CONJURE_TRY_ASSIGN(const usize needed, json_to_blob(info, fields, {}));
    if (needed > mBlobBuffer.size()) {
        mBlobBuffer.resize(needed);
    }
    const std::span<u8> blob(mBlobBuffer.data(), needed);
    CONJURE_TRY(json_to_blob(info, fields, blob));
    const i32 result = mHost->managed_api().apply_fields(entryValue.managedSlot, blob.data(), static_cast<i32>(needed));
    if (result < 0) {
        return core::make_error(
          scripting_error::INSTANCE_NOT_FOUND,
          "apply_fields(slot {}) returned {}",
          entryValue.managedSlot,
          result
        );
    }
    return core::result<void>::success();
}

void script_runtime::mark_started_entries() {
    mStore.for_each_set([](u32, script_set &set) {
        for (script_entry &entryValue : set.entries) {
            // A live entry without CREATED is one whose OnCreate threw: the managed guard disabled every callback,
            // so its OnStart is never attempted, and the OnCreate a reload runs again is followed by OnStart.
            if (entryValue.is_live() && entryValue.has_flag(script_entry_flags::CREATED)) {
                entryValue.flags |= script_entry_flags::STARTED;
            }
        }
    });
}

ecs::query &script_runtime::cached_query(const query_cache_key &key, const ecs::query_desc &desc) {
    const auto found = mQueryCache.find(key);
    if (found != mQueryCache.end()) {
        return found->second;
    }
    // First scope over this set: the query's matches fill on its first refresh and grow only with new
    // archetypes afterwards, so this is the one allocation the set ever costs.
    return mQueryCache.try_emplace(key, *mWorld, desc).first->second;
}

std::unique_ptr<script_runtime::query_scope_state> script_runtime::take_scope_state(usize chunkCount) {
    if (mFreeQueryScopes.empty()) {
        return std::make_unique<query_scope_state>();
    }

    // Best fit, not last-in first-out: the free state with the smallest capacity that still holds every chunk
    // view, so a spilled buffer keeps serving the large scope while the small scopes take the inline-only ones
    // whatever order the scopes were closed in. When none fits, the largest spills (once per size step).
    auto chosen = mFreeQueryScopes.begin();
    for (auto candidate = mFreeQueryScopes.begin() + 1; candidate != mFreeQueryScopes.end(); ++candidate) {
        const usize chosenCapacity = (*chosen)->chunks.capacity();
        const usize candidateCapacity = (*candidate)->chunks.capacity();
        const bool chosenFits = chosenCapacity >= chunkCount;
        const bool candidateFits = candidateCapacity >= chunkCount;
        if (candidateFits != chosenFits) {
            if (candidateFits) {
                chosen = candidate;
            }
            continue;
        }
        // Both fit: the tighter one. Neither fits: the one that needs the smaller spill.
        const bool tighterFit = candidateFits && candidateCapacity < chosenCapacity;
        const bool smallerSpill = !candidateFits && candidateCapacity > chosenCapacity;
        if (tighterFit || smallerSpill) {
            chosen = candidate;
        }
    }
    std::unique_ptr<query_scope_state> state = std::move(*chosen);
    mFreeQueryScopes.erase(chosen);                     // erase keeps the capacity: no allocation on the path
    return state;
}

void script_runtime::force_close_query_scopes() {
    while (!mQueryScopes.empty()) {
        CONJURE_LOG_WARN(
          LOG_SCRIPTING,
          "query scope {} was left open at the end of {}; force-closed (dispose ComponentQuery with 'using')",
          mQueryScopes.back()->id,
          mCurrentPhase == script_phase::COUNT ? std::string_view("the managed call") : to_string(mCurrentPhase)
        );
        end_query_scope(mQueryScopes.back()->id);
    }
}

void script_runtime::flush_deferred_commands() {
    if (mDeferredCommands == nullptr || mWorld == nullptr || mWorld->is_locked()) {
        return;
    }
    if (!mDeferredCommands->empty()) {
        const ecs::flush_report report = mWorld->flush(*mDeferredCommands);
        if (report.skipped > 0) {
            CONJURE_LOG_DEBUG(
              LOG_SCRIPTING,
              "deferred requests: {} applied, {} skipped",
              report.applied,
              report.skipped
            );
        }
    }
    settle_deferred_reservations();
}

void script_runtime::settle_deferred_reservations() {
    if (mDeferredReserved.empty()) {
        return;
    }
    for (auto &[entity, reservation] : mDeferredReserved) {
        const bool alive = mWorld->is_alive(entity);
        if (!alive) {
            // A destroy won and the flush handed the id back through world::release_reserved, which fires no
            // destroyed observer: the name entity_create gave the id goes here, or it would stay for good.
            mEntityNames.erase(entity);
        }
        if (reservation.scriptSet == INVALID_SET_HANDLE) {
            continue;
        }
        // Not adopted by on_component_added: the id is dead (a destroy won), or it already held a script_component
        // when the flush reached the recorded one (an instantiation root whose archive carries scripts), whose
        // set then takes the staged entries after its own.
        const u32 staged = std::exchange(reservation.scriptSet, INVALID_SET_HANDLE);
        const script_component *component = alive ? std::as_const(*mWorld).get<script_component>(entity) : nullptr;
        script_set *stagedSet = mStore.find(staged);
        if (component != nullptr && stagedSet != nullptr && mStore.contains(component->setHandle)) {
            core::small_vector<script_entry, 2> entries = std::move(stagedSet->entries);
            for (script_entry &entryValue : entries) {
                const script_entry_id id = mStore.add_entry(component->setHandle, std::move(entryValue));
                if (!id.is_valid()) {
                    CONJURE_LOG_WARN(
                      LOG_SCRIPTING,
                      "add_script on {}: its set reached {} scripts at the flush; a staged script was dropped",
                      entity,
                      MAX_SCRIPTS_PER_ENTITY
                    );
                    continue;
                }
                if (mMode == script_mode::PLAY) {
                    queue_create(id);
                }
            }
        }
        mStore.free_set(staged);
    }
    mDeferredReserved.clear();
}

void script_runtime::drop_deferred_reservations() {
    for (auto &[entity, reservation] : mDeferredReserved) {
        if (reservation.scriptSet != INVALID_SET_HANDLE) {
            mStore.free_set(reservation.scriptSet);
        }
    }
    mDeferredReserved.clear();
}

script_runtime::deferred_reservation *script_runtime::find_reservation(ecs::entity entity) noexcept {
    const auto found = mDeferredReserved.find(entity);
    return found == mDeferredReserved.end() ? nullptr : &found->second;
}

script_entry_id script_runtime::stage_reserved_script(
  ecs::entity entity,
  deferred_reservation &reservation,
  u64 classHash,
  const core::json::value &fields
) {
    if (!mScriptComponentId.is_valid()) {
        return {};
    }
    if (reservation.scriptSet == INVALID_SET_HANDLE) {
        reservation.scriptSet = mStore.create_set(entity);
        if (reservation.scriptSet == INVALID_SET_HANDLE) {
            return {};
        }
    }
    script_entry entryValue = {};
    entryValue.classHash = classHash;
    entryValue.fields = fields.is_object() ? core::json::document::from_value(fields) : core::json::document::object();
    const script_entry_id id = mStore.add_entry(reservation.scriptSet, std::move(entryValue));
    if (!id.is_valid()) {
        return {};
    }
    if (!reservation.held.test(mScriptComponentId)) {
        // An empty payload: the flush places the id with a zero handle and the ADDED observer adopts the staged
        // set. On an id that already holds the component then, the add keeps that value and fires nothing, and
        // settle_deferred_reservations appends the staged entries instead.
        deferred_commands().add_raw(entity, mScriptComponentId);
        reservation.held.set(mScriptComponentId);
    }
    return id;
}

void script_runtime::warn_missing_class_once(u64 classHash) {
    const auto warned = std::find(mWarnedMissingClasses.begin(), mWarnedMissingClasses.end(), classHash);
    if (warned != mWarnedMissingClasses.end()) {
        return;
    }
    mWarnedMissingClasses.push_back(classHash);
    CONJURE_LOG_WARN(
      LOG_SCRIPTING,
      "script class {:#018x} is not in the loaded assembly; its entries keep their fields until it returns",
      classHash
    );
}

void script_runtime::log_faulted(i32 slot, script_callback callback, const script_entry &entryValue) const {
    const script_class_info *info = mClasses.find(entryValue.classHash);
    const std::string_view className = info == nullptr ? std::string_view("?") : std::string_view(info->fullName);
    if (callback == script_callback::CREATE || callback == script_callback::RELOAD) {
        // The managed guard disables every callback of an instance whose OnCreate or OnReload threw, on the
        // first fault.
        CONJURE_LOG_WARN(
          LOG_SCRIPTING,
          "{} of '{}' (slot {}) threw: every callback of the instance is disabled",
          callback == script_callback::CREATE ? std::string_view("OnCreate") : std::string_view("OnReload"),
          className,
          slot
        );
        return;
    }
    CONJURE_LOG_WARN(
      LOG_SCRIPTING,
      "{} of '{}' (slot {}) disabled after three consecutive faults",
      to_string(callback),
      className,
      slot
    );
}

void script_runtime::write_set_entries_json(script_set &set, core::json::writer &out) {
    for (script_entry &entryValue : set.entries) {
        refresh_live_fields(entryValue);
        const script_class_info *info = mClasses.find(entryValue.classHash);
        out.begin_object();
        out.key("class");
        out.value(info == nullptr ? std::string_view() : std::string_view(info->fullName));
        out.key("hash");
        out.value(entryValue.classHash);
        if (entryValue.has_flag(script_entry_flags::STARTED)) {
            out.key("started");
            out.value(true);
        }
        // OnCreate runs once per entity lifetime: a snapshot restore or a replace of the set from this JSON must
        // recreate the instance with OnReload, not replay OnCreate (the blue coin's chime, OWN-RELOAD).
        if (entryValue.has_flag(script_entry_flags::CREATED)) {
            out.key("created");
            out.value(true);
        }
        out.key("fields");
        const std::string fieldsText = entryValue.fields.to_canonical_string();
        out.write_raw_json(fieldsText.empty() ? std::string_view("{}") : std::string_view(fieldsText));
        out.end_object();
    }
}

core::result<void> script_runtime::replace_set_entries(u32 handle, const core::json::value &in) {
    if (mStore.find(handle) == nullptr) {
        return core::make_error(scripting_error::INSTANCE_NOT_FOUND, "replace_set_entries on an unknown set");
    }
    const core::json::value scripts = in["scripts"];
    if (!in.is_object() || !scripts.is_array()) {
        return core::error(core::error_code::INVALID_ARGUMENT, "expected {\"scripts\":[...]}");
    }

    // Parse first so a malformed document leaves the set untouched.
    core::small_vector<script_entry, 2> parsed = {};
    for (const core::json::value item : scripts) {
        if (!item.is_object()) {
            return core::error(core::error_code::INVALID_ARGUMENT, "every script entry must be an object");
        }
        u64 classHash = 0;
        if (item["hash"].is_number()) {
            classHash = item["hash"].as_u64();
        } else if (item["class"].is_string() && !item["class"].as_string().empty()) {
            classHash = script_name_hash(item["class"].as_string());
        }
        if (classHash == 0) {
            return core::error(core::error_code::INVALID_ARGUMENT, "a script entry needs a \"class\" or a \"hash\"");
        }
        if (parsed.size() >= MAX_SCRIPTS_PER_ENTITY) {
            CONJURE_LOG_WARN(LOG_SCRIPTING, "more than {} scripts on an entity; extra dropped", MAX_SCRIPTS_PER_ENTITY);
            break;
        }
        script_entry entryValue = {};
        entryValue.classHash = classHash;
        const core::json::value fieldsValue = item["fields"];
        entryValue.fields =
          fieldsValue.is_object() ? core::json::document::from_value(fieldsValue) : core::json::document::object();
        if (item["started"].as_bool(false)) {
            entryValue.flags |= script_entry_flags::STARTED;
        }
        // JSON written before "created" existed reads as not created: its OnCreate runs.
        if (item["created"].as_bool(false)) {
            entryValue.flags |= script_entry_flags::CREATED;
        }
        parsed.push_back(std::move(entryValue));
    }

    std::vector<i32> slots = {};
    for (const script_entry &entryValue : mStore.find(handle)->entries) {
        if (entryValue.is_live()) {
            slots.push_back(entryValue.managedSlot);
        }
    }
    destroy_slots(slots, true);
    std::erase_if(mPendingCreates, [handle](const pending_create &pending) {
        return pending.id.setHandle == handle;
    });

    // OnDestroy ran managed code: it may have added scripts (the store reallocated) or freed this very
    // set, so the set is resolved again rather than held across the call.
    script_set *set = mStore.find(handle);
    if (set == nullptr) {
        return core::make_error(scripting_error::INSTANCE_NOT_FOUND, "the script set was freed by OnDestroy");
    }
    set->entries = std::move(parsed);
    if (mMode == script_mode::PLAY) {
        queue_creates_of_set(handle);
    }
    return core::result<void>::success();
}

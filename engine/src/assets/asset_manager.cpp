#include "conjure/assets/asset_manager.h"
#include "conjure/assets/asset_loader.h"
#include "conjure/assets/log_category.h"
#include "assets/asset_slot_table.h"
#include "assets/completion_queue.h"
#include "assets/durations.h"
#include "assets/load_pipeline.h"

#include "conjure/core/assert.h"
#include "conjure/core/containers/flat_hash_map.h"
#include "conjure/core/jobs/main_thread.h"
#include "conjure/core/log.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <deque>
#include <format>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <thread>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::assets;

/*===========================================================================*/
/*===========================================================================*/
// asset_load_batch

namespace {
    /// <summary>
    /// A batch member that needs no more waiting. A RELOADING slot keeps its previous payload
    /// published, so a hot reload does not make a complete batch incomplete again. Dependency
    /// gating uses is_terminal() instead: it must wait for a reloading dependency.
    /// </summary>
    bool is_settled(const asset_handle &handle) noexcept {
        if (!handle.is_bound()) {
            return true;
        }
        const asset_state current = handle.state();
        return current == asset_state::LOADED || current == asset_state::RELOADING || current == asset_state::FAILED;
    }
}

asset_load_batch::asset_load_batch() {}

asset_load_batch::asset_load_batch(std::vector<asset_handle> handles) :
    mHandles(std::move(handles)) {}

asset_load_batch::~asset_load_batch() {}

bool asset_load_batch::is_complete() const noexcept {
    for (const asset_handle &handle : mHandles) {
        if (!is_settled(handle)) {
            return false;
        }
    }
    return true;
}

f32 asset_load_batch::progress() const noexcept {
    if (mHandles.empty()) {
        return 1.0f;
    }
    usize settled = 0;
    for (const asset_handle &handle : mHandles) {
        if (is_settled(handle)) {
            ++settled;
        }
    }
    return static_cast<f32>(settled) / static_cast<f32>(mHandles.size());
}

usize asset_load_batch::loaded_count() const noexcept {
    usize loaded = 0;
    for (const asset_handle &handle : mHandles) {
        const asset_state current = handle.state();
        if (current == asset_state::LOADED || current == asset_state::RELOADING) {
            ++loaded;
        }
    }
    return loaded;
}

/*===========================================================================*/
/*===========================================================================*/
// Private state

namespace {
    using detail::asset_slot_table;
    using detail::completion_queue;
    using detail::load_completion;
    using detail::load_stage_result;
    using detail::slot_data;

    constexpr usize priority_index(asset_load_priority priority) noexcept {
        return static_cast<usize>(priority);
    }

    constexpr std::array<asset_load_priority, ASSET_LOAD_PRIORITY_COUNT> DISPATCH_ORDER = {
        asset_load_priority::HIGH,
        asset_load_priority::NORMAL,
        asset_load_priority::LOW,
    };

    struct retired_payload {
        asset_payload payload = make_null_payload();
        u64 retireFrame = 0;
    };

    struct pending_event {
        asset_event_kind kind = asset_event_kind::LOADED;
        asset_id id = {};
        asset_type_id type = asset_type_id::INVALID;
        u32 version = 0;
        std::optional<core::error> error = std::nullopt;
    };

    struct subscriber {
        u64 id = 0;
        asset_event_callback callback = {};
    };

    bool is_in_flight(asset_state state) noexcept {
        return state == asset_state::QUEUED || state == asset_state::LOADING || state == asset_state::FINALISING ||
          state == asset_state::RELOADING;
    }
}

struct asset_manager::state {
    core::jobs::job_system *jobs = nullptr;
    u32 maxJobsInFlight = DEFAULT_MAX_JOBS_IN_FLIGHT;
    std::chrono::steady_clock::duration finaliseBudget = {};    // asset_manager_desc::finaliseBudgetSeconds
    std::chrono::steady_clock::duration blockingTimeout = {};   // asset_manager_desc::blockingTimeoutSeconds

    asset_type_registry types = {};
    mutable std::shared_mutex sourcesMutex = {};
    std::vector<std::unique_ptr<asset_source>> sources = {};   // [0] is the primary; guarded by sourcesMutex

    asset_slot_table slots = {};
    std::array<std::deque<u32>, ASSET_LOAD_PRIORITY_COUNT> queues = {};
    u64 nextSequence = 1;
    u32 jobsInFlight = 0;
    completion_queue completions = {};
    std::vector<load_completion> drained = {};
    std::vector<u32> finalising = {};                          // slot indices awaiting finalise, arrival order
    std::vector<u32> failedSlots = {};                         // slots that entered FAILED; compacted by failed_ids
    std::vector<retired_payload> retired = {};
    std::vector<pending_event> events = {};
    std::vector<subscriber> subscribers = {};
    u64 nextSubscription = 1;

    core::flat_hash_map<asset_type_id, asset_payload> fallbacks = {};
    core::flat_hash_set<asset_type_id> fallbackAttempted = {};

    u64 frameIndex = 0;
    bool inFrame = false;
    bool blockingWarnedThisFrame = false;
    bool shutDown = false;

    /*-------------------------------------------------------------------*/
    // Sources

    /// <summary>
    /// Reads the blob of an asset from the first source that knows it. The shared lock is held
    /// for the whole read, so swap_source() / add_source() (unique lock) can never destroy a
    /// source while a job is inside its read(); load_cpu then runs on the copy without the lock.
    /// Any thread.
    /// </summary>
    [[nodiscard]] core::result<cooked_blob> read_blob(const asset_id &id) const {
        const std::shared_lock lock(sourcesMutex);
        for (const auto &source : sources) {
            if (source->contains(id)) {
                return source->read(id);
            }
        }
        return make_asset_error(asset_error_code::NOT_FOUND, std::format("asset {} is not in any source", id));
    }

    [[nodiscard]] std::optional<asset_type_id> source_type_of(const asset_id &id) const {
        const std::shared_lock lock(sourcesMutex);
        for (const auto &source : sources) {
            if (source->contains(id)) {
                return source->type_of(id);
            }
        }
        return std::nullopt;
    }

    /*-------------------------------------------------------------------*/
    // Slots

    [[nodiscard]] asset_slot *find_or_create(const asset_id &id) {
        bool created = false;
        asset_slot *slot = slots.find_or_create(id, created);
        if (slot != nullptr && slot->type.load(std::memory_order_acquire) == asset_type_id::INVALID) {
            const std::optional<asset_type_id> known = source_type_of(id);
            if (known.has_value()) {
                slot->type.store(*known, std::memory_order_release);
            }
        }
        return slot;
    }

    void set_state(asset_slot &slot, asset_state newState) noexcept {
        slot.state.store(newState, std::memory_order_release);
    }

    void queue_event(asset_slot &slot, asset_event_kind kind, std::optional<core::error> error = std::nullopt) {
        pending_event event = {};
        event.kind = kind;
        event.id = slot.id;
        event.type = slot.type.load(std::memory_order_acquire);
        event.version = slot.version.load(std::memory_order_acquire);
        event.error = std::move(error);
        events.push_back(std::move(event));
    }

    void enqueue(asset_slot &slot, slot_data &data, asset_load_priority priority) {
        data.priority = priority;
        data.sequence = nextSequence;
        ++nextSequence;
        data.queued = true;
        queues[priority_index(priority)].push_back(slot.index);
    }

    /*-------------------------------------------------------------------*/
    // Pipeline stages

    void dispatch() {
        for (const asset_load_priority priority : DISPATCH_ORDER) {
            std::deque<u32> &queue = queues[priority_index(priority)];
            while (!queue.empty() && jobsInFlight < maxJobsInFlight) {
                const u32 index = queue.front();
                queue.pop_front();

                asset_slot &slot = slots.at(index);
                slot_data &data = slots.data(slot);
                const asset_state current = slot.state.load(std::memory_order_acquire);
                const bool live = data.queued && data.priority == priority &&
                  (current == asset_state::QUEUED || current == asset_state::RELOADING);
                if (!live) {
                    continue;   // stale entry: the slot was taken inline, re-prioritised or reset
                }
                data.queued = false;
                if (current == asset_state::QUEUED) {
                    set_state(slot, asset_state::LOADING);
                }
                schedule_load(slot, data);
            }
        }
    }

    void schedule_load(asset_slot &slot, slot_data &data) {
        CONJURE_ASSERT(!data.jobOutstanding, "asset {} already has a load job in flight", slot.id);
        ++data.generation;
        ++jobsInFlight;
        data.jobOutstanding = true;
        const u32 index = slot.index;
        const u32 generation = data.generation;
        const asset_id id = slot.id;
        const asset_type_id expected = slot.type.load(std::memory_order_acquire);
        state *self = this;
        data.job = jobs->schedule([self, index, generation, id, expected]() {
            load_completion completion = {};
            completion.slotIndex = index;
            completion.generation = generation;
            load_stage_result stage = detail::load_cpu_stage(self->types, id, expected, self->read_blob(id));
            completion.header = stage.header;
            completion.dependencies = std::move(stage.dependencies);
            completion.output = std::move(stage.output);
            self->completions.push(std::move(completion));
        });
    }

    /// <summary>
    /// Runs the worker stage inline on the caller (load_blocking).
    /// </summary>
    [[nodiscard]] load_stage_result run_stage_inline(const asset_slot &slot) const {
        return detail::load_cpu_stage(types, slot.id, slot.type.load(std::memory_order_acquire), read_blob(slot.id));
    }

    void drain_completions() {
        drained.clear();
        completions.drain(drained);
        for (load_completion &completion : drained) {
            // Every scheduled job produces exactly one completion, so the in-flight count comes
            // down here whatever became of the slot meanwhile; dispatch concurrency never leaks.
            if (jobsInFlight > 0) {
                --jobsInFlight;
            }
            asset_slot &slot = slots.at(completion.slotIndex);
            slot_data &data = slots.data(slot);
            if (completion.generation != data.generation || !data.jobOutstanding) {
                continue;   // superseded by a later load
            }
            data.jobOutstanding = false;
            data.job.reset();
            if (shutDown) {
                discard_in_flight(slot, data);
                continue;
            }
            const asset_state current = slot.state.load(std::memory_order_acquire);
            if (current != asset_state::LOADING && current != asset_state::RELOADING) {
                continue;   // the slot left the pipeline while the job ran: its result is stale
            }
            load_stage_result stage = {};
            stage.header = completion.header;
            stage.dependencies = std::move(completion.dependencies);
            stage.output = std::move(completion.output);
            accept_stage(slot, data, std::move(stage));
        }
        drained.clear();
    }

    /// <summary>
    /// Applies the worker stage's result: failure, or dependencies + FINALISING.
    /// </summary>
    void accept_stage(asset_slot &slot, slot_data &data, load_stage_result &&stage) {
        if (!stage.output.has_value()) {
            fail(slot, data, std::move(stage.output).get_error());
            return;
        }

        data.cpuOutput = std::move(stage.output).value();
        data.dependencyIds = std::move(stage.dependencies);
        data.pendingPayloadBytes = stage.header.payloadSize;
        if (slot.type.load(std::memory_order_acquire) == asset_type_id::INVALID) {
            slot.type.store(stage.header.type, std::memory_order_release);
        }

        data.pendingDependencyHandles.clear();
        for (const asset_id &dependency : data.dependencyIds) {
            data.pendingDependencyHandles.push_back(load(dependency, data.priority));
        }

        std::vector<u32> cycle = {};
        if (finds_cycle(slot.index, cycle)) {
            for (const u32 member : cycle) {
                asset_slot &memberSlot = slots.at(member);
                slot_data &memberData = slots.data(memberSlot);
                fail(
                  memberSlot,
                  memberData,
                  make_asset_error(
                    asset_error_code::CYCLIC_DEPENDENCY,
                    std::format("asset {} is part of a dependency cycle", memberSlot.id)
                  )
                );
            }
            return;
        }

        set_state(slot, data.isReload ? asset_state::RELOADING : asset_state::FINALISING);
        data.awaitingFinalise = true;
        finalising.push_back(slot.index);
    }

    /// <summary>
    /// Iterative depth-first search from 'start' (whose dependency list was just received) over
    /// the slots waiting to finalise. Only those lists are authoritative: a QUEUED, LOADING or
    /// in-flight RELOADING slot still carries the ids of its previous blob, and it runs its own
    /// check when its stage completes, so following it would report cycles that no longer exist
    /// (a user reversing a dependency, for instance). Terminal and UNLOADED slots cannot be
    /// waiting on anything. Fills 'outCycle' with the members of the first cycle through 'start'.
    /// </summary>
    [[nodiscard]] bool finds_cycle(u32 start, std::vector<u32> &outCycle) {
        struct frame {
            u32 index = 0;
            usize nextDependency = 0;
        };
        std::vector<frame> path = {};
        core::flat_hash_set<u32> visited = {};
        path.push_back(frame{start, 0});
        visited.insert(start);

        while (!path.empty()) {
            frame &current = path.back();
            const slot_data &data = slots.data(slots.at(current.index));
            if (current.nextDependency >= data.dependencyIds.size()) {
                path.pop_back();
                continue;
            }
            const asset_slot *next = slots.find(data.dependencyIds[current.nextDependency]);
            ++current.nextDependency;
            if (next == nullptr) {
                continue;
            }
            if (next->index == start) {
                outCycle.clear();
                for (const frame &member : path) {
                    outCycle.push_back(member.index);
                }
                return true;
            }
            if (!slots.data(*next).awaitingFinalise || !visited.insert(next->index).second) {
                continue;
            }
            path.push_back(frame{next->index, 0});
        }
        return false;
    }

    [[nodiscard]] bool dependencies_terminal(const slot_data &data) const noexcept {
        for (const asset_handle &dependency : data.pendingDependencyHandles) {
            if (!dependency.is_terminal()) {
                return false;
            }
        }
        return true;
    }

    void finalise_pass() {
        if (finalising.empty()) {
            return;
        }
        // HIGH first, FIFO within a level.
        std::stable_sort(finalising.begin(), finalising.end(), [this](u32 left, u32 right) {
            const slot_data &a = slots.data(slots.at(left));
            const slot_data &b = slots.data(slots.at(right));
            if (a.priority != b.priority) {
                return a.priority > b.priority;
            }
            return a.sequence < b.sequence;
        });

        const auto start = std::chrono::steady_clock::now();
        u32 finalised = 0;
        std::vector<u32> remaining = {};
        remaining.reserve(finalising.size());
        bool budgetExhausted = false;
        for (const u32 index : finalising) {
            asset_slot &slot = slots.at(index);
            slot_data &data = slots.data(slot);
            const asset_state current = slot.state.load(std::memory_order_acquire);
            if (current != asset_state::FINALISING && current != asset_state::RELOADING) {
                continue;   // failed or reset meanwhile
            }
            if (budgetExhausted || !dependencies_terminal(data)) {
                remaining.push_back(index);
                continue;
            }
            finalise_slot(slot, data);
            ++finalised;
            if (finalised >= 1 && std::chrono::steady_clock::now() - start >= finaliseBudget) {
                budgetExhausted = true;
            }
        }
        finalising.swap(remaining);
    }

    void finalise_slot(asset_slot &slot, slot_data &data) {
        data.awaitingFinalise = false;
        for (const asset_handle &dependency : data.pendingDependencyHandles) {
            if (dependency.state() == asset_state::FAILED && !data.dependencyFailureWarned) {
                data.dependencyFailureWarned = true;
                CONJURE_LOG_WARN(
                  LOG_ASSETS,
                  "asset {} depends on {} which failed to load; it will read the fallback",
                  slot.id,
                  dependency.id()
                );
            }
        }

        const asset_type_id type = slot.type.load(std::memory_order_acquire);
        const asset_loader *loader = types.find_loader(type);
        if (loader == nullptr) {
            fail(
              slot,
              data,
              make_asset_error(
                asset_error_code::NO_LOADER,
                std::format("no loader for type {:#x}", static_cast<u64>(type))
              )
            );
            return;
        }

        finalise_context context = {};
        context.id = slot.id;
        context.dependencies = data.pendingDependencyHandles;
        context.frameIndex = frameIndex;
        context.isReload = data.isReload;
        core::result<asset_payload> finished = loader->finalise(context, std::move(data.cpuOutput));
        data.cpuOutput = make_null_payload();
        if (!finished.has_value()) {
            const core::error &failure = finished.get_error();
            if (failure.is_category<asset_error_code>()) {
                fail(slot, data, failure);
            } else {
                fail(
                  slot,
                  data,
                  make_asset_error(
                    asset_error_code::FINALISE_FAILED,
                    std::format("finalise of {} failed: {}", slot.id, failure.to_string())
                  )
                );
            }
            return;
        }
        publish(slot, data, std::move(finished).value());
    }

    void publish(asset_slot &slot, slot_data &data, asset_payload &&payload) {
        const bool wasReload = data.isReload;
        asset_payload previous = std::move(data.payload);
        data.payload = std::move(payload);
        slot.object.store(data.payload.get(), std::memory_order_release);
        slot.version.fetch_add(1, std::memory_order_acq_rel);
        set_state(slot, asset_state::LOADED);
        data.dependencyHandles = std::move(data.pendingDependencyHandles);
        data.pendingDependencyHandles.clear();
        data.payloadBytes = data.pendingPayloadBytes;
        data.lastError.reset();
        data.isReload = false;
        if (previous != nullptr) {
            retired.push_back(retired_payload{std::move(previous), frameIndex});
        }
        queue_event(slot, wasReload ? asset_event_kind::RELOADED : asset_event_kind::LOADED);
    }

    void fail(asset_slot &slot, slot_data &data, core::error failure) {
        if (slot.state.load(std::memory_order_acquire) == asset_state::FAILED && !data.isReload) {
            return;   // already reported
        }
        // The job handle is left alone: an outstanding job still drains (and is waited for at
        // shutdown) through drain_completions(), which owns the in-flight accounting.
        data.cpuOutput = make_null_payload();
        data.pendingDependencyHandles.clear();
        data.awaitingFinalise = false;
        data.lastError = failure;
        if (data.isReload) {
            // The old payload stays published, and so do its dependencies.
            data.isReload = false;
            restore_published_dependencies(data);
            set_state(slot, asset_state::LOADED);
            CONJURE_LOG_WARN(LOG_ASSETS, "reload of asset {} failed: {}", slot.id, failure.to_string());
            queue_event(slot, asset_event_kind::RELOAD_FAILED, std::move(failure));
            return;
        }
        set_state(slot, asset_state::FAILED);
        if (!data.inFailedList) {
            data.inFailedList = true;
            failedSlots.push_back(slot.index);
        }
        if (failure.is(asset_error_code::NOT_FOUND)) {
            if (!data.notFoundLogged) {
                data.notFoundLogged = true;
                CONJURE_LOG_ERROR(LOG_ASSETS, "asset {} failed to load: {}", slot.id, failure.to_string());
            }
        } else {
            CONJURE_LOG_WARN(LOG_ASSETS, "asset {} failed to load: {}", slot.id, failure.to_string());
        }
        queue_event(slot, asset_event_kind::FAILED, std::move(failure));
    }

    void discard_in_flight(asset_slot &slot, slot_data &data) {
        data.cpuOutput = make_null_payload();
        data.pendingDependencyHandles.clear();
        data.awaitingFinalise = false;
        data.queued = false;
        if (!data.jobOutstanding) {
            data.job.reset();
        }
        if (data.isReload) {
            data.isReload = false;
            restore_published_dependencies(data);
            set_state(slot, asset_state::LOADED);
            return;
        }
        set_state(slot, asset_state::UNLOADED);
    }

    /// <summary>
    /// accept_stage() replaced the dependency list with the one of the load in flight; when that
    /// load is abandoned the published payload's list (the ids of its dependency handles) is back.
    /// </summary>
    void restore_published_dependencies(slot_data &data) {
        data.dependencyIds.clear();
        for (const asset_handle &dependency : data.dependencyHandles) {
            data.dependencyIds.push_back(dependency.id());
        }
    }

    void retire_pass() {
        std::erase_if(retired, [this](const retired_payload &item) {
            return item.retireFrame + RETIRE_FRAMES <= frameIndex;
        });
    }

    void fire_events() {
        if (events.empty()) {
            return;
        }
        std::vector<pending_event> firing = {};
        firing.swap(events);

        // The copy keeps a running callback alive; the live list decides who is still called; a
        // subscriber added during delivery starts with the next update().
        const std::vector<subscriber> listeners = subscribers;
        for (const pending_event &pending : firing) {
            asset_event event = {};
            event.kind = pending.kind;
            event.id = pending.id;
            event.type = pending.type;
            event.version = pending.version;
            event.error = pending.error.has_value() ? &*pending.error : nullptr;
            for (const subscriber &listener : listeners) {
                if (listener.callback && is_subscribed(listener.id)) {
                    listener.callback(event);
                }
            }
        }
    }

    [[nodiscard]] bool is_subscribed(u64 subscriptionId) const noexcept {
        return std::any_of(subscribers.begin(), subscribers.end(), [subscriptionId](const subscriber &listener) {
            return listener.id == subscriptionId;
        });
    }

    /*-------------------------------------------------------------------*/
    // Public operations implemented on the state

    [[nodiscard]] asset_handle load(const asset_id &id, asset_load_priority priority) {
        asset_slot *slot = find_or_create(id);
        if (slot == nullptr) {
            return asset_handle();
        }
        slot_data &data = slots.data(*slot);
        const asset_state current = slot->state.load(std::memory_order_acquire);
        if (current == asset_state::UNLOADED) {
            if (shutDown) {
                fail(*slot, data, make_asset_error(asset_error_code::SHUTTING_DOWN, "the asset manager is shut down"));
            } else {
                set_state(*slot, asset_state::QUEUED);
                enqueue(*slot, data, priority);
            }
        } else if (current == asset_state::QUEUED && priority > data.priority && data.queued) {
            enqueue(*slot, data, priority);   // the old queue entry becomes stale
        }
        return asset_handle(slot);
    }

    void reload(const asset_id &id) {
        asset_slot *slot = slots.find(id);
        if (slot == nullptr || shutDown) {
            return;
        }
        if (slot->state.load(std::memory_order_acquire) != asset_state::LOADED) {
            return;
        }
        slot_data &data = slots.data(*slot);
        data.isReload = true;
        set_state(*slot, asset_state::RELOADING);
        enqueue(*slot, data, data.priority);
    }

    void retry_failed(const asset_id &id) {
        asset_slot *slot = slots.find(id);
        if (slot == nullptr || shutDown) {
            return;
        }
        if (slot->state.load(std::memory_order_acquire) != asset_state::FAILED) {
            return;
        }
        slot_data &data = slots.data(*slot);
        data.lastError.reset();
        set_state(*slot, asset_state::QUEUED);
        enqueue(*slot, data, data.priority);
    }

    [[nodiscard]] core::result<asset_handle> load_blocking(const asset_id &id) {
        if (shutDown) {
            return make_asset_error(asset_error_code::SHUTTING_DOWN, "the asset manager is shut down");
        }
        asset_slot *slot = find_or_create(id);
        if (slot == nullptr) {
            return core::error(core::error_code::INVALID_ARGUMENT, "cannot load the null asset id");
        }
        if (inFrame && !blockingWarnedThisFrame) {
            blockingWarnedThisFrame = true;
            CONJURE_LOG_WARN(
              LOG_ASSETS,
              "load_blocking({}) inside frame {} is a hitch; prefer load() or load_batch()",
              id,
              frameIndex
            );
        }

        blocking_context context = {};
        context.deadline = std::chrono::steady_clock::now() + blockingTimeout;
        core::result<void> loaded = load_blocking_slot(*slot, context);
        if (!loaded.has_value()) {
            return std::move(loaded).get_error();
        }
        return asset_handle(slot);
    }

    /// <summary>
    /// State of one load_blocking call: the slots on the recursion stack and, while a cycle is
    /// being unwound, the slot where it was detected (every frame between it and the detection
    /// point is a member of the cycle).
    /// </summary>
    struct blocking_context {
        core::flat_hash_set<u32> visiting = {};
        std::optional<u32> cycleRoot = std::nullopt;
        std::chrono::steady_clock::time_point deadline = {};
    };

    /// <summary>
    /// True while a scheduled job is responsible for the slot: a first load (LOADING) or a reload
    /// that was dispatched (RELOADING whose job has not drained yet).
    /// </summary>
    [[nodiscard]] static bool job_owns_slot(const asset_slot &slot, const slot_data &data) noexcept {
        const asset_state current = slot.state.load(std::memory_order_acquire);
        if (current == asset_state::LOADING) {
            return true;
        }
        return current == asset_state::RELOADING && !data.queued && data.jobOutstanding;
    }

    [[nodiscard]] core::error cycle_error(const asset_slot &slot) const {
        return make_asset_error(
          asset_error_code::CYCLIC_DEPENDENCY,
          std::format("asset {} is part of a dependency cycle", slot.id)
        );
    }

    [[nodiscard]] core::result<void> load_blocking_slot(asset_slot &slot, blocking_context &context) {
        slot_data &data = slots.data(slot);
        core::flat_hash_set<u32> &visiting = context.visiting;
        const std::chrono::steady_clock::time_point deadline = context.deadline;
        if (!visiting.insert(slot.index).second) {
            context.cycleRoot = slot.index;
            fail(slot, data, cycle_error(slot));
            return cycle_error(slot);
        }

        // Wait for a job that already owns this slot, then let the drained completion advance it.
        while (job_owns_slot(slot, data)) {
            if (data.job.is_complete()) {
                drain_completions();
                continue;
            }
            if (std::chrono::steady_clock::now() >= deadline) {
                visiting.erase(slot.index);
                return make_asset_error(
                  asset_error_code::TIMEOUT,
                  std::format("load_blocking of {} timed out", slot.id)
                );
            }
            std::this_thread::yield();
        }

        asset_state current = slot.state.load(std::memory_order_acquire);
        if (current == asset_state::LOADED) {
            visiting.erase(slot.index);
            return core::status::success();
        }
        if (current == asset_state::FAILED) {
            visiting.erase(slot.index);
            return data.lastError.value_or(make_asset_error(asset_error_code::LOAD_FAILED, "asset is FAILED"));
        }

        if (current == asset_state::UNLOADED || current == asset_state::QUEUED ||
            (current == asset_state::RELOADING && data.queued)) {
            data.queued = false;   // the queue entry (if any) becomes stale
            if (current != asset_state::RELOADING) {
                set_state(slot, asset_state::LOADING);
            }
            load_stage_result stage = run_stage_inline(slot);
            accept_stage(slot, data, std::move(stage));
            std::erase(finalising, slot.index);
            current = slot.state.load(std::memory_order_acquire);
            if (current == asset_state::FAILED) {
                visiting.erase(slot.index);
                return *data.lastError;
            }
            if (current == asset_state::LOADED) {
                // A failed reload keeps the old payload published.
                visiting.erase(slot.index);
                return data.lastError.has_value() ? core::result<void>(*data.lastError) : core::status::success();
            }
        }

        // FINALISING (or RELOADING with a completed stage): dependencies first, depth-first.
        std::erase(finalising, slot.index);
        for (usize index = 0; index < data.dependencyIds.size(); ++index) {
            asset_slot *dependency = slots.find(data.dependencyIds[index]);
            if (dependency == nullptr) {
                continue;
            }
            core::result<void> dependencyLoaded = load_blocking_slot(*dependency, context);
            if (!dependencyLoaded.has_value() && dependencyLoaded.get_error().is(asset_error_code::TIMEOUT)) {
                // Still FINALISING/RELOADING: the async pipeline finalises it once the dependency lands.
                const bool listed = std::find(finalising.begin(), finalising.end(), slot.index) != finalising.end();
                if (data.awaitingFinalise && !listed) {
                    finalising.push_back(slot.index);
                }
                visiting.erase(slot.index);
                return dependencyLoaded;
            }
            const bool insideCycle = context.cycleRoot.has_value() && visiting.contains(*context.cycleRoot);
            if (
              !dependencyLoaded.has_value() && dependencyLoaded.get_error().is(
                asset_error_code::CYCLIC_DEPENDENCY
              ) && insideCycle
            ) {
                fail(slot, data, cycle_error(slot));
                if (*context.cycleRoot == slot.index) {
                    context.cycleRoot.reset();   // the whole cycle is unwound
                }
                visiting.erase(slot.index);
                return cycle_error(slot);
            }
        }
        if (slot.state.load(std::memory_order_acquire) == asset_state::FAILED) {
            visiting.erase(slot.index);
            return *data.lastError;
        }
        finalise_slot(slot, data);
        visiting.erase(slot.index);
        if (slot.state.load(std::memory_order_acquire) == asset_state::FAILED) {
            return *data.lastError;
        }
        if (data.lastError.has_value()) {
            return *data.lastError;   // a reload that failed and kept the old payload
        }
        return core::status::success();
    }

    u32 collect() {
        u32 unloaded = 0;
        bool changed = true;
        while (changed) {
            changed = false;
            for (u32 index = 0; index < slots.size(); ++index) {
                asset_slot &slot = slots.at(index);
                slot_data &data = slots.data(slot);
                if (slot.state.load(std::memory_order_acquire) != asset_state::LOADED || data.pinned) {
                    continue;
                }
                if (slot.strongRefs.load(std::memory_order_acquire) != 0) {
                    continue;
                }
                slot.object.store(nullptr, std::memory_order_release);
                slot.version.fetch_add(1, std::memory_order_acq_rel);
                set_state(slot, asset_state::UNLOADED);
                asset_payload previous = std::move(data.payload);
                data.payload = make_null_payload();
                if (previous != nullptr) {
                    retired.push_back(retired_payload{std::move(previous), frameIndex});
                }
                data.dependencyHandles.clear();
                data.dependencyIds.clear();
                data.payloadBytes = 0;
                data.lastError.reset();   // an earlier failed reload says nothing about the next load
                queue_event(slot, asset_event_kind::UNLOADED);
                ++unloaded;
                changed = true;
            }
        }
        return unloaded;
    }

    [[nodiscard]] bool has_pending() const noexcept {
        for (u32 index = 0; index < slots.size(); ++index) {
            if (is_in_flight(slots.at(index).state.load(std::memory_order_acquire))) {
                return true;
            }
        }
        return false;
    }

    void wait_for_jobs() {
        for (u32 index = 0; index < slots.size(); ++index) {
            slot_data &data = slots.data(slots.at(index));
            if (data.job.is_valid() && !data.job.is_complete()) {
                jobs->wait(data.job);
            }
        }
    }

    void update() {
        dispatch();
        drain_completions();
        finalise_pass();
        retire_pass();
        fire_events();
    }

    void flush() {
        while (has_pending() && !shutDown) {
            update();
            if (has_pending()) {
                wait_for_jobs();
            }
        }
    }

    void shutdown() {
        if (shutDown) {
            return;
        }
        shutDown = true;
        wait_for_jobs();
        drain_completions();
        for (auto &queue : queues) {
            queue.clear();
        }
        for (u32 index = 0; index < slots.size(); ++index) {
            asset_slot &slot = slots.at(index);
            slot_data &data = slots.data(slot);
            if (is_in_flight(slot.state.load(std::memory_order_acquire))) {
                discard_in_flight(slot, data);
            }
        }
        finalising.clear();
        events.clear();
        jobsInFlight = 0;
    }

    [[nodiscard]] void *fallback_payload(asset_type_id type) {
        const auto existing = fallbacks.find(type);
        if (existing != fallbacks.end()) {
            return existing->second.get();
        }
        // Nothing is recorded for a type without a loader: a later registration may provide one,
        // and its fallback must then be created on the next request.
        const asset_loader *loader = types.find_loader(type);
        if (loader == nullptr) {
            return nullptr;
        }
        if (!fallbackAttempted.insert(type).second) {
            return nullptr;
        }
        core::result<asset_payload> created = loader->create_fallback();
        if (!created.has_value()) {
            CONJURE_LOG_WARN(
              LOG_ASSETS,
              "fallback for type {:#x} failed: {}",
              static_cast<u64>(type),
              created.get_error().to_string()
            );
            return nullptr;
        }
        if (created.value() == nullptr) {
            return nullptr;
        }
        void *object = created.value().get();
        fallbacks.insert_or_assign(type, std::move(created).value());
        return object;
    }

    [[nodiscard]] bool type_in_use(asset_type_id type) const noexcept {
        for (u32 index = 0; index < slots.size(); ++index) {
            const asset_slot &slot = slots.at(index);
            if (slot.type.load(std::memory_order_acquire) != type) {
                continue;
            }
            const asset_state current = slot.state.load(std::memory_order_acquire);
            if (current == asset_state::LOADED || is_in_flight(current)) {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] asset_manager_stats stats() const noexcept {
        asset_manager_stats result = {};
        result.slots = slots.size();
        for (u32 index = 0; index < slots.size(); ++index) {
            const asset_slot &slot = slots.at(index);
            const asset_state current = slot.state.load(std::memory_order_acquire);
            if (current == asset_state::LOADED || current == asset_state::RELOADING) {
                ++result.loaded;
                result.bytesResident += slots.data(slot).payloadBytes;
            }
            if (is_in_flight(current)) {
                ++result.pending;
            }
            if (current == asset_state::FAILED) {
                ++result.failed;
            }
        }
        result.retired = static_cast<u32>(retired.size());
        result.jobsInFlight = jobsInFlight;
        result.frameIndex = frameIndex;
        return result;
    }
};

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

asset_manager::asset_manager(asset_manager_desc desc) :
    mState(std::make_unique<state>()) {
    CONJURE_ASSERT(desc.jobs != nullptr, "asset_manager needs a job system");
    CONJURE_ASSERT(core::jobs::is_main_thread(), "asset_manager must be created on the main thread");
    mState->jobs = desc.jobs;
    mState->maxJobsInFlight = std::max<u32>(1, desc.maxJobsInFlight);
    mState->finaliseBudget = detail::to_steady_duration(desc.finaliseBudgetSeconds);
    mState->blockingTimeout = detail::to_steady_duration(desc.blockingTimeoutSeconds);
    if (desc.source != nullptr) {
        mState->sources.push_back(std::move(desc.source));
    }

    asset_type_registry_hooks hooks = {};
    state *self = mState.get();
    hooks.isInUse = [self](asset_type_id type) {
        return self->type_in_use(type);
    };
    hooks.onRegistered = [self](const asset_type_desc &typeDesc) {
        if (has_flag(typeDesc.flags, asset_type_flags::PINNED_FALLBACK)) {
            static_cast<void>(self->fallback_payload(typeDesc.id));
        }
    };
    hooks.onUnregistered = [self](asset_type_id type) {
        self->fallbacks.erase(type);
        self->fallbackAttempted.erase(type);
    };
    mState->types.set_hooks(std::move(hooks));
}

asset_manager::~asset_manager() {
    mState->shutdown();

    // The mismatch log is process-wide (asset_ref has no manager to ask); a new manager, such as a
    // reopened project, reports its mismatches afresh.
    detail::forget_type_mismatches();

    // Internal handles go first so the external count below is exact.
    for (u32 index = 0; index < mState->slots.size(); ++index) {
        slot_data &data = mState->slots.data(mState->slots.at(index));
        data.dependencyHandles.clear();
        data.pendingDependencyHandles.clear();
    }

    i32 externalReferences = 0;
    for (u32 index = 0; index < mState->slots.size(); ++index) {
        externalReferences += mState->slots.at(index).strongRefs.load(std::memory_order_acquire);
    }
    if (externalReferences != 0) {
        CONJURE_LOG_ERROR(LOG_ASSETS, "asset_manager destroyed with {} live asset_handle(s)", externalReferences);
    }
    CONJURE_ASSERT(externalReferences == 0, "asset_manager destroyed with {} live asset_handle(s)", externalReferences);

    // Payloads are destroyed while their loaders (owned by the registry inside the state) exist.
    for (u32 index = 0; index < mState->slots.size(); ++index) {
        asset_slot &slot = mState->slots.at(index);
        slot_data &data = mState->slots.data(slot);
        slot.object.store(nullptr, std::memory_order_release);
        data.payload = make_null_payload();
        data.cpuOutput = make_null_payload();
    }
    mState->retired.clear();
    mState->fallbacks.clear();
}

/*===========================================================================*/
/*===========================================================================*/
// Public methods

asset_type_registry &asset_manager::types() noexcept {
    return mState->types;
}

const asset_type_registry &asset_manager::types() const noexcept {
    return mState->types;
}

void asset_manager::add_source(std::unique_ptr<asset_source> source) {
    CONJURE_ASSERT(core::jobs::is_main_thread(), "asset_manager::add_source is main-thread only");
    if (source == nullptr) {
        return;
    }
    const std::unique_lock lock(mState->sourcesMutex);
    mState->sources.push_back(std::move(source));
}

void asset_manager::swap_source(std::unique_ptr<asset_source> source) {
    CONJURE_ASSERT(core::jobs::is_main_thread(), "asset_manager::swap_source is main-thread only");

    // The unique lock waits for every read in flight (state::read_blob holds the shared lock
    // around source->read()), so the old source is never destroyed under a running read. Jobs
    // that have not started yet resolve the new source; the old one dies after the lock is gone.
    std::unique_ptr<asset_source> retired = nullptr;
    {
        const std::unique_lock lock(mState->sourcesMutex);
        if (source == nullptr) {
            if (!mState->sources.empty()) {
                retired = std::move(mState->sources.front());
                mState->sources.erase(mState->sources.begin());
            }
        } else if (mState->sources.empty()) {
            mState->sources.push_back(std::move(source));
        } else {
            retired = std::move(mState->sources.front());
            mState->sources.front() = std::move(source);
        }
    }
    retired.reset();
}

const asset_slot *asset_manager::bind_typed(const asset_id &id, asset_type_id type) {
    CONJURE_ASSERT(core::jobs::is_main_thread(), "asset_manager::bind is main-thread only");
    asset_slot *slot = mState->find_or_create(id);
    if (slot == nullptr) {
        return nullptr;
    }
    const asset_type_id recorded = slot->type.load(std::memory_order_acquire);
    if (recorded == asset_type_id::INVALID) {
        slot->type.store(type, std::memory_order_release);
        return slot;
    }
    if (recorded != type) {
        detail::report_type_mismatch(id, type, recorded);
        return nullptr;
    }
    return slot;
}

const asset_slot *asset_manager::bind_slot(const asset_id &id) {
    CONJURE_ASSERT(core::jobs::is_main_thread(), "asset_manager::bind_slot is main-thread only");
    return mState->find_or_create(id);
}

asset_handle asset_manager::make_handle(const asset_id &id) {
    CONJURE_ASSERT(core::jobs::is_main_thread(), "asset_manager::make_handle is main-thread only");
    return asset_handle(mState->find_or_create(id));
}

const asset_slot *asset_manager::find_slot(const asset_id &id) const noexcept {
    return mState->slots.find(id);
}

std::optional<asset_type_id> asset_manager::type_of(const asset_id &id) const {
    const asset_slot *slot = mState->slots.find(id);
    if (slot != nullptr) {
        const asset_type_id recorded = slot->type.load(std::memory_order_acquire);
        if (recorded != asset_type_id::INVALID) {
            return recorded;
        }
    }
    const std::shared_lock lock(mState->sourcesMutex);
    for (const auto &source : mState->sources) {
        const std::optional<asset_type_id> type = source->type_of(id);
        if (type.has_value()) {
            return type;
        }
    }
    return std::nullopt;
}

std::optional<asset_id> asset_manager::find(std::string_view assetPath) const {
    const std::shared_lock lock(mState->sourcesMutex);
    for (const auto &source : mState->sources) {
        const std::optional<asset_id> found = source->find_by_path(assetPath);
        if (found.has_value()) {
            return found;
        }
    }
    return std::nullopt;
}

asset_handle asset_manager::load(const asset_id &id, asset_load_priority priority) {
    CONJURE_ASSERT(core::jobs::is_main_thread(), "asset_manager::load is main-thread only");
    return mState->load(id, priority);
}

asset_load_batch asset_manager::load_batch(std::span<const asset_id> ids, asset_load_priority priority) {
    CONJURE_ASSERT(core::jobs::is_main_thread(), "asset_manager::load_batch is main-thread only");
    std::vector<asset_handle> handles = {};
    handles.reserve(ids.size());
    for (const asset_id &id : ids) {
        handles.push_back(mState->load(id, priority));
    }
    return asset_load_batch(std::move(handles));
}

core::result<asset_handle> asset_manager::load_blocking(const asset_id &id) {
    CONJURE_ASSERT(core::jobs::is_main_thread(), "asset_manager::load_blocking is main-thread only");
    return mState->load_blocking(id);
}

void asset_manager::reload(const asset_id &id) {
    CONJURE_ASSERT(core::jobs::is_main_thread(), "asset_manager::reload is main-thread only");
    mState->reload(id);
}

void asset_manager::retry_failed(const asset_id &id) {
    CONJURE_ASSERT(core::jobs::is_main_thread(), "asset_manager::retry_failed is main-thread only");
    mState->retry_failed(id);
}

void asset_manager::pin(const asset_id &id, bool pinned) {
    CONJURE_ASSERT(core::jobs::is_main_thread(), "asset_manager::pin is main-thread only");
    asset_slot *slot = mState->find_or_create(id);
    if (slot == nullptr) {
        return;
    }
    mState->slots.data(*slot).pinned = pinned;
}

bool asset_manager::is_pinned(const asset_id &id) const noexcept {
    const asset_slot *slot = mState->slots.find(id);
    if (slot == nullptr) {
        return false;
    }
    return mState->slots.data(*slot).pinned;
}

void *asset_manager::fallback_payload(asset_type_id type) noexcept {
    return mState->fallback_payload(type);
}

const core::error *asset_manager::last_error(const asset_id &id) const noexcept {
    const asset_slot *slot = mState->slots.find(id);
    if (slot == nullptr) {
        return nullptr;
    }
    const slot_data &data = mState->slots.data(*slot);
    if (!data.lastError.has_value()) {
        return nullptr;
    }
    return &*data.lastError;
}

asset_state asset_manager::state_of(const asset_id &id) const noexcept {
    const asset_slot *slot = mState->slots.find(id);
    if (slot == nullptr) {
        return asset_state::UNLOADED;
    }
    return slot->state.load(std::memory_order_acquire);
}

std::vector<asset_id> asset_manager::failed_ids(asset_error_code code) const {
    CONJURE_ASSERT(core::jobs::is_main_thread(), "asset_manager::failed_ids is main-thread only");
    // The list holds every slot fail() moved to FAILED; the ones that left FAILED since (retried,
    // loaded) drop out here, so the hot reload controller's per-frame query never walks every slot.
    std::vector<u32> &failedSlots = mState->failedSlots;
    std::erase_if(failedSlots, [this](u32 index) {
        const asset_slot &slot = mState->slots.at(index);
        if (slot.state.load(std::memory_order_acquire) == asset_state::FAILED) {
            return false;
        }
        mState->slots.data(slot).inFailedList = false;
        return true;
    });
    std::sort(failedSlots.begin(), failedSlots.end());

    std::vector<asset_id> ids = {};
    for (const u32 index : failedSlots) {
        const asset_slot &slot = mState->slots.at(index);
        const slot_data &data = mState->slots.data(slot);
        if (data.lastError.has_value() && data.lastError->is(code)) {
            ids.push_back(slot.id);
        }
    }
    return ids;
}

void asset_manager::begin_frame(u64 frameIndex) {
    CONJURE_ASSERT(core::jobs::is_main_thread(), "asset_manager::begin_frame is main-thread only");
    mState->frameIndex = frameIndex;
    mState->inFrame = true;
    mState->blockingWarnedThisFrame = false;
}

void asset_manager::update() {
    CONJURE_ASSERT(core::jobs::is_main_thread(), "asset_manager::update is main-thread only");
    if (mState->shutDown) {
        return;
    }
    mState->update();
}

void asset_manager::end_frame() {
    CONJURE_ASSERT(core::jobs::is_main_thread(), "asset_manager::end_frame is main-thread only");
    mState->inFrame = false;
}

u32 asset_manager::collect() {
    CONJURE_ASSERT(core::jobs::is_main_thread(), "asset_manager::collect is main-thread only");
    return mState->collect();
}

void asset_manager::flush() {
    CONJURE_ASSERT(core::jobs::is_main_thread(), "asset_manager::flush is main-thread only");
    mState->flush();
}

void asset_manager::shutdown() {
    CONJURE_ASSERT(core::jobs::is_main_thread(), "asset_manager::shutdown is main-thread only");
    mState->shutdown();
}

bool asset_manager::is_shut_down() const noexcept {
    return mState->shutDown;
}

asset_subscription asset_manager::subscribe(asset_event_callback callback) {
    CONJURE_ASSERT(core::jobs::is_main_thread(), "asset_manager::subscribe is main-thread only");
    if (!callback) {
        return asset_subscription();
    }
    const u64 id = mState->nextSubscription;
    ++mState->nextSubscription;
    mState->subscribers.push_back(subscriber{id, std::move(callback)});
    return asset_subscription{id};
}

void asset_manager::unsubscribe(asset_subscription subscription) {
    CONJURE_ASSERT(core::jobs::is_main_thread(), "asset_manager::unsubscribe is main-thread only");
    std::erase_if(mState->subscribers, [subscription](const subscriber &listener) {
        return listener.id == subscription.id;
    });
}

asset_manager_stats asset_manager::stats() const noexcept {
    return mState->stats();
}

u64 asset_manager::frame_index() const noexcept {
    return mState->frameIndex;
}

#include "conjure/ecs/scheduler.h"

#include "ecs/scheduler_internal.h"

#include "conjure/ecs/log_category.h"

#include "conjure/core/assert.h"
#include "conjure/core/log.h"

#include <chrono>
#include <format>
#include <functional>
#include <thread>
#include <utility>
#include <vector>

// Scheduler execution (docs/design/01_ecs.md 3.5). Stage construction lives in scheduler_stages.cpp.

using namespace conjure;
using namespace conjure::ecs;

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

scheduler::scheduler(ecs::world &target, core::jobs::job_system *jobs, scheduler_desc desc) :
    mWorld(&target),
    mJobs(jobs),
    mDesc(desc),
    // No system exists yet, so every access set is resolved against the registry as it stands.
    mResolvedComponentCount(target.registry().count()),
    mPhases(std::make_unique<std::array<phase_state, SYSTEM_PHASE_COUNT>>()) {}

scheduler::~scheduler() {
    // Pending commands of removed systems were discarded at removal; the live ones are discarded here
    // without the destructor warning because a scheduler dying mid-frame is a shutdown, not a bug.
    for (std::unique_ptr<system_record> &record : mSystems) {
        if (record != nullptr) {
            record->commands.clear();
            for (command_buffer &grain : record->scratch.grainBuffers) {
                grain.clear();
            }
        }
    }
}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

core::result<system_id> scheduler::add_system(system_desc desc) {
    if (!desc.run) {
        return core::error(core::error_code::INVALID_ARGUMENT, "system_desc has no run function");
    }
    if (desc.phase >= system_phase::COUNT) {
        return core::error(core::error_code::INVALID_ARGUMENT, "system_desc has an invalid phase");
    }
    // The name is the system's identity for after/before edges, timings and the profiler, so a phase
    // holds each name once (01_ecs.md D13): an edge to a name two systems shared would bind to whichever
    // was registered first.
    if (find_system(desc.phase, desc.name).is_valid()) {
        return make_error(
          ecs_error::NAME_CONFLICT,
          std::format("system '{}' already exists in phase {}", desc.name, to_string(desc.phase))
        );
    }
    if (desc.resolveAccess) {
        desc.resolveAccess(mWorld->registry(), desc.reads, desc.writes);
    }

    u32 index = 0;
    if (!mFreeRecords.empty()) {
        index = mFreeRecords.back();
        mFreeRecords.pop_back();
    } else {
        index = static_cast<u32>(mSystems.size());
        mSystems.push_back(std::make_unique<system_record>(*mWorld));
    }
    system_record &record = *mSystems[index];
    record.desc = std::move(desc);
    record.generation = mNextGeneration;
    ++mNextGeneration;
    if (mNextGeneration == 0) {
        mNextGeneration = 1;
    }
    record.registrationOrder = mRegistrationCounter;
    ++mRegistrationCounter;
    record.alive = true;
    record.sinceVersion = 0;
    record.lastMicroseconds = 0;

    // A cycle through the new system is refused before it becomes visible. Unknown names in after/before
    // are not an error (the module may be compiled out), so edge resolution cannot fail; they are logged
    // once, here, for the new system only (finalise and the lazy rebuilds resolve without logging).
    std::vector<std::vector<u32>> predecessors = {};
    resolve_edges(record.desc.phase, predecessors, &record);
    const core::result<std::vector<u32>> order = topological_order(record.desc.phase, predecessors);
    if (!order.has_value()) {
        record.alive = false;
        record.desc = {};
        mFreeRecords.push_back(index);
        return order.get_error();
    }

    mark_dirty(record.desc.phase);
    return system_id{index, record.generation};
}

core::result<void> scheduler::remove_system(system_id id) {
    system_record *record = find_record(id);
    if (record == nullptr) {
        return make_error(ecs_error::SYSTEM_NOT_FOUND, "remove_system with an unknown or stale id");
    }
    const system_phase phase = record->desc.phase;
    record->alive = false;
    // The record waits on the free list for the next add_system; the room its buffers and grain scratch
    // grew to belongs to the removed system's workload, so it goes back to the heap here (a grain buffer's
    // destructor returns its reserved ids as well).
    record->commands.clear();
    record->commands.shrink_to_fit();
    record->scratch.grainBuffers.clear();
    record->scratch.grainBuffers.shrink_to_fit();
    record->scratch.chunks.clear();
    record->scratch.chunks.shrink_to_fit();
    record->desc = {};
    mFreeRecords.push_back(id.index);
    mark_dirty(phase);
    return core::result<void>::success();
}

core::result<void> scheduler::set_enabled(system_id id, bool enabled) {
    system_record *record = find_record(id);
    if (record == nullptr) {
        return make_error(ecs_error::SYSTEM_NOT_FOUND, "set_enabled with an unknown or stale id");
    }
    if (enabled) {
        record->desc.flags = without_flag(record->desc.flags, system_flags::DISABLED);
    } else {
        record->desc.flags = with_flag(record->desc.flags, system_flags::DISABLED);
    }
    mark_dirty(record->desc.phase);
    return core::result<void>::success();
}

core::result<void> scheduler::finalise() {
    refresh_access();
    for (u32 phase = 0; phase < SYSTEM_PHASE_COUNT; ++phase) {
        CONJURE_TRY(build_stages(static_cast<system_phase>(phase)));
    }
    return core::result<void>::success();
}

void scheduler::run_phase(system_phase target, const frame_info &frame) {
    run_phase_filtered(target, frame, system_flags::NONE);
}

void scheduler::run_phase_filtered(system_phase target, const frame_info &frame, system_flags required) {
    if (target >= system_phase::COUNT) {
        CONJURE_ASSERT(false, "run_phase with an invalid phase");
        return;
    }
    if (std::this_thread::get_id() != mWorld->owner_thread()) {
        CONJURE_ASSERT(false, "scheduler::run_phase off the world's owner thread");
        return;
    }
    if (mWorld->is_locked()) {
        CONJURE_ASSERT(false, "scheduler::run_phase while the world is locked");
        return;
    }

    refresh_access();
    phase_state &state = (*mPhases)[static_cast<usize>(target)];
    if (state.dirty) {
        const core::result<void> built = build_stages(target);
        if (!built.has_value()) {
            CONJURE_LOG_WARN(LOG_ECS, "phase {} not run: {}", to_string(target), built.get_error().message());
            return;
        }
    }

    const bool parallelSystems = mJobs != nullptr && mDesc.parallelSystems;
    // The stage scratch lives in the phase state so a warmed-up phase allocates nothing; the frame is
    // reached through a member so the job closure stays small enough for every std::function SBO. The
    // member is saved and restored, not cleared, because an EXCLUSIVE system (owner thread, world
    // unlocked) may run another phase from inside this one: the outer phase's later stages must still
    // find their own frame once the nested run_phase returns. The stage jobs complete on a fence held
    // on this stack frame (like parallel_for's counter) instead of a heap-allocated job_handle; one
    // fence serves every stage because a stage is waited for before the next one is scheduled.
    std::vector<std::function<void()>> &jobs = state.jobScratch;
    std::vector<system_record *> &mainThread = state.mainThreadScratch;
    core::jobs::job_fence stageFence = {};
    const frame_info *previousFrame = mRunningFrame;
    mRunningFrame = &frame;
    usize timingIndex = 0;
    for (const stage_info &stage : state.stages) {
        if (stage.exclusive) {
            system_record *record = find_record(stage.systems.front());
            if (record != nullptr && has_flag(record->desc.flags, required)) {
                run_system(*record, frame);
                state.timings[timingIndex] = system_timing{record->desc.name, record->lastMicroseconds};
                const flush_report report = mWorld->flush(record->commands);
                CONJURE_UNUSED(report);
            }
            ++timingIndex;
            continue;
        }

        jobs.clear();
        mainThread.clear();
        {
            const iteration_scope scope = mWorld->lock_iteration();
            for (const system_id id : stage.systems) {
                system_record *record = find_record(id);
                if (record == nullptr || !has_flag(record->desc.flags, required)) {
                    continue;
                }
                if (has_flag(record->desc.flags, system_flags::MAIN_THREAD) || !parallelSystems) {
                    mainThread.push_back(record);
                    continue;
                }
                jobs.push_back([this, record]() {
                    run_system(*record, *mRunningFrame);
                });
            }
            if (!jobs.empty()) {
                mJobs->schedule(std::span<const std::function<void()>>(jobs), stageFence);
            }
            for (system_record *record : mainThread) {
                run_system(*record, frame);
            }
            if (!jobs.empty()) {
                mJobs->wait(stageFence);
            }
        }

        for (const system_id id : stage.systems) {
            system_record *record = find_record(id);
            if (record != nullptr && has_flag(record->desc.flags, required)) {
                state.timings[timingIndex] = system_timing{record->desc.name, record->lastMicroseconds};
                const flush_report report = mWorld->flush(record->commands);
                CONJURE_UNUSED(report);
            }
            ++timingIndex;
        }
    }
    mRunningFrame = previousFrame;
}

std::span<const stage_info> scheduler::stages(system_phase target) {
    if (target >= system_phase::COUNT) {
        return {};
    }
    refresh_access();
    phase_state &state = (*mPhases)[static_cast<usize>(target)];
    if (state.dirty) {
        const core::result<void> built = build_stages(target);
        if (!built.has_value()) {
            return {};
        }
    }
    return state.stages;
}

std::span<const system_timing> scheduler::last_timings(system_phase target) const noexcept {
    if (target >= system_phase::COUNT) {
        return {};
    }
    return (*mPhases)[static_cast<usize>(target)].timings;
}

const system_desc *scheduler::system(system_id id) const noexcept {
    const system_record *record = find_record(id);
    return record == nullptr ? nullptr : &record->desc;
}

system_id scheduler::find_system(system_phase target, core::string_id name) const noexcept {
    if (name.is_null() || target >= system_phase::COUNT) {
        return {};
    }
    for (u32 index = 0; index < mSystems.size(); ++index) {
        const system_record *record = mSystems[index].get();
        if (record != nullptr && record->alive && record->desc.phase == target && record->desc.name == name) {
            return system_id{index, record->generation};
        }
    }
    return {};
}

u32 scheduler::system_count() const noexcept {
    u32 total = 0;
    for (const std::unique_ptr<system_record> &record : mSystems) {
        if (record != nullptr && record->alive) {
            ++total;
        }
    }
    return total;
}


/*===========================================================================*/
/*===========================================================================*/
// Private methods

scheduler::system_record *scheduler::find_record(system_id id) noexcept {
    if (!id.is_valid() || id.index >= mSystems.size()) {
        return nullptr;
    }
    system_record *record = mSystems[id.index].get();
    if (record == nullptr || !record->alive || record->generation != id.generation) {
        return nullptr;
    }
    return record;
}

const scheduler::system_record *scheduler::find_record(system_id id) const noexcept {
    if (!id.is_valid() || id.index >= mSystems.size()) {
        return nullptr;
    }
    const system_record *record = mSystems[id.index].get();
    if (record == nullptr || !record->alive || record->generation != id.generation) {
        return nullptr;
    }
    return record;
}

void scheduler::refresh_access() {
    // Component ids are append-only and never reused (01_ecs.md D3), so the count alone says whether a
    // type some system named could have appeared since the last pass. It is read before the hooks run: a
    // type another thread registers during the pass leaves the registry ahead of the stored count, and
    // the next call resolves again.
    const component_registry &registry = mWorld->registry();
    const u32 registered = registry.count();
    if (registered == mResolvedComponentCount) {
        return;
    }
    // A locked world may be a stage in progress, whose systems read their masks on other threads (the
    // Debug access checks), so finalise() or stages() called from inside a system leaves the pass to the
    // next call that finds the world unlocked. run_phase never gets here locked.
    if (mWorld->is_locked()) {
        return;
    }
    for (const std::unique_ptr<system_record> &record : mSystems) {
        if (record == nullptr || !record->alive || !record->desc.resolveAccess) {
            continue;
        }
        system_desc &desc = record->desc;
        const component_mask reads = desc.reads;
        const component_mask writes = desc.writes;
        desc.resolveAccess(registry, desc.reads, desc.writes);
        if (desc.reads == reads && desc.writes == writes) {
            continue;
        }
        // The stages of the phase were packed from the smaller set, which let this system share a stage
        // with one that touches the component it has just gained (01_ecs.md D15). Only the flag is set:
        // a nested run_phase may be here while an outer one walks the stages of that phase.
        CONJURE_LOG_DEBUG(
          LOG_ECS,
          "system '{}' gained access to newly registered components; phase {} is rebuilt",
          desc.name,
          to_string(desc.phase)
        );
        mark_dirty(desc.phase);
    }
    mResolvedComponentCount = registered;
}

void scheduler::run_system(system_record &record, const frame_info &frame) {
    const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    const u64 runVersion = mWorld->bump_version();
    system_context context(
      *mWorld,
      frame,
      record.sinceVersion,
      runVersion,
      record.commands,
      mJobs,
      record.desc,
      record.scratch,
      mDesc.parallelChunks,
      mDesc.minEntitiesPerJob
    );
    record.desc.run(context);
    record.sinceVersion = runVersion;
    const std::chrono::steady_clock::duration elapsed = std::chrono::steady_clock::now() - start;
    record.lastMicroseconds = static_cast<u64>(std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count());
}

void scheduler::mark_dirty(system_phase target) {
    (*mPhases)[static_cast<usize>(target)].dirty = true;
}

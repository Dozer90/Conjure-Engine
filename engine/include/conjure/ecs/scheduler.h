#pragma once

#include "conjure/ecs/command_buffer.h"
#include "conjure/ecs/component.h"
#include "conjure/ecs/ecs_error.h"
#include "conjure/ecs/system.h"
#include "conjure/ecs/system_phase.h"
#include "conjure/ecs/world.h"

#include "conjure/core/jobs/job_system.h"
#include "conjure/core/result.h"
#include "conjure/core/string_id.h"
#include "conjure/core/types.h"

#include <array>
#include <memory>
#include <span>
#include <vector>

// The scheduler (docs/design/01_ecs.md 3.5, 4.7, R2).
//
//   scheduler scheduler(world, &jobs);
//   CONJURE_REQUIRE_OK(scheduler.add_system(std::move(desc)));
//   CONJURE_REQUIRE_OK(scheduler.finalise());
//   scheduler.run_phase(system_phase::UPDATE, frame);
//
// Systems are packed per phase into conflict-free stages from their declared read/write masks and
// after/before edges (Kahn topological sort with registration order as the tiebreak, then greedy
// append-only packing). A stage runs its systems as jobs over a locked world, main-thread systems last
// on the calling thread, exclusive systems alone with an unlocked world; every system's command buffer
// is flushed in stage order when the stage ends. A null job system (or parallelSystems = false) runs
// the same stages inline, so headless tests and the engine share one code path.

namespace conjure::ecs {

    /// <summary>
    /// Handle of a registered system.
    /// </summary>
    struct system_id {
        u32 index = 0;
        u32 generation = 0;

        /// <summary>
        /// True for a handle add_system returned (generation 0 is never issued).
        /// </summary>
        [[nodiscard]] constexpr bool is_valid() const noexcept {
            return generation != 0;
        }

        [[nodiscard]] constexpr bool operator==(const system_id &other) const noexcept = default;
    };

    /// <summary>
    /// Wall-clock cost of one system in the last run of its phase.
    /// </summary>
    struct system_timing {
        core::string_id name = {};
        u64 microseconds = 0;
    };

    /// <summary>
    /// One stage of a phase: systems that run concurrently.
    /// </summary>
    struct stage_info {
        std::vector<system_id> systems = {};
        bool exclusive = false;
    };

    /// <summary>
    /// Parallelism switches. Both false reproduces the engine's stage semantics strictly serially.
    /// </summary>
    struct scheduler_desc {
        bool parallelSystems = true;           // run a stage's systems as jobs
        bool parallelChunks = true;            // parallel_for_chunks fans out over the job system
        u32 minEntitiesPerJob = 1024;          // grain of parallel_for_chunks in entities
    };

    class scheduler {
        private:
            struct system_record;
            struct phase_state;

            ecs::world *mWorld = nullptr;
            core::jobs::job_system *mJobs = nullptr;
            scheduler_desc mDesc = {};
            std::vector<std::unique_ptr<system_record>> mSystems = {};
            std::vector<u32> mFreeRecords = {};
            u32 mNextGeneration = 1;
            u32 mRegistrationCounter = 0;
            std::unique_ptr<std::array<phase_state, SYSTEM_PHASE_COUNT>> mPhases = {};
            const frame_info *mRunningFrame = nullptr;     // the frame of the run_phase in progress

        public:
            scheduler(ecs::world &target, core::jobs::job_system *jobs, scheduler_desc desc = {});
            ~scheduler();

            scheduler(const scheduler &) = delete;
            scheduler &operator=(const scheduler &) = delete;
            scheduler(scheduler &&) = delete;
            scheduler &operator=(scheduler &&) = delete;

        public:
            /// <summary>
            /// Adds a system; its phase's stages are rebuilt lazily. SYSTEM_CYCLE (and the system is not
            /// added) when its after/before edges close a cycle; unknown names in those lists are
            /// ignored with one DEBUG log each. Owner thread.
            /// </summary>
            [[nodiscard]] core::result<system_id> add_system(system_desc desc);

            /// <summary>
            /// Removes a system (its pending commands are discarded). SYSTEM_NOT_FOUND for a stale id.
            /// </summary>
            [[nodiscard]] core::result<void> remove_system(system_id id);

            /// <summary>
            /// Enables or disables a system (DISABLED flag). SYSTEM_NOT_FOUND for a stale id.
            /// </summary>
            [[nodiscard]] core::result<void> set_enabled(system_id id, bool enabled);

            /// <summary>
            /// Builds every phase's stages now and reports SYSTEM_CYCLE and Debug access conflicts (R2).
            /// Optional: run_phase builds lazily; the engine calls it at startup step 18 and after every
            /// module load so configuration errors surface before the first frame instead of inside it.
            /// </summary>
            [[nodiscard]] core::result<void> finalise();

            /// <summary>
            /// Runs every enabled system of the phase, stage by stage, flushing command buffers at each
            /// stage boundary. Owner thread, world unlocked. The per-stage bookkeeping lives in the phase
            /// (kept at capacity), so a phase that has run once allocates nothing of its own afterwards;
            /// with parallelSystems and a job system, job_system::schedule still allocates its counter.
            /// </summary>
            void run_phase(system_phase target, const frame_info &frame);

            /// <summary>
            /// Runs only the systems of the phase whose flags contain every bit of required (R2).
            /// </summary>
            void run_phase_filtered(system_phase target, const frame_info &frame, system_flags required);

            /// <summary>
            /// The stages of a phase (built if dirty).
            /// </summary>
            [[nodiscard]] std::span<const stage_info> stages(system_phase target);

            /// <summary>
            /// Per-system timings of the last run of a phase, in stage order.
            /// </summary>
            [[nodiscard]] std::span<const system_timing> last_timings(system_phase target) const noexcept;

            /// <summary>
            /// The description of a system; nullptr for a stale id.
            /// </summary>
            [[nodiscard]] const system_desc *system(system_id id) const noexcept;

            /// <summary>
            /// Registered systems.
            /// </summary>
            [[nodiscard]] u32 system_count() const noexcept;

            /// <summary>
            /// The parallelism switches in effect.
            /// </summary>
            [[nodiscard]] const scheduler_desc &desc() const noexcept {
                return mDesc;
            }

        private:
            [[nodiscard]] system_record *find_record(system_id id) noexcept;
            [[nodiscard]] const system_record *find_record(system_id id) const noexcept;
            [[nodiscard]] core::result<void> build_stages(system_phase target);
            void resolve_edges(
              system_phase target,
              std::vector<std::vector<u32>> &predecessors,
              bool logUnknown
            );
            [[nodiscard]] core::result<std::vector<u32>> topological_order(
              system_phase target,
              const std::vector<std::vector<u32>> &predecessors
            );
            void run_system(system_record &record, const frame_info &frame);
            void mark_dirty(system_phase target);
    };
}

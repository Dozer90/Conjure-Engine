#pragma once

#include "conjure/ecs/archetype.h"
#include "conjure/ecs/chunk_view.h"
#include "conjure/ecs/command_buffer.h"
#include "conjure/ecs/component.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/ecs_error.h"
#include "conjure/ecs/query.h"
#include "conjure/ecs/system_phase.h"
#include "conjure/ecs/view.h"
#include "conjure/ecs/world.h"

#include "conjure/core/api.h"
#include "conjure/core/assert.h"
#include "conjure/core/containers/small_vector.h"
#include "conjure/core/enum_flags.h"
#include "conjure/core/jobs/job_system.h"
#include "conjure/core/string_id.h"
#include "conjure/core/types.h"

#include <functional>
#include <span>
#include <utility>
#include <vector>

// Systems (docs/design/01_ecs.md 3.5, 4.7).
//
//   CONJURE_TRY(scheduler.add_system(make_system<const velocity, position>(
//       core::string_id("game.integrate"),
//       system_phase::UPDATE,
//       [](system_context &context) {
//           context.view<const velocity, position>().for_each([&](entity, const velocity &v, position &p) {
//               p.x += v.x * context.delta_time();
//           });
//       })));
//
// A system declares the components it reads and writes; the scheduler packs systems with disjoint
// access into one stage and runs a stage's systems as jobs over a locked world. Inside a system,
// structural changes go through context.commands() (flushed at the end of the stage) and chunk-level
// parallelism through context.parallel_for_chunks(), which merges per-grain command buffers in grain
// order so command application is deterministic whatever the worker count.

namespace conjure::ecs {

    class scheduler;

    /// <summary>
    /// Execution properties of a system. Neither thread bit set means PARALLEL: the system runs as a
    /// job alongside the other systems of its stage.
    /// </summary>
    enum class system_flags : u32 {
        NONE = 0,
        MAIN_THREAD = 1 << 0,            // runs on the run_phase caller thread, last in its stage
        EXCLUSIVE = 1 << 1,              // alone in its stage, receives an unlocked world
        DISABLED = 1 << 2,
        RUNS_WHILE_PAUSED = 1 << 3       // selected by run_phase_filtered while the engine is PAUSED (R2)
    };
    CONJURE_ENUM_FLAGS(system_flags);

    class system_context;

    /// <summary>
    /// Everything the scheduler needs to run one system.
    /// </summary>
    struct system_desc {
        core::string_id name = {};
        system_phase phase = system_phase::UPDATE;
        system_flags flags = system_flags::NONE;
        component_mask reads = {};
        component_mask writes = {};
        core::small_vector<core::string_id, 4> after = {};      // names of systems that must run earlier
        core::small_vector<core::string_id, 4> before = {};     // names of systems that must run later
        std::function<void(system_context &context)> run = {};

        /// <summary>
        /// Optional: resolves component types into reads/writes once the registry is known
        /// (make_system fills this so a system_desc needs no registry at construction). Called by
        /// scheduler::add_system and merged into reads/writes.
        /// </summary>
        std::function<void(const component_registry &registry, component_mask &reads, component_mask &writes)>
          resolveAccess = {};
    };

    /// <summary>
    /// Per-system scratch storage reused across runs: grain command buffers and the chunk list of
    /// parallel_for_chunks. Owned by the scheduler's system record (or by the caller of a standalone
    /// system_context).
    /// </summary>
    struct system_scratch {
        /// <summary>
        /// One flattened chunk of a parallel_for_chunks dispatch.
        /// </summary>
        struct chunk_ref {
            archetype *owner = nullptr;
            u32 chunkIndex = 0;
        };

        std::vector<command_buffer> grainBuffers = {};
        std::vector<chunk_ref> chunks = {};
    };

    /// <summary>
    /// The arguments of one system run.
    /// </summary>
    class CONJURE_API system_context {
        private:
            ecs::world *mWorld = nullptr;
            const frame_info *mFrame = nullptr;
            u64 mSinceVersion = 0;
            u64 mRunVersion = 0;
            command_buffer *mCommands = nullptr;
            core::jobs::job_system *mJobs = nullptr;
            const system_desc *mDesc = nullptr;
            system_scratch *mScratch = nullptr;
            bool mParallelChunks = true;
            u32 mMinEntitiesPerJob = 1024;
            u32 mLastGrainCount = 0;

        public:
            /// <summary>
            /// Builds a context. 'sinceVersion' is the version observed at the start of the previous
            /// run, 'runVersion' the version chunk views created through this context stamp.
            /// </summary>
            system_context(
              ecs::world &target,
              const frame_info &frame,
              u64 sinceVersion,
              u64 runVersion,
              command_buffer &commands,
              core::jobs::job_system *jobs,
              const system_desc &desc,
              system_scratch &scratch,
              bool parallelChunks,
              u32 minEntitiesPerJob
            ) noexcept;

            ~system_context();

            system_context(const system_context &) = delete;
            system_context &operator=(const system_context &) = delete;

        public:
            /// <summary>
            /// The world the system runs on.
            /// </summary>
            [[nodiscard]] ecs::world &world() noexcept {
                return *mWorld;
            }

            [[nodiscard]] const ecs::world &world() const noexcept {
                return *mWorld;
            }

            /// <summary>
            /// frame().deltaTime.
            /// </summary>
            [[nodiscard]] f32 delta_time() const noexcept {
                return mFrame->deltaTime;
            }

            /// <summary>
            /// The full per-frame timing block (R2).
            /// </summary>
            [[nodiscard]] const frame_info &frame() const noexcept {
                return *mFrame;
            }

            /// <summary>
            /// The world version at the start of this system's previous run: a changed&lt;T&gt; filter
            /// against it sees every write since, but never the system's own previous writes made through
            /// view() or parallel_for_chunks (those stamp run_version()). Writes through world().get&lt;T&gt;(),
            /// set() or component_ref stamp the current world version, which a sibling's run or a structural
            /// call may have bumped past run_version(), so the next run can see those.
            /// </summary>
            [[nodiscard]] u64 since_version() const noexcept {
                return mSinceVersion;
            }

            /// <summary>
            /// The version this run stamps.
            /// </summary>
            [[nodiscard]] u64 run_version() const noexcept {
                return mRunVersion;
            }

            /// <summary>
            /// The system's command buffer, flushed when the stage ends. System thread only; jobs of
            /// parallel_for_chunks receive their own grain buffer.
            /// </summary>
            [[nodiscard]] command_buffer &commands() noexcept {
                return *mCommands;
            }

            /// <summary>
            /// The job system, or nullptr when everything runs inline.
            /// </summary>
            [[nodiscard]] core::jobs::job_system *jobs() noexcept {
                return mJobs;
            }

            /// <summary>
            /// The running system's description.
            /// </summary>
            [[nodiscard]] const system_desc &desc() const noexcept {
                return *mDesc;
            }

            /// <summary>
            /// A typed view validated against the declared access in Debug and Development: every
            /// required or optional component must be in reads | writes and every written one in writes.
            /// Violations assert naming the system. The chunk views the result hands out carry this
            /// context, so a non-const span of an undeclared component asserts too (3.5); the view must
            /// not outlive the system run. The checks compile out in Distribution; the view is bound to
            /// this context in every configuration, so its writes stamp run_version().
            /// </summary>
            template <typename... ComponentTypes>
            [[nodiscard]] ecs::view<ComponentTypes...> view() {
                ecs::view<ComponentTypes...> result(*mWorld);
#if CONJURE_ASSERTS_ENABLED
                validate_access(result.access());
#endif
                result.underlying().bind_context(this);
                return result;
            }

            /// <summary>
            /// Runs callable(chunk_view &, command_buffer &) over the chunks matched by 'target'.
            /// Chunks are grouped into grains of max(1, minEntitiesPerJob / capacity) chunks, each grain
            /// runs as one job (inline without a job system or with chunk parallelism disabled) with its
            /// own command buffer, and the grain buffers are appended to commands() in grain order after
            /// the join. The callable must not touch state shared between grains without synchronisation.
            /// </summary>
            template <typename Callable>
            void parallel_for_chunks(query &target, Callable &&callable) {
                using callable_type = std::remove_reference_t<Callable>;
                callable_type *body = &callable;
                dispatch_chunks(
                  target,
                  [](void *context, chunk_view &view, command_buffer &commands) {
                      (*static_cast<callable_type *>(context))(view, commands);
                  },
                  body
                );
            }

            /// <summary>
            /// Number of grains the last parallel_for_chunks dispatch used (for tests and profiling).
            /// </summary>
            [[nodiscard]] u32 last_grain_count() const noexcept {
                return mLastGrainCount;
            }

        private:
            void validate_access(const view_access &access) const;
            void dispatch_chunks(
              query &target,
              void (*visit)(void *context, chunk_view &view, command_buffer &commands),
              void *context
            );
    };

    namespace detail {
        template <typename PackType>
        void collect_pack_access(const component_registry &registry, component_mask &reads, component_mask &writes) {
            using traits = view_arg_traits<PackType>;
            const component_id id = registry.id_of<typename traits::component_type>();
            if (!id.is_valid()) {
                return;
            }
            if constexpr (traits::IS_CONST) {
                reads.set(id);
            } else {
                writes.set(id);
            }
        }
    }

    /// <summary>
    /// A system_desc whose reads/writes derive from the pack (const T read, T write, maybe&lt;T&gt; by
    /// constness) so the common one-view system declares nothing by hand. The pack is resolved against
    /// the world's registry when the system is added.
    /// </summary>
    template <typename... ComponentTypes>
    [[nodiscard]] system_desc make_system(
      core::string_id name,
      system_phase target,
      std::function<void(system_context &context)> run
    ) {
        system_desc desc = {};
        desc.name = name;
        desc.phase = target;
        desc.run = std::move(run);
        desc.resolveAccess = [](const component_registry &registry, component_mask &reads, component_mask &writes) {
            (detail::collect_pack_access<ComponentTypes>(registry, reads, writes), ...);
        };
        return desc;
    }

    template <typename... ComponentTypes>
    template <typename Callable>
    void view<ComponentTypes...>::parallel_for_each_chunk(system_context &context, Callable &&callable) {
        context.parallel_for_chunks(mQuery, std::forward<Callable>(callable));
    }
}

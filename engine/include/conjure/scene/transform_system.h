#pragma once

#include "conjure/scene/components.h"
#include "conjure/scene/scene.h"

#include "conjure/core/api.h"
#include "conjure/core/math/transform.h"
#include "conjure/core/types.h"
#include "conjure/ecs/entity.h"
#include "conjure/ecs/query.h"
#include "conjure/ecs/system.h"

#include <atomic>
#include <vector>

// Transform propagation (docs/design/13_scene.md 3.4, R3): the body of scene.propagate_transforms. One
// parallel_for_chunks per hierarchy level (roots first), so the parent of every entity in a level has
// been finished by the previous level's join. A chunk is recomputed when its local_transform column
// changed since the previous run, when it carries transform_dirty, when a parent was recomputed this run
// (the transform_state::lastUpdateRun stamp) or when an interpolated pose is involved. world_transform
// and render_world_transform are written in the same visit; dirty tags are removed through the grain
// command buffers. Allocation-free once the queries have seen every archetype.

namespace conjure::scene {

    /// <summary>
    /// Counters of the last run.
    /// </summary>
    struct propagation_stats {
        u32 entitiesVisited = 0;      // rows of chunks that needed work
        u32 worldRecomputed = 0;      // world matrices written
        u32 renderRecomputed = 0;     // render matrices written
        u32 chunksVisited = 0;        // chunks with at least one row recomputed
        u32 chunksSkipped = 0;        // chunks left untouched (no stamp)
        u8 levels = 0;                // hierarchy levels processed (roots count as one)
    };

    class CONJURE_API transform_system {
        public:
            /// <summary>
            /// Registered system name.
            /// </summary>
            static constexpr std::string_view SYSTEM_NAME = "scene.propagate_transforms";

        private:
            /// <summary>
            /// Relaxed counters the chunk jobs add to once per chunk; copied into mStats after the run.
            /// </summary>
            struct run_counters {
                std::atomic<u32> entitiesVisited = 0;
                std::atomic<u32> worldRecomputed = 0;
                std::atomic<u32> renderRecomputed = 0;
                std::atomic<u32> chunksVisited = 0;
                std::atomic<u32> chunksSkipped = 0;
            };

            scene *mScene = nullptr;
            ecs::query mRootQuery;                             // no default: built from the world in the constructor
            std::vector<ecs::query> mLevelQueries = {};        // index d - 1 holds level d
            run_counters mCounters = {};
            propagation_stats mStats = {};
            u32 mRunCounter = 0;
            bool mInterpolationEnabled = true;

        public:
            explicit transform_system(scene &target);
            ~transform_system();

            transform_system(const transform_system &) = delete;
            transform_system &operator=(const transform_system &) = delete;

        public:
            /// <summary>
            /// Runs the propagation pass (the body of scene.propagate_transforms).
            /// </summary>
            void run(ecs::system_context &context);

            /// <summary>
            /// When disabled, interpolated entities render their current pose (alpha 1).
            /// </summary>
            void set_interpolation_enabled(bool enabled) noexcept {
                mInterpolationEnabled = enabled;
            }

            /// <summary>
            /// Whether interpolated entities render the lerp of their poses (true by default).
            /// </summary>
            [[nodiscard]] bool interpolation_enabled() const noexcept {
                return mInterpolationEnabled;
            }

            /// <summary>
            /// Counters of the last run.
            /// </summary>
            [[nodiscard]] const propagation_stats &last_stats() const noexcept {
                return mStats;
            }

            /// <summary>
            /// Number of runs so far.
            /// </summary>
            [[nodiscard]] u32 run_count() const noexcept {
                return mRunCounter;
            }

            /// <summary>
            /// The system description: name, TRANSFORM phase, RUNS_WHILE_PAUSED, reads (local_transform,
            /// parent, interpolated_transform, transform_dirty) and writes (world_transform,
            /// render_world_transform, transform_state); run() bound to this instance.
            /// </summary>
            [[nodiscard]] ecs::system_desc describe();

        private:
            void process_chunk(ecs::chunk_view &chunk, ecs::command_buffer &commands, u32 runId, u64 since, f32 alpha);
    };

    /// <summary>
    /// Exact world transform of an entity from a fresh ancestor walk. An entity or ancestor without
    /// local_transform contributes the identity and the walk continues upwards, so a transformless
    /// entity yields its nearest transformed ancestor's world (the identity when there is none).
    /// </summary>
    [[nodiscard]] CONJURE_API math::transform compute_world_transform(const scene &target, ecs::entity entity);
}

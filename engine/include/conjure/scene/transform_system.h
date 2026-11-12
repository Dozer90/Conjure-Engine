#pragma once

#include "conjure/scene/components.h"
#include "conjure/scene/scene.h"

#include "conjure/core/api.h"
#include "conjure/core/math/transform.h"
#include "conjure/core/types.h"
#include "conjure/ecs/chunk_view.h"
#include "conjure/ecs/entity.h"
#include "conjure/ecs/query.h"
#include "conjure/ecs/system.h"

#include <atomic>
#include <vector>

// Transform propagation (docs/design/13_scene.md 3.4, R3): the body of scene.propagate_transforms. One
// job dispatch per hierarchy level (roots first), so the parent of every entity in a level has been
// finished by the previous level's join. A chunk is recomputed as a whole when its local_transform
// column changed since the previous run, when it carries transform_dirty or when it holds interpolated
// poses; otherwise only its rows marked transform_state::pending are recomputed, the mark a parent's
// pass left when it recomputed its own world matrix (the transform_state::lastUpdateRun stamp), sits in
// an interpolated subtree or changed flags. Clean rows are never looked up: a level scans the state
// column of its chunks, collects the chunks with work and splits them into jobs by dirty rows.
// world_transform and render_world_transform are written in the same visit; dirty tags are removed
// through the system's command buffer after the level join. Allocation-free once the queries have seen
// every archetype and the level scratch has seen its largest level.

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
            /// Relaxed counters each job range adds its local tally to once (chunksSkipped comes from the
            /// serial collect step); copied into mStats after the run.
            /// </summary>
            struct run_counters {
                std::atomic<u32> entitiesVisited = 0;
                std::atomic<u32> worldRecomputed = 0;
                std::atomic<u32> renderRecomputed = 0;
                std::atomic<u32> chunksVisited = 0;
                std::atomic<u32> chunksSkipped = 0;
            };

            /// <summary>
            /// One job range's counts, kept on its own stack so the shared counters' cache line is written
            /// once per range instead of four times per chunk.
            /// </summary>
            struct chunk_tally {
                u32 visited = 0;
                u32 worldWrites = 0;
                u32 renderWrites = 0;
                u32 chunks = 0;
            };

            /// <summary>
            /// One chunk of the level being processed that has rows to recompute.
            /// </summary>
            struct level_chunk {
                ecs::chunk_view view = {};
                u32 rows = 0;                 // rows the pass visits: count() when whole, pending rows otherwise
                bool whole = false;           // every row: the local column changed, the chunk is dirty or has poses
                bool recomputeAll = false;    // every row's world matrix: local column changed or chunk dirty
                bool removeDirty = false;     // the chunk carries transform_dirty
            };

            scene *mScene = nullptr;
            ecs::query mRootQuery;                             // no default: built from the world in the constructor
            std::vector<ecs::query> mLevelQueries = {};        // index d - 1 holds level d
            std::vector<level_chunk> mLevelChunks = {};        // scratch: the current level's chunks with work
            std::vector<u32> mJobBounds = {};                  // scratch: first chunk of every job plus a sentinel
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
            /// parent, children, interpolated_transform, transform_dirty) and writes (world_transform,
            /// render_world_transform, transform_state); run() bound to this instance. children is read
            /// to mark a recomputed row's children pending, so the scheduler keeps a children writer out
            /// of the pass's stage.
            /// </summary>
            [[nodiscard]] ecs::system_desc describe();

        private:
            void run_level(ecs::query &level, ecs::system_context &context, u32 runId, u64 since, f32 alpha);
            void collect_chunk(ecs::chunk_view &chunk, u64 since);
            void process_chunk(const level_chunk &item, u32 runId, f32 alpha, chunk_tally &tally);
            void mark_children_pending(const children &list);
    };

    /// <summary>
    /// Exact world transform of an entity from a fresh ancestor walk. An entity or ancestor without
    /// local_transform contributes the identity and the walk continues upwards, so a transformless
    /// entity yields its nearest transformed ancestor's world (the identity when there is none).
    /// </summary>
    [[nodiscard]] CONJURE_API math::transform compute_world_transform(const scene &target, ecs::entity entity);
}

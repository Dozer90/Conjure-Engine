#include "conjure/scene/transform_system.h"

#include "conjure/core/jobs/job_system.h"
#include "conjure/core/math/transform.h"
#include "conjure/ecs/chunk_view.h"
#include "conjure/ecs/command_buffer.h"
#include "conjure/ecs/world.h"

#include <array>
#include <atomic>
#include <functional>

// The propagation pass (docs/design/13_scene.md 3.4). Every level runs in three steps on the system
// thread: collect (one pass over the level's chunks that keeps the chunks with work and counts their
// rows), dispatch (the collected chunks split into jobs of ROWS_PER_JOB dirty rows; process_chunk runs on
// job threads, reads its own chunk's columns, reads the parents' columns through const random access
// (the parents' level finished before this one started), writes rows of its own chunk and marks the
// children it invalidated as pending in the next level's chunks) and the dirty-tag removal through the
// system's command buffer after the join. A row of a clean chunk is never looked up: the collect step
// finds the pending rows with a sequential scan of the transform_state column.

namespace conjure::scene {

namespace {

    /// <summary>
    /// Dirty rows a job of the level dispatch aims for. Small enough to spread one busy level of a
    /// 10,000-entity hierarchy over 16 workers, large enough that the per-job dispatch cost stays
    /// below the row work.
    /// </summary>
    constexpr u32 ROWS_PER_JOB = 128;

    /// <summary>
    /// A level with fewer dirty rows than this runs inline: waking the workers and joining them costs
    /// more than the rows (about 30 ns each). The limit is deliberately not scaled by the worker count:
    /// back-to-back dispatches find the workers still spinning from the previous level, so the wake-up
    /// is paid once per run, while a long inline level in between lets them sleep and the next dispatch
    /// pays it again. Measured (Development, 16 workers, tests/benchmarks/README.md scenarios): a limit of
    /// 256 rows per worker (4,096) moved the sparse scenario from 0.156 ms to 0.202 ms and the dense one
    /// from 0.224 ms to 0.330 ms; with 2 workers both limits coincide and the figures were identical.
    /// </summary>
    constexpr u32 INLINE_ROW_LIMIT = 512;

    /// <summary>
    /// What a chunk visit knows about a row's parent.
    /// </summary>
    struct parent_state {
        const world_transform *world = nullptr;
        const render_world_transform *render = nullptr;
        const transform_state *state = nullptr;
    };

    /// <summary>
    /// The nearest ancestor that carries a world transform (a transformless ancestor contributes the
    /// identity, S5); every ancestor sits at a shallower level, so its pass has finished.
    /// </summary>
    [[nodiscard]] parent_state lookup_parent(
      const ecs::world &world,
      const scene_component_ids &ids,
      ecs::entity parentEntity
    ) noexcept {
        parent_state result = {};
        ecs::entity current = parentEntity;
        for (u32 step = 0; step < MAX_HIERARCHY_DEPTH && !current.is_null(); ++step) {
            result.world = static_cast<const world_transform *>(world.get_raw(current, ids.worldTransform));
            if (result.world != nullptr) {
                result.render =
                  static_cast<const render_world_transform *>(world.get_raw(current, ids.renderWorldTransform));
                result.state = static_cast<const transform_state *>(world.get_raw(current, ids.transformState));
                return result;
            }
            const parent *link = static_cast<const parent *>(world.get_raw(current, ids.parent));
            current = link == nullptr ? ecs::NULL_ENTITY : link->value;
        }
        return result;
    }

    [[nodiscard]] bool parent_in_interpolated_subtree(const parent_state &parentData) noexcept {
        return parentData.state != nullptr
          && has_flag(parentData.state->flags, transform_state_flags::INTERPOLATED_SUBTREE);
    }

    /// <summary>
    /// The arguments of one level's jobs, referenced (not copied) by the parallel_for body.
    /// </summary>
    struct level_dispatch {
        transform_system *system = nullptr;
        u32 runId = 0;
        f32 alpha = 1.0f;
    };
}

void transform_system::run(ecs::system_context &context) {
    ++mRunCounter;
    if (mRunCounter == 0) {
        ++mRunCounter;
    }
    const u32 runId = mRunCounter;
    const u64 since = context.since_version();
    const f32 alpha = mInterpolationEnabled ? context.frame().interpolationAlpha : 1.0f;

    mCounters.entitiesVisited.store(0, std::memory_order_relaxed);
    mCounters.worldRecomputed.store(0, std::memory_order_relaxed);
    mCounters.renderRecomputed.store(0, std::memory_order_relaxed);
    mCounters.chunksVisited.store(0, std::memory_order_relaxed);
    mCounters.chunksSkipped.store(0, std::memory_order_relaxed);

    run_level(mRootQuery, context, runId, since, alpha);
    const u8 deepest = mScene->max_depth_in_use();
    for (u8 level = 1; level <= deepest && level < MAX_HIERARCHY_DEPTH; ++level) {
        run_level(mLevelQueries[static_cast<usize>(level) - 1], context, runId, since, alpha);
    }

    mStats.entitiesVisited = mCounters.entitiesVisited.load(std::memory_order_relaxed);
    mStats.worldRecomputed = mCounters.worldRecomputed.load(std::memory_order_relaxed);
    mStats.renderRecomputed = mCounters.renderRecomputed.load(std::memory_order_relaxed);
    mStats.chunksVisited = mCounters.chunksVisited.load(std::memory_order_relaxed);
    mStats.chunksSkipped = mCounters.chunksSkipped.load(std::memory_order_relaxed);
    mStats.levels = static_cast<u8>(deepest + 1);
}

void transform_system::run_level(ecs::query &level, ecs::system_context &context, u32 runId, u64 since, f32 alpha) {
    // Collect: the chunks with work and their dirty row counts. The views stamp the world version the
    // iteration started with, which is this run's version (the scheduler bumps it once per system run).
    mLevelChunks.clear();
    level.bind_context(&context);
    level.for_each_chunk([this, since](ecs::chunk_view &chunk) {
        collect_chunk(chunk, since);
    });
    if (mLevelChunks.empty()) {
        return;
    }

    // Split: consecutive chunks until a job holds ROWS_PER_JOB dirty rows (a single busy chunk is one job).
    mJobBounds.clear();
    u32 accumulated = 0;
    u32 totalRows = 0;
    for (u32 index = 0; index < mLevelChunks.size(); ++index) {
        if (accumulated == 0) {
            mJobBounds.push_back(index);
        }
        accumulated += mLevelChunks[index].rows;
        totalRows += mLevelChunks[index].rows;
        if (accumulated >= ROWS_PER_JOB) {
            accumulated = 0;
        }
    }
    mJobBounds.push_back(static_cast<u32>(mLevelChunks.size()));
    const usize jobCount = mJobBounds.size() - 1;

    // Dispatch: parents are read through const random access while the world stays locked; a job
    // writes only rows of its own chunks and the pending marks of their children (the next level).
    level_dispatch dispatch = {};
    dispatch.system = this;
    dispatch.runId = runId;
    dispatch.alpha = alpha;
    const std::function<void(usize, usize)> body = [&dispatch](usize begin, usize end) {
        transform_system &self = *dispatch.system;
        for (usize job = begin; job < end; ++job) {
            const u32 first = self.mJobBounds[job];
            const u32 last = self.mJobBounds[job + 1];
            for (u32 index = first; index < last; ++index) {
                self.process_chunk(self.mLevelChunks[index], dispatch.runId, dispatch.alpha);
            }
        }
    };
    ecs::iteration_scope lock = mScene->world().lock_iteration();
    core::jobs::job_system *jobs = context.jobs();
    if (jobs != nullptr && jobCount > 1 && totalRows >= INLINE_ROW_LIMIT) {
        jobs->parallel_for(jobCount, 1, body);
    } else {
        body(0, jobCount);
    }
    lock.release();

    // Dirty tags go through the system's buffer (system thread), flushed when the stage ends.
    const ecs::component_id dirtyId = mScene->ids().transformDirty;
    for (const level_chunk &item : mLevelChunks) {
        if (!item.removeDirty) {
            continue;
        }
        for (const ecs::entity entity : item.view.entities()) {
            context.commands().remove_raw(entity, dirtyId);
        }
    }
}

void transform_system::collect_chunk(ecs::chunk_view &chunk, u64 since) {
    const scene_component_ids &ids = mScene->ids();
    const u32 count = chunk.count();
    if (count == 0) {
        return;
    }
    const ecs::chunk_view &constChunk = chunk;
    const transform_state *states = static_cast<const transform_state *>(constChunk.column_data(ids.transformState));
    if (constChunk.column_data(ids.localTransform) == nullptr || states == nullptr
      || constChunk.column_data(ids.worldTransform) == nullptr
      || constChunk.column_data(ids.renderWorldTransform) == nullptr) {
        // A local_transform without its companions: the observers repair it at the next flush.
        mCounters.chunksSkipped.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    level_chunk item = {};
    item.view = chunk;
    item.removeDirty = chunk.has(ids.transformDirty);
    item.recomputeAll = item.removeDirty || chunk.column_version(ids.localTransform) > since;
    item.whole = item.recomputeAll || chunk.has(ids.interpolatedTransform);
    if (item.whole) {
        item.rows = count;
    } else {
        for (u32 row = 0; row < count; ++row) {
            item.rows += states[row].pending ? 1u : 0u;
        }
        if (item.rows == 0) {
            // Nothing above it moved and nothing in it changed: left untouched, no stamp.
            mCounters.chunksSkipped.fetch_add(1, std::memory_order_relaxed);
            return;
        }
    }
    mLevelChunks.push_back(item);
}

void transform_system::process_chunk(const level_chunk &item, u32 runId, f32 alpha) {
    const scene_component_ids &ids = mScene->ids();
    const ecs::world &world = mScene->world();
    ecs::chunk_view chunk = item.view;
    const ecs::chunk_view &constChunk = chunk;
    const u32 count = chunk.count();
    const local_transform *locals = static_cast<const local_transform *>(constChunk.column_data(ids.localTransform));
    const parent *parents = static_cast<const parent *>(constChunk.column_data(ids.parent));
    const interpolated_transform *poses =
      static_cast<const interpolated_transform *>(constChunk.column_data(ids.interpolatedTransform));
    const children *childLists = static_cast<const children *>(constChunk.column_data(ids.children));
    world_transform *worlds = static_cast<world_transform *>(chunk.column_data(ids.worldTransform, true));
    render_world_transform *renders =
      static_cast<render_world_transform *>(chunk.column_data(ids.renderWorldTransform, true));
    transform_state *states = static_cast<transform_state *>(chunk.column_data(ids.transformState, true));

    // Siblings are created together and sit in consecutive rows, so the parent of the previous row is
    // resolved once per run of siblings instead of once per row.
    ecs::entity cachedParent = ecs::NULL_ENTITY;
    parent_state cachedParentData = {};
    u32 visited = 0;
    u32 worldWrites = 0;
    u32 renderWrites = 0;
    for (u32 row = 0; row < count; ++row) {
        if (!item.whole && !states[row].pending) {
            continue;
        }
        ++visited;
        states[row].pending = false;
        parent_state parentData = {};
        if (parents != nullptr) {
            if (parents[row].value != cachedParent || cachedParent.is_null()) {
                cachedParent = parents[row].value;
                cachedParentData = lookup_parent(world, ids, cachedParent);
            }
            parentData = cachedParentData;
        }
        const bool parentUpdated = parentData.state != nullptr && parentData.state->lastUpdateRun == runId;
        const bool recomputeWorld = item.recomputeAll || parentUpdated;
        if (recomputeWorld) {
            const math::mat4 localMatrix = locals[row].to_transform().to_matrix();
            worlds[row].matrix = parentData.world != nullptr ? parentData.world->matrix * localMatrix : localMatrix;
            states[row].lastUpdateRun = runId;
            ++worldWrites;
        }

        const bool self = poses != nullptr;
        const bool subtree = self || parent_in_interpolated_subtree(parentData);
        const bool hadSubtree = has_flag(states[row].flags, transform_state_flags::INTERPOLATED_SUBTREE);
        if (self) {
            renders[row].matrix = math::transform::lerp(poses[row].previous, poses[row].current, alpha).to_matrix();
            ++renderWrites;
        } else if (subtree) {
            const math::mat4 localMatrix = locals[row].to_transform().to_matrix();
            renders[row].matrix = parentData.render != nullptr ? parentData.render->matrix * localMatrix : localMatrix;
            ++renderWrites;
        } else if (recomputeWorld || hadSubtree) {
            renders[row].matrix = worlds[row].matrix;
            ++renderWrites;
        }
        states[row].flags = subtree ? transform_state_flags::INTERPOLATED_SUBTREE : transform_state_flags::NONE;

        // The children must revisit when their parent's world moved, follows a render pose every frame
        // or changed mode (S4: the stamp carries dirtiness down; the mark says where to read it).
        if (childLists != nullptr && (recomputeWorld || subtree || hadSubtree != subtree)) {
            mark_children_pending(childLists[row]);
        }
    }
    mCounters.entitiesVisited.fetch_add(visited, std::memory_order_relaxed);
    mCounters.worldRecomputed.fetch_add(worldWrites, std::memory_order_relaxed);
    mCounters.renderRecomputed.fetch_add(renderWrites, std::memory_order_relaxed);
    mCounters.chunksVisited.fetch_add(1, std::memory_order_relaxed);
}

void transform_system::mark_children_pending(const children &list) {
    // A transformless child (S5) is not in any level, so the mark goes to its own children instead: an
    // explicit stack bounded by the hierarchy depth, never recursion.
    struct frame {
        const ecs::entity *next = nullptr;
        const ecs::entity *end = nullptr;
    };
    if (list.entities.empty()) {
        return;
    }
    const scene_component_ids &ids = mScene->ids();
    ecs::world &world = mScene->world();
    std::array<frame, MAX_HIERARCHY_DEPTH> stack = {};
    usize depth = 0;
    stack[0].next = list.entities.data();
    stack[0].end = list.entities.data() + list.entities.size();
    while (true) {
        frame &top = stack[depth];
        if (top.next == top.end) {
            if (depth == 0) {
                return;
            }
            --depth;
            continue;
        }
        const ecs::entity child = *top.next;
        ++top.next;
        // No column stamp (forWrite false): the child's own level stamps the column when it processes
        // the row, and a stamp here would be one contended atomic store per child on its chunk's
        // version line.
        transform_state *state = static_cast<transform_state *>(world.get_raw(child, ids.transformState, false));
        if (state != nullptr) {
            state->pending = true;
            continue;
        }
        const children *grandchildren = static_cast<const children *>(world.get_raw(child, ids.children));
        if (grandchildren == nullptr || grandchildren->entities.empty() || depth + 1 >= stack.size()) {
            continue;
        }
        ++depth;
        stack[depth].next = grandchildren->entities.data();
        stack[depth].end = grandchildren->entities.data() + grandchildren->entities.size();
    }
}
}

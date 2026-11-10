#include "conjure/scene/transform_system.h"

#include "conjure/core/math/transform.h"
#include "conjure/ecs/archetype.h"
#include "conjure/ecs/chunk_view.h"

#include <algorithm>
#include <atomic>
#include <bitset>

// The propagation pass (docs/design/13_scene.md 3.4). process_chunk runs on job threads inside
// parallel_for_chunks: it reads its own chunk's columns, reads the parents' columns through const random
// access (the parents' level finished before this one started) and writes only rows of its own chunk.

namespace conjure::scene {

namespace {

    /// <summary>
    /// Most rows a chunk can hold (the entity column alone fills the block).
    /// </summary>
    constexpr usize MAX_CHUNK_ROWS = ecs::CHUNK_SIZE / sizeof(ecs::entity);

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

    // Not const: parallel_for_chunks hands the callable through a void * context.
    auto visit = [this, runId, since, alpha](ecs::chunk_view &chunk, ecs::command_buffer &commands) {
        process_chunk(chunk, commands, runId, since, alpha);
    };
    context.parallel_for_chunks(mRootQuery, visit);
    const u8 deepest = mScene->max_depth_in_use();
    for (u8 level = 1; level <= deepest && level < MAX_HIERARCHY_DEPTH; ++level) {
        context.parallel_for_chunks(mLevelQueries[static_cast<usize>(level) - 1], visit);
    }

    mStats.entitiesVisited = mCounters.entitiesVisited.load(std::memory_order_relaxed);
    mStats.worldRecomputed = mCounters.worldRecomputed.load(std::memory_order_relaxed);
    mStats.renderRecomputed = mCounters.renderRecomputed.load(std::memory_order_relaxed);
    mStats.chunksVisited = mCounters.chunksVisited.load(std::memory_order_relaxed);
    mStats.chunksSkipped = mCounters.chunksSkipped.load(std::memory_order_relaxed);
    mStats.levels = static_cast<u8>(deepest + 1);
}

void transform_system::process_chunk(
  ecs::chunk_view &chunk,
  ecs::command_buffer &commands,
  u32 runId,
  u64 since,
  f32 alpha
) {
    const scene_component_ids &ids = mScene->ids();
    const ecs::world &world = mScene->world();
    const u32 count = chunk.count();
    if (count == 0) {
        return;
    }
    const bool localChanged = chunk.column_version(ids.localTransform) > since;
    const bool chunkDirty = chunk.has(ids.transformDirty);
    const ecs::chunk_view &constChunk = chunk;
    const local_transform *locals = static_cast<const local_transform *>(constChunk.column_data(ids.localTransform));
    const parent *parents = static_cast<const parent *>(constChunk.column_data(ids.parent));
    const interpolated_transform *poses =
      static_cast<const interpolated_transform *>(constChunk.column_data(ids.interpolatedTransform));
    const transform_state *currentStates =
      static_cast<const transform_state *>(constChunk.column_data(ids.transformState));
    const std::span<const ecs::entity> entities = chunk.entities();
    if (locals == nullptr || currentStates == nullptr || constChunk.column_data(ids.worldTransform) == nullptr
      || constChunk.column_data(ids.renderWorldTransform) == nullptr) {
        // A local_transform without its companions: the observers repair it at the next flush.
        mCounters.chunksSkipped.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    // Pass 1: decide without stamping anything.
    std::bitset<MAX_CHUNK_ROWS> need = {};
    bool anyNeed = false;
    for (u32 row = 0; row < count; ++row) {
        bool rowNeeds = localChanged || chunkDirty || poses != nullptr;
        bool subtree = poses != nullptr;
        if (parents != nullptr) {
            const parent_state parentData = lookup_parent(world, ids, parents[row].value);
            if (parentData.state != nullptr && parentData.state->lastUpdateRun == runId) {
                rowNeeds = true;
            }
            if (parent_in_interpolated_subtree(parentData)) {
                rowNeeds = true;
                subtree = true;
            }
        }
        const bool hadSubtree = has_flag(currentStates[row].flags, transform_state_flags::INTERPOLATED_SUBTREE);
        if (hadSubtree != subtree) {
            rowNeeds = true;
        }
        if (rowNeeds) {
            need.set(row);
            anyNeed = true;
        }
    }
    if (!anyNeed) {
        mCounters.chunksSkipped.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    // Pass 2: write the rows that need work.
    world_transform *worlds = static_cast<world_transform *>(chunk.column_data(ids.worldTransform, true));
    render_world_transform *renders =
      static_cast<render_world_transform *>(chunk.column_data(ids.renderWorldTransform, true));
    transform_state *states = static_cast<transform_state *>(chunk.column_data(ids.transformState, true));
    u32 visited = 0;
    u32 worldWrites = 0;
    u32 renderWrites = 0;
    for (u32 row = 0; row < count; ++row) {
        if (!need.test(row)) {
            continue;
        }
        ++visited;
        parent_state parentData = {};
        if (parents != nullptr) {
            parentData = lookup_parent(world, ids, parents[row].value);
        }
        const bool parentUpdated = parentData.state != nullptr && parentData.state->lastUpdateRun == runId;
        const bool recomputeWorld = localChanged || chunkDirty || parentUpdated;
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
    }
    if (chunkDirty) {
        for (u32 row = 0; row < count; ++row) {
            commands.remove_raw(entities[row], ids.transformDirty);
        }
    }
    mCounters.entitiesVisited.fetch_add(visited, std::memory_order_relaxed);
    mCounters.worldRecomputed.fetch_add(worldWrites, std::memory_order_relaxed);
    mCounters.renderRecomputed.fetch_add(renderWrites, std::memory_order_relaxed);
    mCounters.chunksVisited.fetch_add(1, std::memory_order_relaxed);
}
}

#include "conjure/ecs/system.h"

#include "ecs/chunk.h"

#include "conjure/core/assert.h"

#include <algorithm>
#include <functional>
#include <limits>
#include <utility>

using namespace conjure;
using namespace conjure::ecs;

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

system_context::system_context(
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
) noexcept :
    mWorld(&target),
    mFrame(&frame),
    mSinceVersion(sinceVersion),
    mRunVersion(runVersion),
    mCommands(&commands),
    mJobs(jobs),
    mDesc(&desc),
    mScratch(&scratch),
    mParallelChunks(parallelChunks),
    mMinEntitiesPerJob(minEntitiesPerJob) {}

system_context::~system_context() {}


/*===========================================================================*/
/*===========================================================================*/
// Private methods

void system_context::validate_access(const view_access &access) const {
    const component_mask declared = mDesc->reads | mDesc->writes;
    const component_mask touched = access.reads | access.writes;
    if (!declared.contains_all(touched)) {
        CONJURE_ASSERT(false, "system '{}' views components it did not declare as read or written", mDesc->name);
        return;
    }
    if (!mDesc->writes.contains_all(access.writes)) {
        CONJURE_ASSERT(false, "system '{}' views components for writing it did not declare as written", mDesc->name);
    }
}

void system_context::dispatch_chunks(
  query &target,
  void (*visit)(void *context, chunk_view &view, command_buffer &commands),
  void *context
) {
    using chunk_ref = system_scratch::chunk_ref;
    std::vector<chunk_ref> &chunks = mScratch->chunks;
    chunks.clear();

    u32 minCapacity = std::numeric_limits<u32>::max();
    for (archetype *owner : target.matches()) {
        if (owner->entity_count() == 0) {
            continue;
        }
        minCapacity = std::min(minCapacity, owner->layout().capacity);
        for (u32 chunkIndex = 0; chunkIndex < owner->chunk_count(); ++chunkIndex) {
            if (target.chunk_passes(*owner, chunkIndex)) {
                chunks.push_back(chunk_ref{owner, chunkIndex});
            }
        }
    }
    const usize total = chunks.size();
    if (total == 0) {
        mLastGrainCount = 0;
        return;
    }

    const u32 chunksPerGrain = std::max<u32>(1, mMinEntitiesPerJob / std::max<u32>(1, minCapacity));
    const usize grainCount = (total + chunksPerGrain - 1) / chunksPerGrain;
    std::vector<command_buffer> &grains = mScratch->grainBuffers;
    while (grains.size() < grainCount) {
        grains.emplace_back(*mWorld);
    }

    const component_registry &registry = mWorld->registry();
    const u64 stamp = mRunVersion;
    const auto runGrain = [&](usize grain) {
        const usize begin = grain * chunksPerGrain;
        const usize end = std::min(total, begin + chunksPerGrain);
        for (usize index = begin; index < end; ++index) {
            const chunk_ref &ref = chunks[index];
            chunk_view view(*ref.owner, *ref.owner->mChunks[ref.chunkIndex], ref.chunkIndex, registry, stamp, this);
            visit(context, view, grains[grain]);
        }
    };

    iteration_scope scope = mWorld->lock_iteration();
    if (mJobs != nullptr && mParallelChunks && grainCount > 1) {
        const std::function<void(usize, usize)> body = [&runGrain](usize begin, usize end) {
            for (usize grain = begin; grain < end; ++grain) {
                runGrain(grain);
            }
        };
        mJobs->parallel_for(grainCount, 1, body);
    } else {
        for (usize grain = 0; grain < grainCount; ++grain) {
            runGrain(grain);
        }
    }
    scope.release();

    // Grain order, whatever worker ran which grain: command application stays deterministic.
    for (usize grain = 0; grain < grainCount; ++grain) {
        mCommands->append(std::move(grains[grain]));
    }
    mLastGrainCount = static_cast<u32>(grainCount);
}

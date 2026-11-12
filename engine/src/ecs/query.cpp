#include "conjure/ecs/query.h"

#include "ecs/chunk.h"

#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/system.h"
#include "conjure/ecs/world.h"

#include <utility>

using namespace conjure;
using namespace conjure::ecs;

namespace {
    /// <summary>
    /// The version non-const spans stamp: the run version of the bound system, so a structural call or
    /// a sibling system bumping the world version inside the run never makes the system's own writes
    /// pass its next since_version(); the current world version for a plain world query.
    /// </summary>
    u64 stamp_version(const world &target, const system_context *context) noexcept {
        if (context != nullptr) {
            return context->run_version();
        }
        return target.version();
    }
}

/*===========================================================================*/
/*===========================================================================*/
// detail

const component_registry &conjure::ecs::detail::registry_of(const world &target) noexcept {
    return target.registry();
}


/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

query::query(world &target, query_desc desc) :
    mWorld(&target),
    mRegistry(&target.registry()),
    mDesc(std::move(desc)) {}

query::~query() {}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

u32 query::chunk_count() {
    refresh();
    u32 total = 0;
    for (const archetype *owner : mMatches) {
        for (u32 chunkIndex = 0; chunkIndex < owner->chunk_count(); ++chunkIndex) {
            if (chunk_passes(*owner, chunkIndex)) {
                ++total;
            }
        }
    }
    return total;
}

u32 query::entity_count() {
    refresh();
    u32 total = 0;
    for (const archetype *owner : mMatches) {
        for (u32 chunkIndex = 0; chunkIndex < owner->chunk_count(); ++chunkIndex) {
            if (chunk_passes(*owner, chunkIndex)) {
                total += owner->chunk_entity_count(chunkIndex);
            }
        }
    }
    return total;
}

chunk_view query::chunk(u32 index) {
    refresh();
    u32 seen = 0;
    const u64 stamp = stamp_version(*mWorld, mContext);
    for (archetype *owner : mMatches) {
        for (u32 chunkIndex = 0; chunkIndex < owner->chunk_count(); ++chunkIndex) {
            if (!chunk_passes(*owner, chunkIndex)) {
                continue;
            }
            if (seen == index) {
                return chunk_view(*owner, *owner->mChunks[chunkIndex], chunkIndex, *mRegistry, stamp, mContext);
            }
            ++seen;
        }
    }
    return chunk_view();
}

std::span<archetype *const> query::matches() {
    refresh();
    return std::span<archetype *const>(mMatches.data(), mMatches.size());
}

void query::reset(query_desc desc) {
    mDesc = std::move(desc);
    mMatches.clear();
    mSeenArchetypeCount = 0;
}

void query::refresh() {
    const std::span<archetype *const> archetypes = mWorld->archetypes();
    for (usize index = mSeenArchetypeCount; index < archetypes.size(); ++index) {
        archetype *candidate = archetypes[index];
        const component_mask &mask = candidate->mask();
        if (mask.contains_all(mDesc.with) && !mask.contains_any(mDesc.without)) {
            mMatches.push_back(candidate);
        }
    }
    mSeenArchetypeCount = static_cast<u32>(archetypes.size());
}

bool query::chunk_passes(const archetype &owner, u32 chunkIndex) const noexcept {
    const ::conjure::ecs::chunk *block = owner.chunk_at(chunkIndex);
    if (block == nullptr || block->count() == 0) {
        return false;
    }
    if (mDesc.changed.none()) {
        return true;
    }
    return block->max_version(owner.layout(), mDesc.changed) > mDesc.sinceVersion;
}


/*===========================================================================*/
/*===========================================================================*/
// Private methods

void query::visit_chunks(void (*visit)(void *context, chunk_view &view), void *context) {
    refresh();
    const iteration_scope scope = mWorld->lock_iteration();
    const u64 stamp = stamp_version(*mWorld, mContext);
    for (archetype *owner : mMatches) {
        if (owner->entity_count() == 0) {
            continue;
        }
        for (u32 chunkIndex = 0; chunkIndex < owner->chunk_count(); ++chunkIndex) {
            if (!chunk_passes(*owner, chunkIndex)) {
                continue;
            }
            chunk_view view(*owner, *owner->mChunks[chunkIndex], chunkIndex, *mRegistry, stamp, mContext);
            visit(context, view);
        }
    }
}

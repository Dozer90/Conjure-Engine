#include "conjure/ecs/chunk_view.h"

#include "ecs/chunk.h"

#include "conjure/ecs/system.h"

#include "conjure/core/assert.h"

using namespace conjure;
using namespace conjure::ecs;

/*===========================================================================*/
/*===========================================================================*/
// detail

bool conjure::ecs::detail::context_declares_write(const system_context &context, component_id id) noexcept {
    return context.desc().writes.test(id);
}

void conjure::ecs::detail::report_undeclared_write(const system_context &context, component_id id) {
    CONJURE_ASSERT(
      false,
      "system '{}' writes component {} ('{}') without declaring it",
      context.desc().name,
      id.value,
      context.world().registry().info(id).name
    );
}


/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

chunk_view::chunk_view(
  archetype &owner,
  chunk &target,
  u32 chunkIndex,
  const component_registry &registry,
  u64 stampVersion,
  const system_context *context
) noexcept :
    mArchetype(&owner),
    mChunk(&target),
    mRegistry(&registry),
    mStampVersion(stampVersion),
    mContext(context),
    mChunkIndex(chunkIndex) {}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

u32 chunk_view::count() const noexcept {
    return mChunk == nullptr ? 0 : mChunk->count();
}

std::span<const entity> chunk_view::entities() const noexcept {
    if (mChunk == nullptr) {
        return {};
    }
    return mChunk->live_entities();
}

bool chunk_view::has(component_id id) const noexcept {
    return mArchetype != nullptr && mArchetype->has(id);
}

void *chunk_view::column_data(component_id id, bool forWrite) noexcept {
    if (mChunk == nullptr) {
        return nullptr;
    }
    const chunk_layout &layout = mArchetype->layout();
    const u16 column = layout.column_of(id);
    if (column == chunk_layout::NO_COLUMN) {
        return nullptr;
    }
    if (forWrite) {
#if CONJURE_ASSERTS_ENABLED
        // The declared-write check is Debug and Development validation (01_ecs.md 3.5); the stamp is not.
        if (mContext != nullptr && !detail::context_declares_write(*mContext, id)) {
            detail::report_undeclared_write(*mContext, id);
        }
#endif
        mChunk->stamp(column, mStampVersion);
    }
    return mChunk->column(layout, column);
}

const void *chunk_view::column_data(component_id id) const noexcept {
    if (mChunk == nullptr) {
        return nullptr;
    }
    const chunk_layout &layout = mArchetype->layout();
    const u16 column = layout.column_of(id);
    if (column == chunk_layout::NO_COLUMN) {
        return nullptr;
    }
    return mChunk->column(layout, column);
}

u32 chunk_view::column_stride(component_id id) const noexcept {
    if (mArchetype == nullptr) {
        return 0;
    }
    const chunk_layout &layout = mArchetype->layout();
    const u16 column = layout.column_of(id);
    if (column == chunk_layout::NO_COLUMN) {
        return 0;
    }
    return layout.columnSizes[column];
}

u64 chunk_view::column_version(component_id id) const noexcept {
    if (mChunk == nullptr) {
        return 0;
    }
    const u16 column = mArchetype->layout().column_of(id);
    if (column == chunk_layout::NO_COLUMN) {
        return 0;
    }
    return mChunk->version(column);
}

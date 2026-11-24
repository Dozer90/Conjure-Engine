#include "conjure/ecs/archetype.h"

#include "ecs/chunk.h"
#include "ecs/chunk_pool.h"

#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/ecs_error.h"
#include "conjure/ecs/log_category.h"

#include "conjure/core/assert.h"
#include "conjure/core/log.h"

#include <algorithm>
#include <utility>

using namespace conjure;
using namespace conjure::ecs;

namespace {
    u32 align_offset(u32 offset, u32 alignment) noexcept {
        const u32 mask = alignment - 1;
        return (offset + mask) & ~mask;
    }

    /// <summary>
    /// Lays the columns out for one capacity. Returns the end offset, or 0 when it would overflow.
    /// </summary>
    u32 layout_columns(chunk_layout &layout, const component_registry &registry, u32 capacity) {
        const u64 entityBytes = static_cast<u64>(sizeof(entity)) * capacity;
        u64 cursor = entityBytes;
        for (u32 column = 0; column < layout.column_count(); ++column) {
            const component_info &info = registry.info(layout.components[column]);
            const u32 alignment = std::max(info.alignment, COLUMN_ALIGNMENT);
            const u64 start = align_offset(static_cast<u32>(cursor), alignment);
            const u64 end = start + static_cast<u64>(info.size) * capacity;
            if (end > CHUNK_SIZE) {
                return 0;
            }
            layout.columnOffsets[column] = static_cast<u32>(start);
            cursor = end;
        }
        return static_cast<u32>(cursor);
    }
}

/*===========================================================================*/
/*===========================================================================*/
// chunk_layout

core::result<chunk_layout> chunk_layout::compute(const component_mask &mask, const component_registry &registry) {
    chunk_layout layout = {};

    // Gather the data columns and the row size; tags only live in the mask.
    u64 rowBytes = sizeof(entity);
    bool unknown = false;
    mask.for_each_set([&](component_id id) {
        if (!registry.is_registered(id)) {
            unknown = true;
            return;
        }
        const component_info &info = registry.info(id);
        if (info.is_tag()) {
            return;
        }
        layout.components.push_back(id);
        rowBytes += info.size;
    });
    if (unknown) {
        return core::make_error(ecs_error::UNKNOWN_COMPONENT, "the mask names an unregistered component");
    }

    // Descending alignment packs the padding at the front where the entity column already pays it.
    std::stable_sort(
      layout.components.begin(),
      layout.components.end(),
      [&registry](component_id left, component_id right) {
          return registry.info(left).alignment > registry.info(right).alignment;
      }
    );
    layout.columnOffsets.resize(layout.components.size());
    layout.columnSizes.resize(layout.components.size());
    for (u32 column = 0; column < layout.column_count(); ++column) {
        const component_id id = layout.components[column];
        layout.columnSizes[column] = registry.info(id).size;
        layout.columnIndex[id.value] = static_cast<u16>(column);
    }

    u32 capacity = static_cast<u32>(CHUNK_SIZE / rowBytes);
    while (capacity > 0 && layout_columns(layout, registry, capacity) == 0) {
        --capacity;
    }
    if (capacity == 0) {
        return core::make_error(ecs_error::ARCHETYPE_TOO_LARGE, "not even one row of this component set fits a chunk");
    }
    layout.capacity = capacity;

    if (capacity < SMALL_CAPACITY_WARNING) {
        CONJURE_LOG_WARN(
          LOG_ECS,
          "archetype with {} components holds only {} rows per {} byte chunk",
          mask.count(),
          capacity,
          CHUNK_SIZE
        );
    }
    return layout;
}

u32 chunk_layout::block_end() const noexcept {
    u32 end = static_cast<u32>(sizeof(entity)) * capacity;
    for (u32 column = 0; column < column_count(); ++column) {
        const u32 columnEnd = columnOffsets[column] + columnSizes[column] * capacity;
        if (columnEnd > end) {
            end = columnEnd;
        }
    }
    return end;
}


/*===========================================================================*/
/*===========================================================================*/
// archetype: constructors and destructors

archetype::archetype(const component_mask &mask, chunk_layout layout, u32 index) :
    mMask(mask),
    mLayout(std::move(layout)),
    mIndex(index) {}

archetype::~archetype() {
    CONJURE_ASSERT(mChunks.empty(), "archetype destroyed while still holding chunks; the world returns them first");
}


/*===========================================================================*/
/*===========================================================================*/
// archetype: public methods

u32 archetype::chunk_entity_count(u32 chunkIndex) const noexcept {
    const chunk *target = chunk_at(chunkIndex);
    return target == nullptr ? 0 : target->count();
}

std::span<const entity> archetype::chunk_entities(u32 chunkIndex) const noexcept {
    const chunk *target = chunk_at(chunkIndex);
    if (target == nullptr) {
        return {};
    }
    return target->live_entities();
}

const void *archetype::chunk_column(u32 chunkIndex, component_id id) const noexcept {
    const chunk *target = chunk_at(chunkIndex);
    const u16 column = mLayout.column_of(id);
    if (target == nullptr || column == chunk_layout::NO_COLUMN) {
        return nullptr;
    }
    return target->column(mLayout, column);
}

u64 archetype::chunk_column_version(u32 chunkIndex, component_id id) const noexcept {
    const chunk *target = chunk_at(chunkIndex);
    const u16 column = mLayout.column_of(id);
    if (target == nullptr || column == chunk_layout::NO_COLUMN) {
        return 0;
    }
    return target->version(column);
}


/*===========================================================================*/
/*===========================================================================*/
// archetype: private methods

chunk *archetype::chunk_at(u32 chunkIndex) noexcept {
    if (chunkIndex >= mChunks.size()) {
        return nullptr;
    }
    return mChunks[chunkIndex].get();
}

const chunk *archetype::chunk_at(u32 chunkIndex) const noexcept {
    if (chunkIndex >= mChunks.size()) {
        return nullptr;
    }
    return mChunks[chunkIndex].get();
}

archetype::archetype_edge *archetype::find_edge(component_id id) noexcept {
    for (archetype_edge &entry : mEdges) {
        if (entry.id == id) {
            return &entry;
        }
    }
    return nullptr;
}

archetype::archetype_edge &archetype::edge(component_id id) {
    archetype_edge *existing = find_edge(id);
    if (existing != nullptr) {
        return *existing;
    }
    mEdges.push_back(archetype_edge{id, nullptr, nullptr});
    return mEdges.back();
}

chunk *archetype::open_chunk(u32 &chunkIndex) noexcept {
    if (mChunks.empty()) {
        return nullptr;
    }
    // remove_row keeps one drained spare behind the last used chunk; that chunk fills up first so the
    // packing invariant (only the last used chunk is partial) survives create/destroy churn.
    usize index = mChunks.size() - 1;
    if (mChunks[index]->count() == 0 && index > 0 && mChunks[index - 1]->count() < mLayout.capacity) {
        --index;
    }
    chunk *candidate = mChunks[index].get();
    if (candidate->count() == mLayout.capacity) {
        return nullptr;
    }
    chunkIndex = static_cast<u32>(index);
    return candidate;
}

core::result<void> archetype::reserve_row(chunk_pool &pool) {
    if (mLayout.capacity == 0) {
        return core::make_error(ecs_error::ARCHETYPE_TOO_LARGE, "archetype holds no rows");
    }
    u32 chunkIndex = 0;
    if (open_chunk(chunkIndex) != nullptr) {
        return core::result<void>::success();
    }
    void *block = pool.allocate();
    if (block == nullptr) {
        return core::make_error(ecs_error::OUT_OF_MEMORY, "the chunk pool could not provide a block");
    }
    if (!mSpareChunks.empty()) {
        // A drained chunk's metadata was parked by the world; attaching the block is allocation-free.
        std::unique_ptr<chunk> spare = std::move(mSpareChunks.back());
        mSpareChunks.pop_back();
        spare->rebind(block);
        mChunks.push_back(std::move(spare));
        return core::result<void>::success();
    }
    mChunks.push_back(std::make_unique<chunk>(block, mLayout.column_count()));
    return core::result<void>::success();
}

core::result<archetype::row_location> archetype::push_row(chunk_pool &pool) {
    CONJURE_TRY(reserve_row(pool));
    u32 chunkIndex = 0;
    chunk *target = open_chunk(chunkIndex);
    if (target == nullptr) {
        CONJURE_ASSERT(false, "archetype::reserve_row left no open chunk");
        return core::make_error(ecs_error::OUT_OF_MEMORY, "no chunk accepts a row after a successful reservation");
    }
    row_location location = {};
    location.chunkIndex = chunkIndex;
    location.row = target->count();
    target->set_count(target->count() + 1);
    ++mEntityCount;
    return location;
}

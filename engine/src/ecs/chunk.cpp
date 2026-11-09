#include "ecs/chunk.h"

using namespace conjure;
using namespace conjure::ecs;

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

chunk::chunk(void *block, u32 columnCount) :
    mBlock(block),
    mColumnCount(columnCount) {
    if (columnCount > 0) {
        mColumnVersions = std::make_unique<std::atomic<u64>[]>(columnCount);
        for (u32 column = 0; column < columnCount; ++column) {
            mColumnVersions[column].store(0, std::memory_order_relaxed);
        }
    }
}

chunk::~chunk() {}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

u64 chunk::max_version(const chunk_layout &layout, const component_mask &components) const noexcept {
    u64 highest = 0;
    components.for_each_set([&](component_id id) {
        const u16 column = layout.column_of(id);
        if (column == chunk_layout::NO_COLUMN) {
            return;
        }
        const u64 stamped = version(column);
        if (stamped > highest) {
            highest = stamped;
        }
    });
    return highest;
}

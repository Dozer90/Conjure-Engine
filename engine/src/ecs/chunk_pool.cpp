#include "ecs/chunk_pool.h"

#include "conjure/ecs/archetype.h"

using namespace conjure;
using namespace conjure::ecs;

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

chunk_pool::chunk_pool(u32 chunksPerSlab, u32 maxSlabs) :
    mPool(CHUNK_SIZE, CHUNK_ALIGNMENT, chunksPerSlab == 0 ? 1u : chunksPerSlab, maxSlabs) {}

chunk_pool::~chunk_pool() {}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

void *chunk_pool::allocate() noexcept {
    return mPool.allocate();
}

void chunk_pool::deallocate(void *block) noexcept {
    mPool.deallocate(block);
}

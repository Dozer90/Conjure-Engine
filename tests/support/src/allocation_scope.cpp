#include "conjure/testing/allocation_scope.h"

using namespace conjure::testing;

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

allocation_scope::allocation_scope() :
    mStart(conjure::core::memory::snapshot()) {}

allocation_scope::~allocation_scope() {}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

conjure::u64 allocation_scope::allocation_count() const noexcept {
    return conjure::core::memory::snapshot().allocationCount - mStart.allocationCount;
}

conjure::u64 allocation_scope::free_count() const noexcept {
    return conjure::core::memory::snapshot().freeCount - mStart.freeCount;
}

conjure::u64 allocation_scope::allocated_bytes() const noexcept {
    return conjure::core::memory::snapshot().allocatedBytes - mStart.allocatedBytes;
}

void allocation_scope::reset() noexcept {
    mStart = conjure::core::memory::snapshot();
}

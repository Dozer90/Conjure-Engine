#include "conjure/core/memory/allocation_counters.h"

using namespace conjure::core::memory;

/*===========================================================================*/
/*===========================================================================*/
// Counters

allocation_counters &conjure::core::memory::counters() noexcept {
    // Function-local so the counters exist before any static initialiser allocates, whatever the
    // translation unit order.
    static allocation_counters instance = {};
    return instance;
}

memory_snapshot conjure::core::memory::snapshot() noexcept {
    const allocation_counters &current = counters();
    memory_snapshot copy = {};
    copy.allocationCount = current.allocationCount.load(std::memory_order_relaxed);
    copy.freeCount = current.freeCount.load(std::memory_order_relaxed);
    copy.allocatedBytes = current.allocatedBytes.load(std::memory_order_relaxed);
    return copy;
}

void conjure::core::memory::record_allocation(usize bytes) noexcept {
    allocation_counters &current = counters();
    current.allocationCount.fetch_add(1, std::memory_order_relaxed);
    current.allocatedBytes.fetch_add(static_cast<u64>(bytes), std::memory_order_relaxed);
}

void conjure::core::memory::record_free() noexcept {
    counters().freeCount.fetch_add(1, std::memory_order_relaxed);
}

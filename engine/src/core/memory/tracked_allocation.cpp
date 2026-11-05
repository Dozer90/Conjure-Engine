#include "conjure/core/memory/tracked_allocation.h"

#include "conjure/core/memory/allocation_counters.h"

#include "conjure/core/assert.h"
#include "conjure/core/platform_detect.h"

#include <algorithm>
#include <cstdint>
#include <cstring>

// The one backend file of core::memory: aligned allocation differs between the C runtimes, so the
// platform switch lives here and nowhere else in the area.
#if CONJURE_PLATFORM_WINDOWS
    #include <malloc.h>
#else
    #include <cstdlib>
    #include <stdlib.h>     // posix_memalign is POSIX, not ISO C: <cstdlib> alone need not declare it
#endif

using namespace conjure::core::memory;

/*===========================================================================*/
/*===========================================================================*/
// Platform backend

namespace {
    // Alignments below the default are raised to it: posix_memalign rejects alignments smaller than
    // a pointer, and callers never lose anything by over-aligning.
    conjure::usize effective_alignment(conjure::usize alignment) noexcept {
        return std::max(alignment, DEFAULT_ALIGNMENT);
    }

    // A zero-byte request still gets a unique block like operator new. Neither runtime needs the size
    // rounded to the alignment (_aligned_malloc and posix_memalign both take any size), so no
    // rounding happens here and the size arithmetic cannot wrap.
    conjure::usize effective_size(conjure::usize bytes) noexcept {
        return std::max<conjure::usize>(bytes, 1);
    }

    // Requests above MAX_ALLOCATION_BYTES are impossible by construction; they are rejected before
    // the runtime sees them so that the failure is deterministic (and silent under sanitizers, which
    // otherwise treat an absurd malloc as a bug).
    bool validate_size(conjure::usize bytes) noexcept {
        return bytes <= MAX_ALLOCATION_BYTES;
    }

    void *system_aligned_allocate(conjure::usize bytes, conjure::usize alignment) noexcept {
#if CONJURE_PLATFORM_WINDOWS
        return _aligned_malloc(bytes, alignment);
#else
        // posix_memalign is available on every supported Linux and macOS, has no deployment-target
        // gate (unlike std::aligned_alloc on Apple) and no size-multiple rule.
        void *pointer = nullptr;
        if (posix_memalign(&pointer, alignment, bytes) != 0) {
            return nullptr;
        }
        return pointer;
#endif
    }

    void system_aligned_free(void *pointer) noexcept {
#if CONJURE_PLATFORM_WINDOWS
        _aligned_free(pointer);
#else
        std::free(pointer);
#endif
    }

    bool validate_alignment(conjure::usize alignment) noexcept {
        const bool valid = is_power_of_two(alignment);
        CONJURE_ASSERT(valid, "alignment {} is not a power of two", alignment);
        return valid;
    }
}


/*===========================================================================*/
/*===========================================================================*/
// Public functions

bool conjure::core::memory::is_aligned(const void *pointer, usize alignment) noexcept {
    const std::uintptr_t address = reinterpret_cast<std::uintptr_t>(pointer);
    return (address & (static_cast<std::uintptr_t>(alignment) - 1)) == 0;
}

void *conjure::core::memory::tracked_allocate(usize bytes, usize alignment) noexcept {
    if (!validate_alignment(alignment) || !validate_size(bytes)) {
        return nullptr;
    }

    void *pointer = system_aligned_allocate(effective_size(bytes), effective_alignment(alignment));
    if (pointer == nullptr) {
        return nullptr;
    }

    record_allocation(bytes);
    return pointer;
}

void conjure::core::memory::tracked_free(void *pointer, usize bytes) noexcept {
    // The size is part of the contract for symmetry with the allocation call; the counters only
    // count calls (live bytes are not tracked, see allocation_counters.h).
    CONJURE_UNUSED(bytes);
    if (pointer == nullptr) {
        return;
    }

    system_aligned_free(pointer);
    record_free();
}

void *conjure::core::memory::tracked_reallocate(
  void *pointer,
  usize oldBytes,
  usize newBytes,
  usize alignment
) noexcept {
    if (pointer == nullptr) {
        return tracked_allocate(newBytes, alignment);
    }
    if (newBytes == 0) {
        tracked_free(pointer, oldBytes);
        return nullptr;
    }
    if (!validate_alignment(alignment) || !validate_size(newBytes)) {
        return nullptr;
    }

    const usize actualAlignment = effective_alignment(alignment);

#if CONJURE_PLATFORM_WINDOWS
    void *resized = _aligned_realloc(pointer, newBytes, actualAlignment);
    if (resized == nullptr) {
        return nullptr;
    }
#else
    void *resized = system_aligned_allocate(newBytes, actualAlignment);
    if (resized == nullptr) {
        return nullptr;
    }
    std::memcpy(resized, pointer, std::min(oldBytes, newBytes));
    system_aligned_free(pointer);
#endif

    // One free plus one allocation keeps "allocations minus frees" equal to the live block count.
    record_free();
    record_allocation(newBytes);
    return resized;
}

#pragma once

#include "conjure/core/api.h"
#include "conjure/core/types.h"

#include <cstddef>
#include <limits>

// Counted, aligned heap allocation.
//
// Everything in the engine that needs heap memory outside STL containers goes through these three
// functions: they are the only place core::memory touches the system allocator, and every call bumps
// the process-wide allocation_counters. Alignment is honoured exactly (up to any power of two the
// system allocator accepts; 4096 is tested) and the implementation is the platform's aligned
// allocator, so the result must be released with tracked_free, never with free() or delete.

namespace conjure::core::memory {

    /// <summary>
    /// Alignment used when a caller does not ask for a specific one. Every allocation is at least
    /// this aligned, whatever smaller value was requested.
    ///
    /// Trap: alignof(std::max_align_t) is 8 on MSVC x64 but __STDCPP_DEFAULT_NEW_ALIGNMENT__ is 16
    /// there, so this is *not* the alignment operator new guarantees. A replacement of global
    /// operator new (the test support's tracked_global_new.cpp) must pass
    /// __STDCPP_DEFAULT_NEW_ALIGNMENT__ to tracked_allocate explicitly, and its aligned overloads
    /// must forward their alignment; relying on this default would hand out under-aligned storage
    /// for 16-byte aligned types (SSE and glm aligned types) on MSVC only.
    /// </summary>
    inline constexpr usize DEFAULT_ALIGNMENT = alignof(std::max_align_t);

    /// <summary>
    /// Largest request tracked_allocate and tracked_reallocate accept: PTRDIFF_MAX. No object may be
    /// larger (pointer differences inside it would not be representable), every C runtime refuses
    /// such requests, and rejecting them here keeps the size arithmetic of the allocators built on
    /// top from wrapping. Larger requests fail like any other out-of-memory condition: nullptr, no
    /// assert, counters untouched.
    /// </summary>
    inline constexpr usize MAX_ALLOCATION_BYTES = static_cast<usize>(std::numeric_limits<isize>::max());

    /// <summary>
    /// True for 1, 2, 4, 8, ... and false for 0.
    /// </summary>
    [[nodiscard]] constexpr bool is_power_of_two(usize value) noexcept {
        return value != 0 && (value & (value - 1)) == 0;
    }

    /// <summary>
    /// Rounds 'value' up to the next multiple of 'alignment' (a power of two). Returns 'value' when it
    /// is already aligned. Wraps for values within 'alignment' of the usize maximum; callers that
    /// cannot rule that out check first.
    /// </summary>
    [[nodiscard]] constexpr usize align_up(usize value, usize alignment) noexcept {
        return (value + alignment - 1) & ~(alignment - 1);
    }

    /// <summary>
    /// True when the address of 'pointer' is a multiple of 'alignment' (a power of two). nullptr is
    /// aligned to everything.
    /// </summary>
    [[nodiscard]] CONJURE_API bool is_aligned(const void *pointer, usize alignment) noexcept;

    /// <summary>
    /// Allocates 'bytes' bytes whose address is a multiple of 'alignment' (a power of two; values
    /// below DEFAULT_ALIGNMENT are raised to it). A zero-byte request returns a unique, non-null
    /// pointer like operator new does. Returns nullptr when the system is out of memory, when 'bytes'
    /// exceeds MAX_ALLOCATION_BYTES, or when the alignment is not a power of two (the latter also
    /// asserts). Nothing is counted on failure.
    /// </summary>
    [[nodiscard]] CONJURE_API void *tracked_allocate(usize bytes, usize alignment = DEFAULT_ALIGNMENT) noexcept;

    /// <summary>
    /// Releases memory obtained from tracked_allocate or tracked_reallocate. 'bytes' is the size that
    /// was requested; it is kept for symmetry with the allocation call and for future per-size
    /// statistics, the counters themselves only count calls. nullptr is ignored and not counted.
    /// </summary>
    CONJURE_API void tracked_free(void *pointer, usize bytes) noexcept;

    /// <summary>
    /// Resizes an allocation, preserving the first min(oldBytes, newBytes) bytes. 'alignment' must be
    /// the alignment the block was allocated with. A nullptr 'pointer' behaves like tracked_allocate,
    /// a zero 'newBytes' frees the block and returns nullptr. Counted as one free plus one allocation
    /// so live counts stay balanced. On failure (out of memory, 'newBytes' above MAX_ALLOCATION_BYTES,
    /// invalid alignment) the original block is untouched and nullptr returned.
    ///
    /// Platform note: Windows resizes in place when the C runtime can (_aligned_realloc); POSIX has
    /// no aligned realloc, so the block is always allocated anew and the preserved bytes copied.
    /// Callers that resize often should grow geometrically so the copy stays amortised O(1).
    /// </summary>
    [[nodiscard]] CONJURE_API void *tracked_reallocate(
      void *pointer,
      usize oldBytes,
      usize newBytes,
      usize alignment = DEFAULT_ALIGNMENT
    ) noexcept;
}

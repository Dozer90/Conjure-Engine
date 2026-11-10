#pragma once

#include "conjure/core/types.h"

#include <cstddef>

// Jolt allocation hooks over core::memory (docs/design/14_physics.md P3, 3.1). Every block carries a
// 16-byte header holding the tracked size and the offset back to the tracked base, so JPH::Free and
// JPH::AlignedFree need no size argument and every Jolt allocation bumps allocation_counters().

namespace conjure::physics::jolt_memory {

    /// <summary>
    /// Bytes reserved in front of every block handed to Jolt.
    /// </summary>
    inline constexpr usize HEADER_BYTES = 16;

    /// <summary>
    /// JPH::Allocate: 16-byte aligned block of 'bytes'.
    /// </summary>
    [[nodiscard]] void *allocate(std::size_t bytes) noexcept;

    /// <summary>
    /// JPH::Reallocate: a block of newBytes holding the first min(oldBytes, newBytes) bytes of 'block'.
    /// nullptr behaves like allocate; newBytes == 0 frees and returns nullptr.
    /// </summary>
    [[nodiscard]] void *reallocate(void *block, std::size_t oldBytes, std::size_t newBytes) noexcept;

    /// <summary>
    /// JPH::Free. nullptr is ignored.
    /// </summary>
    void release(void *block) noexcept;

    /// <summary>
    /// JPH::AlignedAllocate: a block aligned to 'alignment' (a power of two, raised to at least 16).
    /// </summary>
    [[nodiscard]] void *aligned_allocate(std::size_t bytes, std::size_t alignment) noexcept;

    /// <summary>
    /// JPH::AlignedFree. nullptr is ignored.
    /// </summary>
    void aligned_release(void *block) noexcept;

    /// <summary>
    /// Points JPH::Allocate / Reallocate / Free / AlignedAllocate / AlignedFree at the hooks above.
    /// </summary>
    void install_hooks() noexcept;
}

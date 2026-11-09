#pragma once

#include "conjure/assets/asset_types.h"

#include "conjure/core/types.h"

#include <cstddef>
#include <span>

// Byte-level FNV-1a helpers shared by the blob, pack, cache and database sources. The constants
// are those of asset_types.h (and of core::hash), so every content hash in the module agrees
// with make_asset_type_id and with core::hash::fnv1a64.

namespace conjure::assets::detail {

    /// <summary>
    /// Continues a 64-bit FNV-1a hash over a byte range.
    /// </summary>
    [[nodiscard]] constexpr u64 fnv1a64_bytes(
      std::span<const std::byte> bytes,
      u64 seed = FNV1A64_OFFSET_BASIS
    ) noexcept {
        u64 hash = seed;
        for (const std::byte value : bytes) {
            hash = (hash ^ static_cast<u8>(value)) * FNV1A64_PRIME;
        }
        return hash;
    }

    /// <summary>
    /// Mixes one 64-bit value into a running hash (as eight little-endian bytes).
    /// </summary>
    [[nodiscard]] constexpr u64 fnv1a64_u64(u64 value, u64 seed) noexcept {
        u64 hash = seed;
        for (u32 shift = 0; shift < 64; shift += 8) {
            hash = (hash ^ static_cast<u8>((value >> shift) & 0xFFu)) * FNV1A64_PRIME;
        }
        return hash;
    }
}

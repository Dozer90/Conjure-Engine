#pragma once

#include "conjure/core/types.h"

#include <cstddef>
#include <span>
#include <string_view>

// FNV-1a hashing. These are the engine's canonical content hashes: string_id, error categories,
// asset identity and the C# NameHash all use them, so the constants and the exact byte order are
// part of the ABI and must never change. Test vectors live in tests/core/foundation/test_hash.cpp
// and are mirrored by Conjure.Managed.

namespace conjure::core::hash {

    inline constexpr u64 FNV1A64_OFFSET_BASIS = 0xCBF29CE484222325ull;
    inline constexpr u64 FNV1A64_PRIME = 0x00000100000001B3ull;
    inline constexpr u32 FNV1A32_OFFSET_BASIS = 0x811C9DC5u;
    inline constexpr u32 FNV1A32_PRIME = 0x01000193u;

    /// <summary>
    /// Continues a 64-bit FNV-1a hash with one more byte.
    /// </summary>
    [[nodiscard]] constexpr u64 fnv1a64_step(u64 hash, u8 byteValue) noexcept {
        return (hash ^ byteValue) * FNV1A64_PRIME;
    }

    /// <summary>
    /// Continues a 32-bit FNV-1a hash with one more byte.
    /// </summary>
    [[nodiscard]] constexpr u32 fnv1a32_step(u32 hash, u8 byteValue) noexcept {
        return (hash ^ byteValue) * FNV1A32_PRIME;
    }

    /// <summary>
    /// 64-bit FNV-1a of the UTF-8 bytes of a string. Usable in constant expressions.
    /// </summary>
    [[nodiscard]] constexpr u64 fnv1a64(std::string_view text, u64 seed = FNV1A64_OFFSET_BASIS) noexcept {
        u64 hash = seed;
        for (const char character : text) {
            hash = fnv1a64_step(hash, static_cast<u8>(character));
        }
        return hash;
    }

    /// <summary>
    /// 64-bit FNV-1a of a byte range.
    /// </summary>
    [[nodiscard]] constexpr u64 fnv1a64(std::span<const std::byte> bytes, u64 seed = FNV1A64_OFFSET_BASIS) noexcept {
        u64 hash = seed;
        for (const std::byte value : bytes) {
            hash = fnv1a64_step(hash, static_cast<u8>(value));
        }
        return hash;
    }

    /// <summary>
    /// 32-bit FNV-1a of the UTF-8 bytes of a string. Usable in constant expressions.
    /// </summary>
    [[nodiscard]] constexpr u32 fnv1a32(std::string_view text, u32 seed = FNV1A32_OFFSET_BASIS) noexcept {
        u32 hash = seed;
        for (const char character : text) {
            hash = fnv1a32_step(hash, static_cast<u8>(character));
        }
        return hash;
    }

    /// <summary>
    /// 32-bit FNV-1a of a byte range.
    /// </summary>
    [[nodiscard]] constexpr u32 fnv1a32(std::span<const std::byte> bytes, u32 seed = FNV1A32_OFFSET_BASIS) noexcept {
        u32 hash = seed;
        for (const std::byte value : bytes) {
            hash = fnv1a32_step(hash, static_cast<u8>(value));
        }
        return hash;
    }

    /// <summary>
    /// Combines two 64-bit hashes order-dependently (boost::hash_combine style with a 64-bit mix).
    /// </summary>
    [[nodiscard]] constexpr u64 combine(u64 seed, u64 value) noexcept {
        // Golden-ratio constant and shifts from the 64-bit variant of hash_combine.
        return seed ^ (value + 0x9E3779B97F4A7C15ull + (seed << 12) + (seed >> 4));
    }
}

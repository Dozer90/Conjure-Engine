#pragma once

#include "conjure/core/platform_detect.h"

#include <cstddef>
#include <cstdint>

// Fundamental type aliases. They live directly in the conjure namespace so every nested module
// namespace (conjure::ecs, conjure::renderer, ...) can use them unqualified.
namespace conjure {

    using u8 = std::uint8_t;
    using u16 = std::uint16_t;
    using u32 = std::uint32_t;
    using u64 = std::uint64_t;

    using i8 = std::int8_t;
    using i16 = std::int16_t;
    using i32 = std::int32_t;
    using i64 = std::int64_t;

    using f32 = float;
    using f64 = double;

    using usize = std::size_t;
    using isize = std::ptrdiff_t;

    using byte = std::byte;

    static_assert(sizeof(f32) == 4, "f32 must be 32 bits");
    static_assert(sizeof(f64) == 8, "f64 must be 64 bits");
    static_assert(sizeof(void *) == 8, "Conjure only supports 64-bit targets");

    inline constexpr usize KIB = 1024;
    inline constexpr usize MIB = 1024 * KIB;
    inline constexpr usize GIB = 1024 * MIB;

    inline constexpr usize CACHE_LINE_SIZE = 64;
}

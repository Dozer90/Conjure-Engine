#pragma once

#include "conjure/core/types.h"

#include <compare>
#include <concepts>
#include <utility>

// Helpers shared by the container headers. Nothing in here is part of the public API; engine code
// includes the container it needs (small_vector.h, flat_hash_map.h, ...) and never this file.

namespace conjure::core::detail {

    /// <summary>
    /// Checked access that found nothing to return (at() on a missing key or index) is a
    /// programmer error. It asserts, then terminates even when the assertion handler continues.
    /// Defined in containers.cpp.
    /// </summary>
    [[noreturn]] void fatal_container_access(const char *message);

    /// <summary>
    /// Comparators that opt into heterogeneous lookup, e.g. std::less<>.
    /// </summary>
    template <typename Compare>
    concept transparent_compare = requires { typename Compare::is_transparent; };

    /// <summary>
    /// Finaliser that spreads entropy into every bit so identity-style std::hash specialisations
    /// (integers on libstdc++) still spread across power-of-two buckets.
    /// </summary>
    [[nodiscard]] constexpr u64 mix_hash(u64 hash) noexcept {
        hash ^= hash >> 33;
        hash *= 0xFF51AFD7ED558CCDull;
        hash ^= hash >> 33;
        hash *= 0xC4CEB9FE1A85EC53ull;
        hash ^= hash >> 33;
        return hash;
    }

    /// <summary>
    /// Element types a container can order lexicographically: either three-way comparable or
    /// at least less-than comparable.
    /// </summary>
    template <typename T>
    concept synth_three_way_comparable = std::three_way_comparable<T> || requires(const T &left, const T &right) {
        { left < right } -> std::convertible_to<bool>;
    };

    /// <summary>
    /// The standard library's exposition-only synth-three-way: uses operator<=> when the type has
    /// one and falls back to operator< (yielding a weak ordering) otherwise, exactly like
    /// std::vector's operator<=>.
    /// </summary>
    template <synth_three_way_comparable T>
    [[nodiscard]] constexpr auto synth_three_way(const T &left, const T &right) {
        if constexpr (std::three_way_comparable<T>) {
            return left <=> right;
        } else {
            if (left < right) {
                return std::weak_ordering::less;
            }
            if (right < left) {
                return std::weak_ordering::greater;
            }
            return std::weak_ordering::equivalent;
        }
    }

    template <synth_three_way_comparable T>
    using synth_three_way_result = decltype(synth_three_way(std::declval<const T &>(), std::declval<const T &>()));
}

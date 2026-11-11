#pragma once

#include "conjure/core/api.h"
#include "conjure/core/types.h"

#include <span>

// Stable least-significant-digit radix sort for unsigned keys with optional 32-bit payloads.
//
//   std::vector<u64> keys = ...;  std::vector<u32> values = ...;   // values[i] belongs to keys[i]
//   std::vector<u64> scratchKeys(keys.size());  std::vector<u32> scratchValues(values.size());
//   radix_sort_u64(keys, values, scratchKeys, scratchValues);      // sorted in place, allocation-free
//
// The sort runs one 8-bit pass per key byte (8 for u64, 4 for u32), skipping passes in which every
// key shares the same byte, so already sorted, all-equal and small-range inputs finish in one read
// sweep plus the passes that matter. Equal keys keep their relative order. The caller provides the
// scratch buffers so the sort never allocates; they must be at least as large as the inputs and
// must not overlap them (the sort ping-pongs between the two, so aliasing corrupts the data).

namespace conjure::core {

    /// <summary>
    /// Sorts 'keys' ascending and permutes 'values' alongside them. Stable. 'values' must have the
    /// same size as 'keys', both scratch spans must be at least that large and neither scratch span
    /// may overlap 'keys' or 'values' (all asserted; the input is left untouched on violation). On
    /// return the sorted data is in 'keys'/'values'; the scratch contents are unspecified.
    /// </summary>
    CONJURE_API void radix_sort_u64(
      std::span<u64> keys,
      std::span<u32> values,
      std::span<u64> scratchKeys,
      std::span<u32> scratchValues
    ) noexcept;

    /// <summary>
    /// Sorts 'keys' ascending. Stable. 'scratchKeys' must be at least as large as 'keys' and must
    /// not overlap it.
    /// </summary>
    CONJURE_API void radix_sort_u64(std::span<u64> keys, std::span<u64> scratchKeys) noexcept;

    /// <summary>
    /// 32-bit variant of radix_sort_u64 with payloads: four passes at most.
    /// </summary>
    CONJURE_API void radix_sort_u32(
      std::span<u32> keys,
      std::span<u32> values,
      std::span<u32> scratchKeys,
      std::span<u32> scratchValues
    ) noexcept;

    /// <summary>
    /// 32-bit variant of radix_sort_u64 for keys only.
    /// </summary>
    CONJURE_API void radix_sort_u32(std::span<u32> keys, std::span<u32> scratchKeys) noexcept;
}

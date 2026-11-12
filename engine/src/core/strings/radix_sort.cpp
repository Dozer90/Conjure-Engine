#include "conjure/core/radix_sort.h"

#include "conjure/core/assert.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <utility>

using namespace conjure::core;

/*===========================================================================*/
/*===========================================================================*/
// Implementation

namespace {
    constexpr conjure::usize RADIX_BITS = 8;
    constexpr conjure::usize BUCKET_COUNT = 1u << RADIX_BITS;

    template <typename KeyType>
    constexpr conjure::usize PASS_COUNT = sizeof(KeyType);

    template <typename KeyType>
    using histogram_set = std::array<std::array<conjure::usize, BUCKET_COUNT>, PASS_COUNT<KeyType>>;

    template <typename KeyType>
    [[nodiscard]] constexpr conjure::usize digit_of(KeyType key, conjure::usize pass) noexcept {
        return static_cast<conjure::usize>((key >> (pass * RADIX_BITS)) & (BUCKET_COUNT - 1));
    }

    // One read sweep fills the histogram of every pass at once, so skipping a trivial pass costs
    // nothing extra and the data is touched exactly once before the first scatter.
    template <typename KeyType>
    void build_histograms(std::span<const KeyType> keys, histogram_set<KeyType> &histograms) noexcept {
        for (std::array<conjure::usize, BUCKET_COUNT> &histogram : histograms) {
            histogram.fill(0);
        }
        for (const KeyType key : keys) {
            for (conjure::usize pass = 0; pass < PASS_COUNT<KeyType>; ++pass) {
                ++histograms[pass][digit_of(key, pass)];
            }
        }
    }

    // A pass is trivial when every key has the same digit: scattering would reproduce the input.
    [[nodiscard]] bool is_trivial_pass(
      const std::array<conjure::usize, BUCKET_COUNT> &histogram,
      conjure::usize count
    ) noexcept {
        for (const conjure::usize bucket : histogram) {
            if (bucket == count) {
                return true;
            }
        }
        return false;
    }

    // True when the two byte ranges share at least one byte. std::less gives a total order over
    // pointers into different arrays, which the built-in operators do not.
    [[nodiscard]] bool ranges_overlap(
      const void *first,
      conjure::usize firstBytes,
      const void *second,
      conjure::usize secondBytes
    ) noexcept {
        if (firstBytes == 0 || secondBytes == 0) {
            return false;
        }
        const std::byte *firstBegin = static_cast<const std::byte *>(first);
        const std::byte *secondBegin = static_cast<const std::byte *>(second);
        const std::less<const std::byte *> before = {};
        return before(firstBegin, secondBegin + secondBytes) && before(secondBegin, firstBegin + firstBytes);
    }

    // Compares bytes, so spans of different element types (u64 keys, u32 payloads) can be checked.
    template <typename FirstType, typename SecondType>
    [[nodiscard]] bool spans_overlap(std::span<FirstType> first, std::span<SecondType> second) noexcept {
        return ranges_overlap(first.data(), first.size_bytes(), second.data(), second.size_bytes());
    }

    // Exclusive prefix sum: each bucket becomes the index of its first element.
    void to_offsets(std::array<conjure::usize, BUCKET_COUNT> &histogram) noexcept {
        conjure::usize running = 0;
        for (conjure::usize &bucket : histogram) {
            const conjure::usize count = bucket;
            bucket = running;
            running += count;
        }
    }

    template <typename KeyType, bool WithValues>
    void sort_impl(
      std::span<KeyType> keys,
      std::span<conjure::u32> values,
      std::span<KeyType> scratchKeys,
      std::span<conjure::u32> scratchValues
    ) noexcept {
        const conjure::usize count = keys.size();
        CONJURE_ASSERT(
          scratchKeys.size() >= count,
          "radix sort scratch keys too small ({} < {})",
          scratchKeys.size(),
          count
        );
        if constexpr (WithValues) {
            CONJURE_ASSERT(values.size() == count, "radix sort values size {} != keys size {}", values.size(), count);
            CONJURE_ASSERT(
              scratchValues.size() >= count,
              "radix sort scratch values too small ({} < {})",
              scratchValues.size(),
              count
            );
            if (values.size() != count || scratchValues.size() < count) {
                return;
            }
        }
        if (count < 2 || scratchKeys.size() < count) {
            return;
        }

        // The passes ping-pong between the caller's buffers and the scratch and write keys and
        // values side by side, so aliasing between any two of the spans silently corrupts the data.
        bool aliased = spans_overlap(keys, scratchKeys);
        if constexpr (WithValues) {
            aliased = aliased || spans_overlap(values, scratchValues) || spans_overlap(keys, values) ||
                spans_overlap(keys, scratchValues) || spans_overlap(values, scratchKeys) ||
                spans_overlap(scratchKeys, scratchValues);
        }
        CONJURE_ASSERT(!aliased, "radix sort spans overlap each other");
        if (aliased) {
            return;
        }

        histogram_set<KeyType> histograms = {};
        build_histograms<KeyType>(keys, histograms);

        // Ping-pong between the caller's buffers and the scratch buffers; 'source' always holds
        // the current ordering.
        KeyType *sourceKeys = keys.data();
        KeyType *targetKeys = scratchKeys.data();
        [[maybe_unused]] conjure::u32 *sourceValues = values.data();
        [[maybe_unused]] conjure::u32 *targetValues = scratchValues.data();

        for (conjure::usize pass = 0; pass < PASS_COUNT<KeyType>; ++pass) {
            std::array<conjure::usize, BUCKET_COUNT> &offsets = histograms[pass];
            if (is_trivial_pass(offsets, count)) {
                continue;
            }
            to_offsets(offsets);

            for (conjure::usize index = 0; index < count; ++index) {
                const KeyType key = sourceKeys[index];
                const conjure::usize destination = offsets[digit_of(key, pass)]++;
                targetKeys[destination] = key;
                if constexpr (WithValues) {
                    targetValues[destination] = sourceValues[index];
                }
            }

            std::swap(sourceKeys, targetKeys);
            if constexpr (WithValues) {
                std::swap(sourceValues, targetValues);
            }
        }

        // An odd number of effective passes leaves the result in the scratch buffers.
        if (sourceKeys != keys.data()) {
            std::copy_n(sourceKeys, count, keys.data());
            if constexpr (WithValues) {
                std::copy_n(sourceValues, count, values.data());
            }
        }
    }
}


/*===========================================================================*/
/*===========================================================================*/
// Public functions

void conjure::core::radix_sort_u64(
  std::span<conjure::u64> keys,
  std::span<conjure::u32> values,
  std::span<conjure::u64> scratchKeys,
  std::span<conjure::u32> scratchValues
) noexcept {
    sort_impl<conjure::u64, true>(keys, values, scratchKeys, scratchValues);
}

void conjure::core::radix_sort_u64(std::span<conjure::u64> keys, std::span<conjure::u64> scratchKeys) noexcept {
    sort_impl<conjure::u64, false>(keys, {}, scratchKeys, {});
}

void conjure::core::radix_sort_u32(
  std::span<conjure::u32> keys,
  std::span<conjure::u32> values,
  std::span<conjure::u32> scratchKeys,
  std::span<conjure::u32> scratchValues
) noexcept {
    sort_impl<conjure::u32, true>(keys, values, scratchKeys, scratchValues);
}

void conjure::core::radix_sort_u32(std::span<conjure::u32> keys, std::span<conjure::u32> scratchKeys) noexcept {
    sort_impl<conjure::u32, false>(keys, {}, scratchKeys, {});
}

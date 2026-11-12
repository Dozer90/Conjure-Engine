#include "conjure/testing/doctest.h"

#include "conjure/testing/assert_capture.h"

#include "conjure/core/radix_sort.h"

#include <algorithm>
#include <limits>
#include <random>
#include <span>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::core;

namespace {
    constexpr u64 SEED = 0x5EED;

    template <typename KeyType>
    struct sort_fixture {
        std::vector<KeyType> keys = {};
        std::vector<u32> values = {};
        std::vector<KeyType> scratchKeys = {};
        std::vector<u32> scratchValues = {};

        explicit sort_fixture(std::vector<KeyType> initialKeys) :
            keys(std::move(initialKeys)),
            values(keys.size(), 0),
            scratchKeys(keys.size(), 0),
            scratchValues(keys.size(), 0) {
            for (usize index = 0; index < values.size(); ++index) {
                values[index] = static_cast<u32>(index);
            }
        }

        // The oracle: a stable sort of (key, original index) pairs.
        [[nodiscard]] std::vector<std::pair<KeyType, u32>> expected() const {
            std::vector<std::pair<KeyType, u32>> pairs = {};
            pairs.reserve(keys.size());
            for (usize index = 0; index < keys.size(); ++index) {
                pairs.emplace_back(keys[index], static_cast<u32>(index));
            }
            std::stable_sort(pairs.begin(), pairs.end(), [](const auto &left, const auto &right) {
                return left.first < right.first;
            });
            return pairs;
        }

        [[nodiscard]] bool matches(const std::vector<std::pair<KeyType, u32>> &oracle) const {
            if (oracle.size() != keys.size()) {
                return false;
            }
            for (usize index = 0; index < keys.size(); ++index) {
                if (keys[index] != oracle[index].first || values[index] != oracle[index].second) {
                    return false;
                }
            }
            return true;
        }
    };

    void sort_pairs(sort_fixture<u64> &fixture) {
        radix_sort_u64(fixture.keys, fixture.values, fixture.scratchKeys, fixture.scratchValues);
    }

    void sort_pairs(sort_fixture<u32> &fixture) {
        radix_sort_u32(fixture.keys, fixture.values, fixture.scratchKeys, fixture.scratchValues);
    }

    template <typename KeyType>
    std::vector<KeyType> random_keys(usize count, KeyType maximum) {
        std::mt19937_64 engine(SEED);
        std::uniform_int_distribution<KeyType> distribution(0, maximum);
        std::vector<KeyType> keys(count, 0);
        for (KeyType &key : keys) {
            key = distribution(engine);
        }
        return keys;
    }

    template <typename KeyType>
    void check_sorted_with_payload(std::vector<KeyType> keys) {
        sort_fixture<KeyType> fixture(std::move(keys));
        const auto oracle = fixture.expected();
        sort_pairs(fixture);
        CHECK(fixture.matches(oracle));
        CHECK(std::is_sorted(fixture.keys.begin(), fixture.keys.end()));
    }
}

TEST_SUITE("core/radix_sort") {

    TEST_CASE("radix_sort - random u64 keys with payload match a stable sort oracle") {
        check_sorted_with_payload(random_keys<u64>(5000, std::numeric_limits<u64>::max()));
    }

    TEST_CASE("radix_sort - random u32 keys with payload match a stable sort oracle") {
        check_sorted_with_payload(random_keys<u32>(5000, std::numeric_limits<u32>::max()));
    }

    TEST_CASE("radix_sort - duplicate keys keep their original relative order (stability)") {
        // Few distinct keys so almost every key has duplicates.
        check_sorted_with_payload(random_keys<u64>(4000, 15));
        check_sorted_with_payload(random_keys<u32>(4000, 3));

        sort_fixture<u64> fixture({5, 3, 5, 3, 5, 1});
        sort_pairs(fixture);
        CHECK(fixture.keys == std::vector<u64>{1, 3, 3, 5, 5, 5});
        CHECK(fixture.values == std::vector<u32>{5, 1, 3, 0, 2, 4});
    }

    TEST_CASE("radix_sort - already sorted input stays sorted and keeps its payload") {
        std::vector<u64> keys(1000, 0);
        for (usize index = 0; index < keys.size(); ++index) {
            keys[index] = static_cast<u64>(index) * 3;
        }
        check_sorted_with_payload(keys);
    }

    TEST_CASE("radix_sort - reverse sorted input is reversed") {
        std::vector<u64> keys(1000, 0);
        for (usize index = 0; index < keys.size(); ++index) {
            keys[index] = static_cast<u64>(keys.size() - index);
        }
        check_sorted_with_payload(keys);
    }

    TEST_CASE("radix_sort - all equal keys leave the payload order untouched") {
        sort_fixture<u64> fixture(std::vector<u64>(777, 0xABCDEFull));
        sort_pairs(fixture);
        for (usize index = 0; index < fixture.values.size(); ++index) {
            CHECK(fixture.values[index] == static_cast<u32>(index));
            CHECK(fixture.keys[index] == 0xABCDEFull);
        }
    }

    TEST_CASE("radix_sort - empty input is a no-op") {
        std::vector<u64> keys = {};
        std::vector<u32> values = {};
        std::vector<u64> scratchKeys = {};
        std::vector<u32> scratchValues = {};
        radix_sort_u64(keys, values, scratchKeys, scratchValues);
        radix_sort_u64(keys, scratchKeys);
        CHECK(keys.empty());
        CHECK(values.empty());
    }

    TEST_CASE("radix_sort - single element input is unchanged") {
        check_sorted_with_payload(std::vector<u64>{42});
        check_sorted_with_payload(std::vector<u32>{42});
    }

    TEST_CASE("radix_sort - two elements swap when needed") {
        check_sorted_with_payload(std::vector<u64>{2, 1});
        check_sorted_with_payload(std::vector<u64>{1, 2});
        check_sorted_with_payload(std::vector<u32>{~u32{0}, 0});
    }

    TEST_CASE("radix_sort - 100k random elements sort correctly") {
        check_sorted_with_payload(random_keys<u64>(100'000, std::numeric_limits<u64>::max()));
        check_sorted_with_payload(random_keys<u32>(100'000, std::numeric_limits<u32>::max()));
    }

    TEST_CASE("radix_sort - keys that differ in only one byte take one pass and copy back from scratch") {
        // Low byte only: a single effective pass, so the result lands in scratch and must be copied back.
        check_sorted_with_payload(random_keys<u64>(3000, 255));
        // Only the top byte varies.
        std::vector<u64> highOnly = random_keys<u64>(3000, 255);
        for (u64 &key : highOnly) {
            key <<= 56;
        }
        check_sorted_with_payload(highOnly);
        // Two middle bytes vary: two passes, result already in place.
        std::vector<u64> middle = random_keys<u64>(3000, 0xFFFF);
        for (u64 &key : middle) {
            key <<= 24;
        }
        check_sorted_with_payload(middle);
    }

    TEST_CASE("radix_sort - extreme key values sort to the ends") {
        check_sorted_with_payload(std::vector<u64>{
          std::numeric_limits<u64>::max(), 0, 1, std::numeric_limits<u64>::max() - 1, 0x8000'0000'0000'0000ull
        });
        check_sorted_with_payload(std::vector<u32>{std::numeric_limits<u32>::max(), 0, 0x8000'0000u, 1});
    }

    TEST_CASE("radix_sort - keys-only overloads sort without a payload") {
        std::vector<u64> keys64 = random_keys<u64>(2000, std::numeric_limits<u64>::max());
        std::vector<u64> expected64 = keys64;
        std::vector<u64> scratch64(keys64.size(), 0);
        std::sort(expected64.begin(), expected64.end());
        radix_sort_u64(keys64, scratch64);
        CHECK(keys64 == expected64);

        std::vector<u32> keys32 = random_keys<u32>(2000, std::numeric_limits<u32>::max());
        std::vector<u32> expected32 = keys32;
        std::vector<u32> scratch32(keys32.size(), 0);
        std::sort(expected32.begin(), expected32.end());
        radix_sort_u32(keys32, scratch32);
        CHECK(keys32 == expected32);

        std::vector<u64> single = {7};
        std::vector<u64> singleScratch = {0};
        radix_sort_u64(single, singleScratch);
        CHECK(single == std::vector<u64>{7});
    }

    TEST_CASE("radix_sort - oversized scratch buffers are accepted") {
        std::vector<u64> keys = {3, 1, 2};
        std::vector<u32> values = {0, 1, 2};
        std::vector<u64> scratchKeys(10, 0);
        std::vector<u32> scratchValues(10, 0);
        radix_sort_u64(keys, values, scratchKeys, scratchValues);
        CHECK(keys == std::vector<u64>{1, 2, 3});
        CHECK(values == std::vector<u32>{1, 2, 0});
    }

    TEST_CASE("radix_sort - undersized scratch asserts and leaves the input unchanged") {
        testing::expect_assert asserts = {};
        std::vector<u64> keys = {3, 1, 2};
        std::vector<u32> values = {0, 1, 2};
        std::vector<u64> smallScratchKeys(2, 0);
        std::vector<u32> smallScratchValues(2, 0);
        std::vector<u64> scratchKeys(3, 0);
        std::vector<u32> scratchValues(3, 0);

        radix_sort_u64(keys, values, smallScratchKeys, scratchValues);
        CHECK(asserts.count() == 1);
        CHECK(keys == std::vector<u64>{3, 1, 2});
        CHECK(values == std::vector<u32>{0, 1, 2});

        radix_sort_u64(keys, values, scratchKeys, smallScratchValues);
        CHECK(asserts.count() == 2);
        CHECK(keys == std::vector<u64>{3, 1, 2});

        radix_sort_u64(keys, smallScratchKeys);
        CHECK(asserts.count() == 3);
        CHECK(keys == std::vector<u64>{3, 1, 2});

        std::vector<u32> keys32 = {3, 1, 2};
        std::vector<u32> smallScratch32(1, 0);
        radix_sort_u32(keys32, smallScratch32);
        CHECK(asserts.count() == 4);
        CHECK(keys32 == std::vector<u32>{3, 1, 2});
    }

    TEST_CASE("radix_sort - a values span of the wrong size asserts and leaves the input unchanged") {
        testing::expect_assert asserts = {};
        std::vector<u64> keys = {3, 1, 2};
        std::vector<u32> values = {0, 1};
        std::vector<u64> scratchKeys(3, 0);
        std::vector<u32> scratchValues(3, 0);

        radix_sort_u64(keys, values, scratchKeys, scratchValues);
        CHECK(asserts.count() == 1);
        CHECK(keys == std::vector<u64>{3, 1, 2});
        CHECK(values == std::vector<u32>{0, 1});
    }

    TEST_CASE("radix_sort - scratch buffers that alias the input assert and leave the input unchanged") {
        testing::expect_assert asserts = {};
        std::vector<u64> keys = {3, 1, 2, 5, 4};
        std::vector<u32> values = {0, 1, 2, 3, 4};
        std::vector<u64> scratchKeys(5, 0);
        std::vector<u32> scratchValues(5, 0);

        // The same buffer passed as both input and scratch.
        radix_sort_u64(keys, keys);
        CHECK(asserts.count() == 1);
        CHECK(keys == std::vector<u64>{3, 1, 2, 5, 4});

        radix_sort_u64(keys, values, keys, scratchValues);
        CHECK(asserts.count() == 2);
        CHECK(keys == std::vector<u64>{3, 1, 2, 5, 4});
        CHECK(values == std::vector<u32>{0, 1, 2, 3, 4});

        radix_sort_u64(keys, values, scratchKeys, values);
        CHECK(asserts.count() == 3);
        CHECK(keys == std::vector<u64>{3, 1, 2, 5, 4});
        CHECK(values == std::vector<u32>{0, 1, 2, 3, 4});

        // Partially overlapping subspans of one buffer are aliasing too.
        std::vector<u64> buffer = {9, 8, 7, 6, 5, 4};
        const std::span<u64> front(buffer.data(), 4);
        const std::span<u64> back(buffer.data() + 2, 4);
        radix_sort_u64(front, back);
        CHECK(asserts.count() == 4);
        CHECK(buffer == std::vector<u64>{9, 8, 7, 6, 5, 4});

        std::vector<u32> keys32 = {3, 1, 2};
        radix_sort_u32(keys32, keys32);
        CHECK(asserts.count() == 5);
        CHECK(keys32 == std::vector<u32>{3, 1, 2});

        // Adjacent, non-overlapping halves of one buffer are fine.
        std::vector<u64> halves = {3, 1, 2, 0, 0, 0};
        const std::span<u64> data(halves.data(), 3);
        const std::span<u64> scratch(halves.data() + 3, 3);
        radix_sort_u64(data, scratch);
        CHECK(asserts.count() == 5);
        CHECK(halves[0] == 1);
        CHECK(halves[1] == 2);
        CHECK(halves[2] == 3);
    }

    TEST_CASE(
      "regression - sweep-2026-11-12 F204: any two overlapping radix_sort spans assert and leave the input unchanged"
    ) {
        testing::expect_assert asserts = {};
        std::vector<u32> keys = {3, 1, 2, 5, 4};
        std::vector<u32> values = {0, 1, 2, 3, 4};
        std::vector<u32> scratchKeys(5, 0);
        std::vector<u32> scratchValues(5, 0);
        const std::vector<u32> originalKeys = keys;
        const std::vector<u32> originalValues = values;

        SUBCASE("the scratch keys and scratch values are one buffer") {
            radix_sort_u32(keys, values, scratchKeys, scratchKeys);
        }

        SUBCASE("the scratch values are the keys") {
            radix_sort_u32(keys, values, scratchKeys, keys);
        }

        SUBCASE("the scratch keys are the values") {
            radix_sort_u32(keys, values, values, scratchValues);
        }

        SUBCASE("the keys and the values are one buffer") {
            radix_sort_u32(keys, keys, scratchKeys, scratchValues);
        }

        SUBCASE("the scratch keys and scratch values partially overlap") {
            std::vector<u32> shared(8, 0);
            const std::span<u32> sharedKeys(shared.data(), 5);
            const std::span<u32> sharedValues(shared.data() + 3, 5);
            radix_sort_u32(keys, values, sharedKeys, sharedValues);
        }

        CHECK(asserts.count() == 1);
        CHECK(keys == originalKeys);
        CHECK(values == originalValues);
    }

    TEST_CASE("radix_sort - sorting is deterministic") {
        const std::vector<u64> keys = random_keys<u64>(1000, std::numeric_limits<u64>::max());
        sort_fixture<u64> first(keys);
        sort_fixture<u64> second(keys);
        sort_pairs(first);
        sort_pairs(second);
        CHECK(first.keys == second.keys);
        CHECK(first.values == second.values);
    }
}

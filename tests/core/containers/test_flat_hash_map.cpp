#include "conjure/testing/doctest.h"

#include "conjure/testing/assert_capture.h"

#include "tracked_value.h"

#include "conjure/core/containers/flat_hash_map.h"

#include <algorithm>
#include <bit>
#include <initializer_list>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::core;

namespace {
    /// <summary>
    /// Every key lands in the same bucket: exercises long probe runs and backward shifting.
    /// </summary>
    struct constant_hash {
        [[nodiscard]] usize operator()(int) const noexcept {
            return 0;
        }
    };

    /// <summary>
    /// Every key hashes to Constant. mix_hash(2) homes at bucket 231 of 256 and mix_hash(3) at
    /// bucket 14 of 16, so long runs from those homes wrap around the end of the table.
    /// </summary>
    template <usize Constant>
    struct fixed_hash {
        [[nodiscard]] usize operator()(int) const noexcept {
            return Constant;
        }
    };

    /// <summary>
    /// A legal but weak hash with only 256 distinct values.
    /// </summary>
    struct byte_hash {
        [[nodiscard]] usize operator()(u64 key) const noexcept {
            return static_cast<usize>(key & 0xFF);
        }
    };

    /// <summary>
    /// Reserves for exactly the grow threshold of 'capacity' buckets, fills the map to that
    /// threshold and returns how many element addresses changed along the way (must be 0: no
    /// insertion below the threshold may rehash).
    /// </summary>
    usize fill_reserved_to_threshold(usize capacity) {
        const usize threshold = capacity / 8 * 7;
        flat_hash_map<u64, u64> map = {};
        map.reserve(threshold);
        REQUIRE(map.capacity() == capacity);

        std::mt19937_64 random(0x5EED);
        std::vector<u64> keys = {};
        std::vector<const u64 *> addresses = {};
        keys.reserve(threshold);
        addresses.reserve(threshold);
        for (usize index = 0; index < threshold; ++index) {
            const u64 key = random();
            const auto inserted = map.try_emplace(key, static_cast<u64>(index));
            REQUIRE(inserted.second);
            keys.push_back(key);
            addresses.push_back(&inserted.first->second);
        }
        REQUIRE(map.size() == threshold);
        REQUIRE(map.capacity() == capacity);
        REQUIRE(map.load_factor() == 0.875f);

        usize moved = 0;
        for (usize index = 0; index < threshold; ++index) {
            const auto found = map.find(keys[index]);
            if (found == map.end() || &found->second != addresses[index] || found->second != index) {
                ++moved;
            }
        }

        // The very next insertion crosses the threshold and is the first one allowed to rehash.
        REQUIRE(map.try_emplace(random(), 0).second);
        REQUIRE(map.capacity() == capacity * 2);
        return moved;
    }

    /// <summary>
    /// Identity hash: the finaliser must still spread strided keys over the buckets.
    /// </summary>
    struct identity_hash {
        [[nodiscard]] usize operator()(int value) const noexcept {
            return static_cast<usize>(value);
        }
    };

    template <typename Map>
    std::vector<int> sorted_keys(const Map &container) {
        std::vector<int> keys = {};
        for (const auto &entry : container) {
            keys.push_back(entry.first);
        }
        std::sort(keys.begin(), keys.end());
        return keys;
    }

    template <typename Set>
    std::vector<int> sorted_values(const Set &container) {
        std::vector<int> values(container.begin(), container.end());
        std::sort(values.begin(), values.end());
        return values;
    }
}

TEST_SUITE("core/flat_hash_map") {

    TEST_CASE("flat_hash_map - default constructed is empty and allocation free") {
        flat_hash_map<int, int> map = {};
        CHECK(map.empty());
        CHECK(map.size() == 0);
        CHECK(map.capacity() == 0);
        CHECK(map.load_factor() == 0.0f);
        CHECK(map.begin() == map.end());
        CHECK(map.find(1) == map.end());
        CHECK_FALSE(map.contains(1));
        CHECK(map.count(1) == 0);
        CHECK_FALSE(map.erase(1));
        map.clear();
        CHECK(map.capacity() == 0);
        CHECK(flat_hash_map<int, int>::MAX_LOAD_FACTOR == 0.875f);
    }

    TEST_CASE("flat_hash_map - insert, find and contains") {
        flat_hash_map<int, std::string> map = {};
        auto inserted = map.insert({1, "one"});
        CHECK(inserted.second);
        CHECK(inserted.first->first == 1);
        CHECK(inserted.first->second == "one");

        const std::pair<const int, std::string> two(2, "two");
        inserted = map.insert(two);
        CHECK(inserted.second);
        inserted = map.insert(std::make_pair(3, std::string("three")));
        CHECK(inserted.second);

        inserted = map.insert({1, "duplicate"});
        CHECK_FALSE(inserted.second);
        CHECK(inserted.first->second == "one");

        CHECK(map.size() == 3);
        REQUIRE(map.find(2) != map.end());
        CHECK(map.find(2)->second == "two");
        CHECK(map.find(4) == map.end());
        CHECK(map.contains(3));
        CHECK_FALSE(map.contains(0));
        CHECK(map.count(1) == 1);

        const flat_hash_map<int, std::string> &constMap = map;
        REQUIRE(constMap.find(3) != constMap.end());
        CHECK(constMap.find(3)->second == "three");
        map.find(3)->second = "drei";
        CHECK(constMap.at(3) == "drei");

        const std::vector<std::pair<int, std::string>> more = {{4, "four"}, {1, "ignored"}};
        map.insert(more.begin(), more.end());
        CHECK(map.size() == 4);
        CHECK(map.at(1) == "one");

        CHECK(map.emplace(5, "five").second);
        CHECK_FALSE(map.emplace(5, "again").second);
        CHECK(map.at(5) == "five");
    }

    TEST_CASE("flat_hash_map - try_emplace only constructs the value when the key is absent") {
        testing::tracked_value::reset();
        flat_hash_map<int, testing::tracked_value> map = {};
        auto first = map.try_emplace(1, 10);
        CHECK(first.second);
        CHECK(first.first->second.value() == 10);
        const i64 constructedAfterFirst = testing::tracked_value::constructed_count();

        auto second = map.try_emplace(1, 20);
        CHECK_FALSE(second.second);
        CHECK(second.first->second.value() == 10);
        CHECK(testing::tracked_value::constructed_count() == constructedAfterFirst);

        const int key = 2;
        CHECK(map.try_emplace(key, 30).second);
        CHECK(map.try_emplace(3).second);   // default constructed value
        CHECK(map.at(3).value() == 0);
        CHECK(testing::tracked_value::live_count() == 3);
    }

    TEST_CASE("flat_hash_map - insert_or_assign overwrites and operator[] default-inserts") {
        flat_hash_map<std::string, int> map = {};
        auto result = map.insert_or_assign("a", 1);
        CHECK(result.second);
        result = map.insert_or_assign("a", 2);
        CHECK_FALSE(result.second);
        CHECK(result.first->second == 2);
        const std::string key = "b";
        result = map.insert_or_assign(key, 3);
        CHECK(result.second);
        CHECK(map.at("b") == 3);

        CHECK(map["c"] == 0);
        map["c"] += 5;
        CHECK(map["c"] == 5);
        std::string movable = "d";
        map[std::move(movable)] = 4;
        CHECK(map.at("d") == 4);
        CHECK(map.size() == 4);
    }

    TEST_CASE("flat_hash_map - at returns the value of a present key") {
        // A missing key is fatal (assert + terminate, like result::value()), so only the success
        // path can run inside the test process.
        flat_hash_map<int, int> map = {{1, 1}};
        const flat_hash_map<int, int> &constMap = map;
        CHECK(map.at(1) == 1);
        CHECK(constMap.at(1) == 1);
        map.at(1) = 5;
        CHECK(constMap.at(1) == 5);
    }

    TEST_CASE("flat_hash_map - erase by key and by iterator") {
        flat_hash_map<int, int> map = {};
        for (int key = 0; key < 100; ++key) {
            map.try_emplace(key, key * 10);
        }
        const usize capacity = map.capacity();

        CHECK(map.erase(50));
        CHECK_FALSE(map.erase(50));
        CHECK_FALSE(map.contains(50));
        CHECK(map.size() == 99);
        CHECK(map.capacity() == capacity);   // erase never rehashes
        for (int key = 0; key < 100; ++key) {
            CAPTURE(key);
            if (key != 50) {
                REQUIRE(map.find(key) != map.end());
                CHECK(map.find(key)->second == key * 10);
            }
        }

        SUBCASE("erase(iterator) returns the following element and visits every element once") {
            usize visited = 0;
            for (auto iterator = map.begin(); iterator != map.end();) {
                ++visited;
                if (iterator->first % 2 == 0) {
                    iterator = map.erase(iterator);
                } else {
                    ++iterator;
                }
            }
            CHECK(visited == 99);
            CHECK(map.size() == 50);
            for (const auto &[key, value] : map) {
                CHECK(key % 2 == 1);
            }
            for (int key = 1; key < 100; key += 2) {
                CHECK(map.contains(key));
            }
        }

        SUBCASE("erasing every element while iterating leaves an empty, reusable map") {
            for (auto iterator = map.begin(); iterator != map.end();) {
                iterator = map.erase(iterator);
            }
            CHECK(map.empty());
            CHECK(map.begin() == map.end());
            map.try_emplace(7, 70);
            CHECK(map.at(7) == 70);
        }

        SUBCASE("erasing the end iterator asserts and returns end") {
            testing::expect_assert asserts = {};
            CHECK(map.erase(map.cend()) == map.end());
            CHECK(asserts.count() == 1);
            CHECK(map.size() == 99);
        }
    }

    TEST_CASE("flat_hash_map - load factor stays at or below 0.875 and capacity is a power of two") {
        flat_hash_map<int, int> map = {};
        for (int key = 0; key < 10000; ++key) {
            map.try_emplace(key, key);
            REQUIRE(map.load_factor() <= 0.875f);
            REQUIRE(std::has_single_bit(map.capacity()));
        }
        CHECK(map.capacity() == 16384);
        CHECK(map.size() == 10000);

        flat_hash_map<int, int> small = {};
        for (int key = 0; key < 7; ++key) {
            small.try_emplace(key, key);
        }
        CHECK(small.capacity() == 8);
        CHECK(small.load_factor() == 0.875f);
        small.try_emplace(7, 7);
        CHECK(small.capacity() == 16);
    }

    TEST_CASE("flat_hash_map - reserve prevents rehashing and keeps pointers stable") {
        flat_hash_map<int, int> map = {};
        map.reserve(1000);
        const usize capacity = map.capacity();
        CHECK(capacity >= 1000);
        CHECK(std::has_single_bit(capacity));

        std::vector<const int *> addresses = {};
        for (int key = 0; key < 1000; ++key) {
            addresses.push_back(&map.try_emplace(key, key).first->second);
        }
        CHECK(map.capacity() == capacity);
        for (int key = 0; key < 1000; ++key) {
            CAPTURE(key);
            REQUIRE(&map.at(key) == addresses[static_cast<usize>(key)]);
        }

        map.reserve(10);   // never shrinks
        CHECK(map.capacity() == capacity);

        flat_hash_map<int, int> presized(100);
        CHECK(presized.capacity() >= 100);
        CHECK(presized.empty());
    }

    TEST_CASE(
      "regression - sweep-2026-11-12 F215: flat_hash_map reserve(0) and zero-sized construction stay allocation free"
    ) {
        flat_hash_map<int, int> map = {};
        map.reserve(0);
        CHECK(map.capacity() == 0);

        flat_hash_map<int, int> sized(0);
        CHECK(sized.capacity() == 0);

        const std::initializer_list<flat_hash_map<int, int>::value_type> noPairs = {};
        flat_hash_map<int, int> fromList(noPairs);
        CHECK(fromList.capacity() == 0);
        CHECK(fromList.empty());

        flat_hash_set<int> set(0);
        CHECK(set.capacity() == 0);
        set.reserve(0);
        CHECK(set.capacity() == 0);

        const std::initializer_list<int> noKeys = {};
        flat_hash_set<int> setFromList(noKeys);
        CHECK(setFromList.capacity() == 0);

        // The first insertion still allocates the minimum table.
        map.try_emplace(1, 1);
        CHECK(map.capacity() == 8);
        sized.try_emplace(1, 1);
        CHECK(sized.capacity() == 8);
        set.insert(1);
        CHECK(set.capacity() == 8);

        // On an allocated table reserve(0) changes nothing either.
        map.reserve(0);
        CHECK(map.capacity() == 8);
        CHECK(map.size() == 1);
    }

    TEST_CASE("flat_hash_map - a reserved table filled exactly to its grow threshold never rehashes") {
        // Regression: probe runs longer than 255 used to force a rehash below the load threshold,
        // which routinely moved most elements of large tables despite reserve().
        SUBCASE("2^17 buckets") {
            CHECK(fill_reserved_to_threshold(usize{1} << 17) == 0);
        }

        SUBCASE("2^21 buckets") {
            CHECK(fill_reserved_to_threshold(usize{1} << 21) == 0);
        }
    }

    TEST_CASE("flat_hash_map - a hash with 256 distinct values stores 100,000 keys at the minimum capacity") {
        // Regression: more than 255 keys sharing a probe path used to double the table regardless
        // of load until allocation failed.
        flat_hash_map<u64, u64, byte_hash> map = {};
        for (u64 key = 0; key < 100000; ++key) {
            REQUIRE(map.try_emplace(key, key * 2).second);
        }
        CHECK(map.size() == 100000);
        CHECK(map.capacity() == 131072);   // smallest power of two with 0.875 * capacity >= 100000
        CHECK(map.load_factor() <= 0.875f);
        for (u64 key = 0; key < 100000; key += 7) {
            REQUIRE(map.at(key) == key * 2);
        }
        CHECK(map.find(100000) == map.end());
        CHECK_FALSE(map.contains(123456789));

        for (u64 key = 0; key < 100000; key += 3) {
            REQUIRE(map.erase(key));
        }
        CHECK(map.size() == 100000 - 33334);
        CHECK(map.capacity() == 131072);
        for (u64 key = 0; key < 100000; key += 11) {
            REQUIRE(map.contains(key) == (key % 3 != 0));
        }
        usize visited = 0;
        for (const auto &[key, value] : map) {
            REQUIRE(value == key * 2);
            ++visited;
        }
        CHECK(visited == map.size());
    }

    TEST_CASE("flat_hash_map - probe runs that wrap around the end of the table") {
        SUBCASE("lookups, erase(key) and iteration") {
            flat_hash_map<int, int, fixed_hash<2>> map = {};
            map.reserve(200);
            REQUIRE(map.capacity() == 256);
            for (int key = 0; key < 200; ++key) {
                REQUIRE(map.try_emplace(key, key).second);
            }
            CHECK(map.capacity() == 256);

            // The run starts at bucket 231 and continues at bucket 0: the lowest and highest
            // element addresses span the whole table.
            const int *lowest = &map.begin()->second;
            const int *highest = lowest;
            for (const auto &entry : map) {
                lowest = std::min(lowest, &entry.second);
                highest = std::max(highest, &entry.second);
            }
            const usize span = static_cast<usize>(
              reinterpret_cast<const char *>(highest) - reinterpret_cast<const char *>(lowest)
            ) / sizeof(std::pair<const int, int>) + 1;
            CHECK(span == 256);

            for (int key = 0; key < 200; key += 2) {
                REQUIRE(map.erase(key));
            }
            for (int key = 0; key < 200; ++key) {
                CAPTURE(key);
                REQUIRE(map.contains(key) == (key % 2 == 1));
                if (key % 2 == 1) {
                    REQUIRE(map.at(key) == key);
                }
            }
            CHECK(sorted_keys(map).size() == 100);
            CHECK(map.find(200) == map.end());
        }

        SUBCASE("erase(iterator) visits every element exactly once") {
            flat_hash_map<int, int, fixed_hash<3>> map = {};
            for (int key = 0; key < 10; ++key) {
                map.try_emplace(key, key);
            }
            REQUIRE(map.capacity() == 16);   // home bucket 14: the run occupies 14, 15, 0..7

            std::vector<int> visited = {};
            for (auto iterator = map.begin(); iterator != map.end();) {
                visited.push_back(iterator->first);
                if (iterator->first % 3 == 0) {
                    iterator = map.erase(iterator);
                } else {
                    ++iterator;
                }
            }
            std::sort(visited.begin(), visited.end());
            CHECK(visited == std::vector<int>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9});
            CHECK(sorted_keys(map) == std::vector<int>{1, 2, 4, 5, 7, 8});

            for (auto iterator = map.begin(); iterator != map.end();) {
                iterator = map.erase(iterator);
            }
            CHECK(map.empty());
            CHECK(map.begin() == map.end());
        }

        SUBCASE("random erase-while-iterating over wrapped runs against an oracle") {
            std::mt19937_64 random(0x5EED);
            for (int round = 0; round < 20; ++round) {
                flat_hash_map<int, int, fixed_hash<2>> map = {};
                std::unordered_set<int> oracle = {};
                const int count = 100 + round * 5;
                for (int key = 0; key < count; ++key) {
                    map.try_emplace(key, key);
                    oracle.insert(key);
                }
                std::vector<int> visited = {};
                for (auto iterator = map.begin(); iterator != map.end();) {
                    visited.push_back(iterator->first);
                    if (random() % 2 == 0) {
                        oracle.erase(iterator->first);
                        iterator = map.erase(iterator);
                    } else {
                        ++iterator;
                    }
                }
                std::sort(visited.begin(), visited.end());
                REQUIRE(visited.size() == static_cast<usize>(count));
                REQUIRE(std::unique(visited.begin(), visited.end()) == visited.end());
                REQUIRE(map.size() == oracle.size());
                for (const int key : oracle) {
                    REQUIRE(map.contains(key));
                }
            }
        }
    }

    TEST_CASE("flat_hash_map - erase moves later elements of the same probe run and nothing else") {
        flat_hash_map<int, int, constant_hash> map = {};
        for (int key = 0; key < 10; ++key) {
            map.try_emplace(key, key);
        }
        std::vector<const int *> before = {};
        for (int key = 0; key < 10; ++key) {
            before.push_back(&map.at(key));
        }

        // All keys share one run in insertion order; erasing key 3 shifts 4..9 one slot back.
        REQUIRE(map.erase(3));
        for (int key = 0; key < 10; ++key) {
            CAPTURE(key);
            if (key < 3) {
                CHECK(&map.at(key) == before[static_cast<usize>(key)]);
            } else if (key > 3) {
                CHECK(&map.at(key) == before[static_cast<usize>(key) - 1]);
            }
        }

        // Elements of other runs do not move: a well-spread table keeps every other address.
        flat_hash_map<int, int> spread = {};
        spread.reserve(64);
        for (int key = 0; key < 32; ++key) {
            spread.try_emplace(key, key);
        }
        std::vector<const int *> spreadBefore = {};
        for (int key = 0; key < 32; ++key) {
            spreadBefore.push_back(&spread.at(key));
        }
        REQUIRE(spread.erase(5));
        usize moved = 0;
        for (int key = 0; key < 32; ++key) {
            if (key != 5 && &spread.at(key) != spreadBefore[static_cast<usize>(key)]) {
                ++moved;
            }
        }
        CHECK(moved < 32 - 1);   // only followers of key 5 in its run may have moved
        CHECK(spread.size() == 31);
    }

    TEST_CASE("flat_hash_map - copies iterate in the same order as the source") {
        std::mt19937_64 random(0x5EED);
        flat_hash_map<u64, int> source = {};
        for (int step = 0; step < 5000; ++step) {
            const u64 key = random() % 3000;
            if (random() % 3 == 0) {
                source.erase(key);
            } else {
                source.insert_or_assign(key, step);
            }
        }
        REQUIRE(source.size() > 100);

        std::vector<u64> sourceOrder = {};
        for (const auto &[key, value] : source) {
            sourceOrder.push_back(key);
        }

        const flat_hash_map<u64, int> copy = source;
        std::vector<u64> copyOrder = {};
        for (const auto &[key, value] : copy) {
            copyOrder.push_back(key);
        }
        CHECK(copyOrder == sourceOrder);
        CHECK(copy.capacity() == source.capacity());

        flat_hash_map<u64, int> assigned = {{1, 1}};
        assigned = source;
        std::vector<u64> assignedOrder = {};
        for (const auto &[key, value] : assigned) {
            assignedOrder.push_back(key);
        }
        CHECK(assignedOrder == sourceOrder);

        // The copy is independent: erasing from it keeps the source order.
        assigned.erase(sourceOrder.front());
        std::vector<u64> afterErase = {};
        for (const auto &[key, value] : source) {
            afterErase.push_back(key);
        }
        CHECK(afterErase == sourceOrder);
    }

    TEST_CASE("flat_hash_map - over-aligned values are stored aligned") {
        flat_hash_map<int, testing::wide_value> map = {};
        for (int key = 0; key < 100; ++key) {
            map.try_emplace(key, testing::wide_value{static_cast<u64>(key)});
        }
        for (const auto &entry : map) {
            REQUIRE(testing::is_aligned(&entry.second, alignof(testing::wide_value)));
            REQUIRE(entry.second.payload == static_cast<u64>(entry.first));
        }
        REQUIRE(testing::is_aligned(&map.at(42), alignof(testing::wide_value)));
        REQUIRE(map.size() == 100);
    }

    TEST_CASE("flat_hash_map - iteration yields pair<const K, V> with structured bindings") {
        flat_hash_map<int, int> map = {{1, 10}, {2, 20}, {3, 30}};
        static_assert(std::is_same_v<flat_hash_map<int, int>::value_type, std::pair<const int, int>>);

        int keySum = 0;
        int valueSum = 0;
        for (auto &[key, value] : map) {
            static_assert(std::is_same_v<decltype(key), const int>);
            keySum += key;
            value += 1;
            valueSum += value;
        }
        CHECK(keySum == 6);
        CHECK(valueSum == 63);

        const flat_hash_map<int, int> &constMap = map;
        usize count = 0;
        for (const auto &[key, value] : constMap) {
            CHECK(constMap.at(key) == value);
            ++count;
        }
        CHECK(count == 3);
        CHECK(std::distance(map.cbegin(), map.cend()) == 3);

        flat_hash_map<int, int>::const_iterator converted = map.begin();
        CHECK(converted == map.cbegin());
        auto copy = map.begin();
        copy++;
        ++copy;
        ++copy;
        CHECK(copy == map.end());
        CHECK((*map.begin()).first == map.begin()->first);
    }

    TEST_CASE("flat_hash_map - clear destroys every element and keeps the table") {
        testing::tracked_value::reset();
        {
            testing::leak_check leaks = {};
            flat_hash_map<int, testing::tracked_value> map = {};
            for (int key = 0; key < 100; ++key) {
                map.try_emplace(key, key);
            }
            const usize capacity = map.capacity();
            map.clear();
            CHECK(map.empty());
            CHECK(map.capacity() == capacity);
            CHECK(testing::tracked_value::live_count() == 0);
            CHECK(map.find(1) == map.end());
            CHECK(map.begin() == map.end());

            map.try_emplace(1, 1);
            CHECK(map.size() == 1);
            CHECK(map.at(1).value() == 1);
        }
        CHECK(testing::tracked_value::live_count() == 0);
    }

    TEST_CASE("flat_hash_map - copy, move, swap and equality") {
        flat_hash_map<int, std::string> source = {{1, "a"}, {2, "b"}, {3, "c"}};
        flat_hash_map<int, std::string> copy = source;
        CHECK(copy == source);
        CHECK(copy.size() == 3);
        CHECK(copy.capacity() == source.capacity());
        copy[1] = "changed";
        CHECK(copy != source);
        CHECK(source.at(1) == "a");

        flat_hash_map<int, std::string> moved = std::move(source);
        CHECK(moved.size() == 3);
        CHECK(moved.at(3) == "c");
        CHECK(source.empty());
        CHECK(source.capacity() == 0);
        source.insert({9, "reused"});
        CHECK(source.at(9) == "reused");

        flat_hash_map<int, std::string> assigned = {{100, "x"}};
        assigned = copy;
        CHECK(assigned == copy);
        assigned = std::move(moved);
        CHECK(assigned.at(1) == "a");
        CHECK(assigned.size() == 3);
        flat_hash_map<int, std::string> &self = assigned;
        assigned = self;   // self assignment is harmless
        CHECK(assigned.size() == 3);

        flat_hash_map<int, std::string> empty = {};
        flat_hash_map<int, std::string> emptyCopy = empty;
        CHECK(emptyCopy.capacity() == 0);
        CHECK(emptyCopy == empty);

        flat_hash_map<int, std::string> different = {{1, "a"}, {2, "b"}, {4, "c"}};
        CHECK(assigned != different);
        CHECK(assigned != flat_hash_map<int, std::string>{{1, "a"}});

        assigned.swap(different);
        CHECK(assigned.contains(4));
        CHECK(different.contains(3));
        swap(assigned, different);
        CHECK(assigned.contains(3));
    }

    TEST_CASE("flat_hash_map - initializer list keeps the first of duplicate keys") {
        const flat_hash_map<int, std::string> map = {{1, "a"}, {2, "b"}, {1, "duplicate"}};
        CHECK(map.size() == 2);
        CHECK(map.at(1) == "a");
    }

    TEST_CASE("flat_hash_map - a constant hash degrades gracefully to a linear run") {
        flat_hash_map<int, int, constant_hash> map = {};
        for (int key = 0; key < 200; ++key) {
            REQUIRE(map.try_emplace(key, key).second);
        }
        CHECK(map.size() == 200);
        for (int key = 0; key < 200; ++key) {
            CAPTURE(key);
            REQUIRE(map.find(key) != map.end());
            REQUIRE(map.at(key) == key);
        }
        CHECK(map.find(200) == map.end());

        // Backward-shift deletion keeps the rest of the run reachable.
        for (int key = 0; key < 200; key += 3) {
            REQUIRE(map.erase(key));
        }
        for (int key = 0; key < 200; ++key) {
            CAPTURE(key);
            REQUIRE(map.contains(key) == (key % 3 != 0));
        }
        CHECK(sorted_keys(map).size() == map.size());

        for (int key = 0; key < 200; ++key) {
            map.insert_or_assign(key, -key);
        }
        CHECK(map.size() == 200);
        CHECK(map.at(199) == -199);
        map.clear();
        CHECK(map.empty());
    }

    TEST_CASE("flat_hash_map - strided keys with an identity hash still spread over the table") {
        flat_hash_map<int, int, identity_hash> map = {};
        for (int key = 0; key < 4096; ++key) {
            map.try_emplace(key * 4096, key);
        }
        CHECK(map.size() == 4096);
        for (int key = 0; key < 4096; ++key) {
            REQUIRE(map.at(key * 4096) == key);
        }
    }

    TEST_CASE("flat_hash_map - string keys and heavy churn") {
        flat_hash_map<std::string, int> map = {};
        for (int round = 0; round < 3; ++round) {
            for (int key = 0; key < 5000; ++key) {
                map.insert_or_assign("key-" + std::to_string(key), key + round);
            }
            for (int key = 0; key < 5000; key += 2) {
                REQUIRE(map.erase("key-" + std::to_string(key)));
            }
            CHECK(map.size() == 2500);
            for (int key = 1; key < 5000; key += 2) {
                REQUIRE(map.at("key-" + std::to_string(key)) == key + round);
            }
        }
        CHECK_FALSE(map.contains("missing"));
    }

    TEST_CASE("flat_hash_map - non-trivial elements are never leaked or double destroyed") {
        testing::tracked_value::reset();
        std::mt19937_64 random(0x5EED);
        {
            testing::leak_check leaks = {};
            flat_hash_map<int, testing::tracked_value> map = {};
            std::unordered_map<int, int> oracle = {};
            for (int step = 0; step < 20000; ++step) {
                const int key = static_cast<int>(random() % 2000);
                const int value = static_cast<int>(random() % 100000);
                if (random() % 3 != 0) {
                    map.insert_or_assign(key, testing::tracked_value(value));
                    oracle.insert_or_assign(key, value);
                } else {
                    CHECK(map.erase(key) == (oracle.erase(key) == 1));
                }
                REQUIRE(testing::tracked_value::live_count() == static_cast<i64>(oracle.size()));
            }
            flat_hash_map<int, testing::tracked_value> copy = map;
            CHECK(testing::tracked_value::live_count() == static_cast<i64>(oracle.size() * 2));
            for (const auto &[key, value] : copy) {
                REQUIRE(value.value() == oracle.at(key));
            }
        }
        CHECK(testing::tracked_value::live_count() == 0);
        CHECK(testing::tracked_value::constructed_count() == testing::tracked_value::destroyed_count());
    }

    TEST_CASE("flat_hash_map - move-only values") {
        flat_hash_map<int, std::unique_ptr<int>> map = {};
        map.try_emplace(1, std::make_unique<int>(10));
        map.insert({2, std::make_unique<int>(20)});
        map[3] = std::make_unique<int>(30);
        for (int key = 4; key < 100; ++key) {
            map.try_emplace(key, std::make_unique<int>(key * 10));   // forces rehashes
        }
        REQUIRE(map.size() == 99);
        for (int key = 1; key < 100; ++key) {
            CAPTURE(key);
            REQUIRE(map.at(key) != nullptr);
            REQUIRE(*map.at(key) == key * 10);
        }
        flat_hash_map<int, std::unique_ptr<int>> moved = std::move(map);
        CHECK(*moved.at(50) == 500);
    }

    TEST_CASE("flat_hash_map - property test against a std::unordered_map oracle") {
        std::mt19937_64 random(0x5EED);
        flat_hash_map<u64, u64> map = {};
        std::unordered_map<u64, u64> oracle = {};

        for (int step = 0; step < 50000; ++step) {
            const u64 roll = random() % 100;
            const u64 key = random() % 4000;
            const u64 value = random();
            if (roll < 35) {
                CHECK(map.insert({key, value}).second == oracle.insert({key, value}).second);
            } else if (roll < 50) {
                map.insert_or_assign(key, value);
                oracle.insert_or_assign(key, value);
            } else if (roll < 60) {
                CHECK(map.try_emplace(key, value).second == oracle.try_emplace(key, value).second);
            } else if (roll < 85) {
                CHECK(map.erase(key) == (oracle.erase(key) == 1));
            } else if (roll < 97) {
                const auto found = map.find(key);
                const auto expected = oracle.find(key);
                REQUIRE((found == map.end()) == (expected == oracle.end()));
                if (found != map.end()) {
                    REQUIRE(found->second == expected->second);
                }
            } else if (roll == 97) {
                map[key] = value;
                oracle[key] = value;
            } else if (roll == 98 && step % 5000 == 0) {
                map.clear();
                oracle.clear();
            } else if (roll == 99 && step % 5000 == 0) {
                flat_hash_map<u64, u64> copy = map;
                map = std::move(copy);
            }

            REQUIRE(map.size() == oracle.size());
            REQUIRE(map.load_factor() <= 0.875f);
            if (step % 1000 == 0) {
                usize visited = 0;
                for (const auto &[mapKey, mapValue] : map) {
                    const auto expected = oracle.find(mapKey);
                    REQUIRE(expected != oracle.end());
                    REQUIRE(expected->second == mapValue);
                    ++visited;
                }
                REQUIRE(visited == oracle.size());
                for (const auto &[oracleKey, oracleValue] : oracle) {
                    REQUIRE(map.at(oracleKey) == oracleValue);
                }
            }
        }
    }

    TEST_CASE("flat_hash_map - concurrent readers see a consistent map") {
        flat_hash_map<int, int> map = {};
        for (int key = 0; key < 2000; ++key) {
            map.try_emplace(key, key * 2);
        }
        for (int key = 0; key < 2000; key += 2) {
            REQUIRE(map.erase(key));
        }

        const flat_hash_map<int, int> &shared = map;
        std::vector<std::thread> threads = {};
        std::vector<int> failures(8, 0);
        for (usize threadIndex = 0; threadIndex < 8; ++threadIndex) {
            threads.emplace_back([&shared, &failures, threadIndex] {
                for (int repeat = 0; repeat < 20; ++repeat) {
                    for (int key = 0; key < 2000; ++key) {
                        const auto found = shared.find(key);
                        const bool expectedPresent = key % 2 == 1;
                        if ((found != shared.end()) != expectedPresent) {
                            ++failures[threadIndex];
                        } else if (found != shared.end() && found->second != key * 2) {
                            ++failures[threadIndex];
                        }
                    }
                    usize count = 0;
                    for (const auto &entry : shared) {
                        if (entry.first % 2 == 1) {
                            ++count;
                        }
                    }
                    if (count != 1000) {
                        ++failures[threadIndex];
                    }
                }
            });
        }
        for (std::thread &thread : threads) {
            thread.join();
        }
        for (const int failure : failures) {
            CHECK(failure == 0);
        }
    }

    TEST_CASE("flat_hash_set - insert, contains, erase and iteration") {
        flat_hash_set<int> set = {};
        CHECK(set.empty());
        CHECK(set.capacity() == 0);
        CHECK(set.begin() == set.end());
        CHECK(flat_hash_set<int>::MAX_LOAD_FACTOR == 0.875f);

        CHECK(set.insert(3).second);
        CHECK(set.insert(1).second);
        CHECK_FALSE(set.insert(3).second);
        CHECK(set.insert(3).first != set.end());
        CHECK(*set.insert(3).first == 3);
        int movable = 2;
        CHECK(set.insert(std::move(movable)).second);
        CHECK(set.emplace(4).second);
        CHECK_FALSE(set.emplace(4).second);
        const std::vector<int> more = {5, 6, 1};
        set.insert(more.begin(), more.end());

        CHECK(set.size() == 6);
        CHECK(set.contains(1));
        CHECK(set.count(1) == 1);
        CHECK_FALSE(set.contains(7));
        CHECK(set.count(7) == 0);
        REQUIRE(set.find(2) != set.end());
        CHECK(*set.find(2) == 2);
        CHECK(set.find(9) == set.end());
        CHECK(sorted_values(set) == std::vector<int>{1, 2, 3, 4, 5, 6});
        static_assert(std::is_same_v<decltype(*set.begin()), const int &>);

        CHECK(set.erase(3));
        CHECK_FALSE(set.erase(3));
        CHECK(set.size() == 5);
        for (auto iterator = set.begin(); iterator != set.end();) {
            if (*iterator % 2 == 0) {
                iterator = set.erase(iterator);
            } else {
                ++iterator;
            }
        }
        CHECK(sorted_values(set) == std::vector<int>{1, 5});
        CHECK(set.load_factor() > 0.0f);

        set.reserve(100);
        CHECK(set.capacity() >= 100);
        set.clear();
        CHECK(set.empty());
        CHECK(set.capacity() >= 100);
    }

    TEST_CASE("flat_hash_set - initializer list, copy, move, swap and equality") {
        const flat_hash_set<std::string> set = {"a", "b", "a"};
        CHECK(set.size() == 2);
        flat_hash_set<std::string> copy = set;
        CHECK(copy == set);
        copy.insert("c");
        CHECK(copy != set);
        flat_hash_set<std::string> moved = std::move(copy);
        CHECK(moved.size() == 3);
        CHECK(copy.empty());

        flat_hash_set<std::string> assigned = {};
        assigned = set;
        CHECK(assigned == set);
        assigned = std::move(moved);
        CHECK(assigned.contains("c"));
        assigned.swap(copy);
        CHECK(assigned.empty());
        CHECK(copy.size() == 3);
        swap(assigned, copy);
        CHECK(copy.empty());
        CHECK(assigned.size() == 3);

        flat_hash_set<std::string> presized(64);
        CHECK(presized.capacity() >= 64);
        CHECK(presized.empty());
    }

    TEST_CASE("flat_hash_set - property test against a std::unordered_set oracle") {
        std::mt19937_64 random(0x5EED);
        flat_hash_set<int> set = {};
        std::unordered_set<int> oracle = {};
        for (int step = 0; step < 30000; ++step) {
            const int value = static_cast<int>(random() % 3000);
            if (random() % 2 == 0) {
                CHECK(set.insert(value).second == oracle.insert(value).second);
            } else {
                CHECK(set.erase(value) == (oracle.erase(value) == 1));
            }
            REQUIRE(set.size() == oracle.size());
            if (step % 1000 == 0) {
                for (const int element : set) {
                    REQUIRE(oracle.contains(element));
                }
                for (const int element : oracle) {
                    REQUIRE(set.contains(element));
                }
            }
        }
    }
}

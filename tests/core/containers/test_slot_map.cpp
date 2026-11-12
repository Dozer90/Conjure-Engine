#include "conjure/testing/doctest.h"

#include "conjure/testing/assert_capture.h"

#include "tracked_value.h"

#include "conjure/core/containers/slot_key_format.h"
#include "conjure/core/containers/slot_map.h"

#include <algorithm>
#include <format>
#include <iterator>
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
    /// A key with narrow fields: 255 slots, generations wrap after 255 reuses.
    /// </summary>
    struct narrow_key {
        u8 index = 0;
        u8 generation = 0;

        [[nodiscard]] constexpr bool operator==(const narrow_key &) const noexcept = default;
    };

    static_assert(slot_key_type<narrow_key>);

    struct not_a_key {
        int index = 0;
    };

    static_assert(!slot_key_type<not_a_key>);

    /// <summary>
    /// Fields declared in the other order: keys must be built member by member, not positionally.
    /// </summary>
    struct swapped_key {
        u16 generation = 0;
        u16 index = 0;

        [[nodiscard]] constexpr bool operator==(const swapped_key &) const noexcept = default;
    };

    static_assert(slot_key_type<swapped_key>);

    /// <summary>
    /// Fields wider than the u32 generation a slot stores would truncate silently; rejected.
    /// </summary>
    struct wide_key {
        u64 index = 0;
        u64 generation = 0;
    };

    static_assert(!slot_key_type<wide_key>);
}

TEST_SUITE("core/slot_map") {

    TEST_CASE("slot_key - null key is all zero, invalid, and round-trips through u64") {
        constexpr slot_key NULL_KEY = slot_key::null();
        static_assert(NULL_KEY.index == 0 && NULL_KEY.generation == 0);
        static_assert(!NULL_KEY.is_valid());
        static_assert(NULL_KEY == slot_key{});
        static_assert(NULL_KEY.to_u64() == 0);

        constexpr slot_key KEY{0x12345678u, 0x9ABCDEF0u};
        static_assert(KEY.is_valid());
        static_assert(KEY.to_u64() == 0x9ABCDEF012345678ull);
        static_assert(slot_key::from_u64(KEY.to_u64()) == KEY);
        static_assert(slot_key::from_u64(0) == NULL_KEY);

        CHECK(sizeof(slot_key) == 8);
        CHECK(slot_key{5, 0}.is_valid() == false);
        CHECK(slot_key{0, 1}.is_valid());
    }

    TEST_CASE("slot_key - to_string and parse round trip, parse rejects malformed text") {
        CHECK(to_string(slot_key{12, 3}) == "12:3");
        CHECK(to_string(slot_key::null()) == "0:0");
        CHECK(std::format("{}", slot_key{7, 2}) == "7:2");

        CHECK(parse_slot_key("12:3") == slot_key{12, 3});
        CHECK(parse_slot_key("0:0") == slot_key::null());
        CHECK(parse_slot_key("4294967295:4294967295") == slot_key{0xFFFFFFFFu, 0xFFFFFFFFu});

        CHECK_FALSE(parse_slot_key("").has_value());
        CHECK_FALSE(parse_slot_key("12").has_value());
        CHECK_FALSE(parse_slot_key(":3").has_value());
        CHECK_FALSE(parse_slot_key("12:").has_value());
        CHECK_FALSE(parse_slot_key("a:3").has_value());
        CHECK_FALSE(parse_slot_key("12:3:4").has_value());
        CHECK_FALSE(parse_slot_key("-1:3").has_value());
        CHECK_FALSE(parse_slot_key("4294967296:1").has_value());
        CHECK_FALSE(parse_slot_key("12 :3").has_value());
    }

    TEST_CASE("slot_key - comparison and hashing") {
        const slot_key low{1, 1};
        const slot_key high{1, 2};
        CHECK(low < high);
        CHECK(slot_key{0, 5} < slot_key{1, 0});
        CHECK(std::hash<slot_key>()(low) != std::hash<slot_key>()(high));
        CHECK(std::hash<slot_key>()(low) == low.to_u64());

        std::unordered_set<slot_key> keys = {low, high, low};
        CHECK(keys.size() == 2);
    }

    TEST_CASE("slot_map - default constructed map is empty") {
        slot_map<int> map = {};
        CHECK(map.empty());
        CHECK(map.size() == 0);
        CHECK(map.capacity() == 0);
        CHECK(map.slot_count() == 0);
        CHECK(map.free_slot_count() == 0);
        CHECK(map.retired_slot_count() == 0);
        CHECK(map.begin() == map.end());
        CHECK(map.find(slot_key{}) == nullptr);
        CHECK(map.find(slot_key{0, 1}) == nullptr);
        CHECK_FALSE(map.contains(slot_key{0, 1}));
        CHECK(map.entries().empty());
    }

    TEST_CASE("slot_map - insert returns a valid key that finds the element") {
        slot_map<std::string> map = {};
        const slot_key first = map.insert("one");
        std::string two = "two";
        const slot_key second = map.insert(two);

        REQUIRE(first.is_valid());
        REQUIRE(second.is_valid());
        CHECK(first != second);
        CHECK(map.size() == 2);
        REQUIRE(map.find(first) != nullptr);
        REQUIRE(map.find(second) != nullptr);
        CHECK(*map.find(first) == "one");
        CHECK(*map.find(second) == "two");
        CHECK(map.contains(first));
        CHECK(map.contains(second));

        const slot_map<std::string> &constMap = map;
        REQUIRE(constMap.find(first) != nullptr);
        CHECK(*constMap.find(first) == "one");

        *map.find(first) = "uno";
        CHECK(*constMap.find(first) == "uno");
    }

    TEST_CASE("slot_map - emplace constructs the element in place") {
        slot_map<std::pair<int, std::string>> map = {};
        const slot_key key = map.emplace(7, "seven");
        REQUIRE(map.find(key) != nullptr);
        CHECK(map.find(key)->first == 7);
        CHECK(map.find(key)->second == "seven");
    }

    TEST_CASE("slot_map - generation 0 is never issued and indices start at 0") {
        slot_map<int> map = {};
        const slot_key first = map.insert(1);
        const slot_key second = map.insert(2);
        CHECK(first.index == 0);
        CHECK(first.generation == 1);
        CHECK(second.index == 1);
        CHECK(second.generation == 1);
    }

    TEST_CASE("slot_map - erase invalidates the handle generation") {
        slot_map<int> map = {};
        const slot_key key = map.insert(42);
        REQUIRE(map.erase(key));

        CHECK(map.size() == 0);
        CHECK(map.find(key) == nullptr);
        CHECK_FALSE(map.contains(key));
        CHECK_FALSE(map.erase(key));
        CHECK(map.free_slot_count() == 1);

        SUBCASE("the slot is reused with a higher generation and the stale key stays rejected") {
            const slot_key reused = map.insert(43);
            CHECK(reused.index == key.index);
            CHECK(reused.generation == key.generation + 1);
            CHECK(map.find(key) == nullptr);
            REQUIRE(map.find(reused) != nullptr);
            CHECK(*map.find(reused) == 43);
            CHECK(map.free_slot_count() == 0);
            CHECK(map.slot_count() == 1);
        }

        SUBCASE("a forged key with the next generation does not find a free slot") {
            const slot_key forged{key.index, key.generation + 1};
            CHECK(map.find(forged) == nullptr);
            CHECK_FALSE(map.erase(forged));
        }
    }

    TEST_CASE("slot_map - erase of null, stale, out-of-range and wrong-generation keys returns false") {
        slot_map<int> map = {};
        const slot_key key = map.insert(1);

        CHECK_FALSE(map.erase(slot_key::null()));
        CHECK_FALSE(map.erase(slot_key{key.index, key.generation + 1}));
        CHECK_FALSE(map.erase(slot_key{key.index + 1, key.generation}));
        CHECK_FALSE(map.erase(slot_key{1000000, 1}));
        CHECK(map.find(slot_key{1000000, 1}) == nullptr);
        CHECK(map.size() == 1);
        CHECK(map.contains(key));
    }

    TEST_CASE("slot_map - swap-remove keeps the storage dense and other keys valid") {
        slot_map<int> map = {};
        std::vector<slot_key> keys = {};
        for (int value = 0; value < 6; ++value) {
            keys.push_back(map.insert(value));
        }

        REQUIRE(map.erase(keys[1]));
        CHECK(map.size() == 5);

        // The last element moved into the hole; its key must still resolve.
        CHECK(map.entries()[1].value() == 5);
        CHECK(map.entries()[1].key() == keys[5]);
        REQUIRE(map.find(keys[5]) != nullptr);
        CHECK(*map.find(keys[5]) == 5);
        CHECK(map.key_at(1) == keys[5]);

        for (int value = 0; value < 6; ++value) {
            CAPTURE(value);
            if (value == 1) {
                CHECK(map.find(keys[static_cast<usize>(value)]) == nullptr);
            } else {
                REQUIRE(map.find(keys[static_cast<usize>(value)]) != nullptr);
                CHECK(*map.find(keys[static_cast<usize>(value)]) == value);
            }
        }

        REQUIRE(map.erase(keys[5]));   // erasing the (now first-moved) element again works
        CHECK(map.find(keys[5]) == nullptr);
        CHECK(map.size() == 4);
        CHECK(map.entries().size() == map.size());

        testing::expect_assert asserts = {};
        CHECK(map.key_at(4) == slot_key::null());
        CHECK(asserts.count() == 1);
    }

    TEST_CASE("slot_map - iteration visits live elements in storage order with structured bindings") {
        slot_map<int> map = {};
        const slot_key a = map.insert(10);
        const slot_key b = map.insert(20);
        const slot_key c = map.insert(30);

        std::vector<slot_key> seenKeys = {};
        std::vector<int> seenValues = {};
        for (auto &[key, value] : map) {
            static_assert(std::is_same_v<decltype(key), const slot_key>);
            static_assert(std::is_same_v<decltype(value), int>);
            seenKeys.push_back(key);
            seenValues.push_back(value);
            value += 1;   // mutable through the binding
        }
        CHECK(seenKeys == std::vector<slot_key>{a, b, c});
        CHECK(seenValues == std::vector<int>{10, 20, 30});
        CHECK(*map.find(a) == 11);

        int sum = 0;
        const slot_map<int> &constMap = map;
        for (const auto &[key, value] : constMap) {
            static_assert(std::is_same_v<decltype(value), const int>);
            CHECK(constMap.contains(key));
            sum += value;
        }
        CHECK(sum == 11 + 21 + 31);

        usize count = 0;
        for (auto &entry : map) {
            CHECK(map.find(entry.key()) == &entry.value());
            ++count;
        }
        CHECK(count == 3);

        auto iterator = map.begin();
        CHECK(iterator->key() == a);
        CHECK((*iterator).value() == 11);
        slot_map<int>::const_iterator converted = iterator;
        CHECK(converted == map.cbegin());
        ++converted;
        CHECK(converted->key() == b);
        CHECK(converted != map.cend());
        iterator++;
        iterator++;
        iterator++;
        CHECK(iterator == map.end());
    }

    TEST_CASE("slot_map - dereferencing an iterator yields a reference into the map's storage") {
        slot_map<int> map = {};
        const slot_key first = map.insert(1);
        const slot_key second = map.insert(2);
        static_assert(std::is_same_v<decltype(*map.begin()), slot_map<int>::entry &>);
        static_assert(std::is_same_v<decltype(*map.cbegin()), const slot_map<int>::entry &>);
        static_assert(std::forward_iterator<slot_map<int>::iterator>);

        // A reference bound to *it must not dangle once the (temporary) iterator is gone.
        const auto &entry = *map.begin();
        CHECK(&entry == map.begin());
        CHECK(&entry == map.entries().data());
        CHECK(entry.key() == first);

        auto &[key, value] = *std::find_if(map.begin(), map.end(), [second](const auto &candidate) {
            return candidate.key() == second;
        });
        CHECK(key == second);
        CHECK(&value == map.find(second));
        value = 20;
        CHECK(*map.find(second) == 20);

        // Equal iterators dereference to the same object, and *it is unaffected by ++it.
        auto a = map.begin();
        auto b = map.begin();
        CHECK(&*a == &*b);
        const auto &firstEntry = *a;
        ++a;
        CHECK(&firstEntry == &*b);
        CHECK(firstEntry.key() == first);
        CHECK(a->key() == second);
    }

    TEST_CASE("slot_map - a key type with swapped field order is built member by member") {
        slot_map<int, swapped_key> map = {};
        const swapped_key key = map.insert(5);
        CHECK(key.index == 0);
        CHECK(key.generation == 1);
        REQUIRE(map.find(key) != nullptr);
        CHECK(*map.find(key) == 5);

        REQUIRE(map.erase(key));
        const swapped_key reused = map.insert(6);
        CHECK(reused.index == 0);
        CHECK(reused.generation == 2);
        CHECK(map.find(key) == nullptr);
        REQUIRE(map.find(reused) != nullptr);
        CHECK(*map.find(reused) == 6);
        CHECK(map.key_at(0) == reused);
    }

    TEST_CASE("slot_map - iteration is stable under erase") {
        slot_map<int> map = {};
        for (int value = 0; value < 10; ++value) {
            static_cast<void>(map.insert(value));
        }

        SUBCASE("erase(iterator) returns the element that took the slot") {
            usize visited = 0;
            for (auto iterator = map.begin(); iterator != map.end();) {
                ++visited;
                if (iterator->value() % 2 == 0) {
                    iterator = map.erase(iterator);
                } else {
                    ++iterator;
                }
            }
            CHECK(visited == 10);
            CHECK(map.size() == 5);
            for (const auto &[key, value] : map) {
                CHECK(value % 2 == 1);
            }
        }

        SUBCASE("erasing by key while iterating over a snapshot of the keys") {
            std::vector<slot_key> snapshot = {};
            for (const auto &[key, value] : map) {
                snapshot.push_back(key);
            }
            for (const slot_key key : snapshot) {
                if (*map.find(key) < 5) {
                    CHECK(map.erase(key));
                }
            }
            CHECK(map.size() == 5);
            std::vector<int> remaining = {};
            for (const auto &[key, value] : map) {
                remaining.push_back(value);
            }
            std::sort(remaining.begin(), remaining.end());
            CHECK(remaining == std::vector<int>{5, 6, 7, 8, 9});
        }

        SUBCASE("erasing the end iterator asserts and changes nothing") {
            testing::expect_assert asserts = {};
            const auto result = map.erase(map.cend());
            CHECK(asserts.count() == 1);
            CHECK(result == map.end());
            CHECK(map.size() == 10);
        }

        SUBCASE("erasing an iterator of another map asserts, returns end and changes nothing") {
            slot_map<int> other = {};
            static_cast<void>(other.insert(1));
            testing::expect_assert asserts = {};
            const auto result = map.erase(other.begin());
            CHECK(asserts.count() == 1);
            CHECK(result == map.end());
            CHECK(map.size() == 10);
            CHECK(other.size() == 1);

            // An erase-while-iterating loop fed that result terminates instead of running away.
            usize visited = 0;
            for (auto iterator = result; iterator != map.end(); ++iterator) {
                ++visited;
            }
            CHECK(visited == 0);
        }
    }

    TEST_CASE("slot_map - clear bumps generations and keeps slots for reuse") {
        slot_map<int> map = {};
        const slot_key a = map.insert(1);
        const slot_key b = map.insert(2);
        const slot_key c = map.insert(3);
        const usize capacityBefore = map.capacity();

        map.clear();
        CHECK(map.empty());
        CHECK(map.find(a) == nullptr);
        CHECK(map.find(b) == nullptr);
        CHECK(map.find(c) == nullptr);
        CHECK(map.slot_count() == 3);
        CHECK(map.free_slot_count() == 3);
        CHECK(map.capacity() == capacityBefore);

        // Lowest slot is reused first, each with a bumped generation.
        const slot_key reusedA = map.insert(4);
        CHECK(reusedA.index == a.index);
        CHECK(reusedA.generation == a.generation + 1);
        const slot_key reusedB = map.insert(5);
        CHECK(reusedB.index == b.index);
        CHECK(map.find(a) == nullptr);
        CHECK(*map.find(reusedA) == 4);
        CHECK(map.slot_count() == 3);

        map.clear();
        map.clear();   // clearing an empty map is a no-op
        CHECK(map.free_slot_count() == 3);
    }

    TEST_CASE("slot_map - reserve grows capacity without changing contents") {
        slot_map<int> map = {};
        const slot_key key = map.insert(1);
        map.reserve(100);
        CHECK(map.capacity() >= 100);
        CHECK(map.size() == 1);
        CHECK(*map.find(key) == 1);
    }

    TEST_CASE("slot_map - generation wrap retires the slot") {
        slot_map<int, narrow_key> map = {};
        narrow_key key = map.insert(0);
        CHECK(key.index == 0);
        CHECK(key.generation == 1);

        // Generations 1..255 are issued; the 255th erase would wrap to 0, which retires the slot.
        for (int reuse = 1; reuse < 255; ++reuse) {
            REQUIRE(map.erase(key));
            key = map.insert(reuse);
            REQUIRE(key.index == 0);
            REQUIRE(key.generation == static_cast<u8>(reuse + 1));
        }
        CHECK(key.generation == 255);
        REQUIRE(map.erase(key));
        CHECK(map.retired_slot_count() == 1);
        CHECK(map.free_slot_count() == 0);
        CHECK(map.find(key) == nullptr);

        // The next insertion takes a brand-new slot; the retired one is never handed out again.
        const narrow_key fresh = map.insert(99);
        CHECK(fresh.index == 1);
        CHECK(fresh.generation == 1);
        CHECK(map.slot_count() == 2);
        CHECK(map.find(narrow_key{0, 0}) == nullptr);
        CHECK(map.find(narrow_key{0, 1}) == nullptr);
        CHECK(map.find(narrow_key{0, 255}) == nullptr);

        // clear() must not resurrect the retired slot either.
        map.clear();
        CHECK(map.free_slot_count() == 1);
        CHECK(map.retired_slot_count() == 1);
        CHECK(map.insert(1).index == 1);
    }

    TEST_CASE("slot_map - exhausting the key index type asserts and returns the null key") {
        slot_map<int, narrow_key> map = {};
        CHECK(slot_map<int, narrow_key>::MAX_SLOT_COUNT == 256);
        for (int value = 0; value < 256; ++value) {
            REQUIRE(map.insert(value).generation == 1);
        }
        CHECK(map.size() == 256);
        CHECK(map.slot_count() == 256);

        testing::expect_assert asserts = {};
        const narrow_key overflow = map.insert(256);
        CHECK(asserts.count() == 1);
        CHECK(overflow == narrow_key{});
        CHECK(map.size() == 256);

        // Freed slots are reused without hitting the limit.
        REQUIRE(map.erase(narrow_key{3, 1}));
        const narrow_key reused = map.insert(300);
        CHECK(reused.index == 3);
        CHECK(reused.generation == 2);
        CHECK(asserts.count() == 1);
    }

    TEST_CASE("slot_map - non-trivial elements are constructed and destroyed exactly once") {
        testing::tracked_value::reset();
        {
            testing::leak_check leaks = {};
            slot_map<testing::tracked_value> map = {};
            std::vector<slot_key> keys = {};
            for (int value = 0; value < 100; ++value) {
                keys.push_back(map.emplace(value));
            }
            CHECK(testing::tracked_value::live_count() == 100);

            for (usize index = 0; index < keys.size(); index += 3) {
                REQUIRE(map.erase(keys[index]));
            }
            CHECK(testing::tracked_value::live_count() == static_cast<i64>(map.size()));

            map.clear();
            CHECK(testing::tracked_value::live_count() == 0);

            static_cast<void>(map.emplace(1));
            static_cast<void>(map.emplace(2));
            CHECK(testing::tracked_value::live_count() == 2);
        }
        CHECK(testing::tracked_value::live_count() == 0);
        CHECK(testing::tracked_value::constructed_count() == testing::tracked_value::destroyed_count());
    }

    TEST_CASE("slot_map - copy and move preserve keys and contents") {
        slot_map<std::string> map = {};
        const slot_key a = map.insert("a");
        const slot_key b = map.insert("b");
        REQUIRE(map.erase(a));
        const slot_key c = map.insert("c");

        slot_map<std::string> copy = map;
        CHECK(copy.size() == 2);
        CHECK(copy.find(a) == nullptr);
        CHECK(*copy.find(b) == "b");
        CHECK(*copy.find(c) == "c");
        *copy.find(b) = "changed";
        CHECK(*map.find(b) == "b");

        slot_map<std::string> moved = std::move(map);
        CHECK(moved.size() == 2);
        CHECK(*moved.find(b) == "b");
        CHECK(*moved.find(c) == "c");

        slot_map<std::string> assigned = {};
        assigned = copy;
        CHECK(*assigned.find(b) == "changed");
        assigned = std::move(moved);
        CHECK(*assigned.find(b) == "b");
    }

    TEST_CASE("regression - sweep-2026-11-12 F000: a moved-from slot_map is empty and reusable") {
        // The source has a free slot, so a stale free-list head would point past its emptied storage.
        slot_map<std::string> source = {};
        const slot_key first = source.insert("first");
        const slot_key second = source.insert("second");
        REQUIRE(source.erase(first));
        REQUIRE(source.free_slot_count() == 1);

        SUBCASE("move construction") {
            slot_map<std::string> constructed = std::move(source);
            REQUIRE(constructed.find(second) != nullptr);
            CHECK(*constructed.find(second) == "second");
            CHECK(constructed.free_slot_count() == 1);
        }

        SUBCASE("move assignment") {
            slot_map<std::string> target = {};
            static_cast<void>(target.insert("replaced"));
            target = std::move(source);
            REQUIRE(target.find(second) != nullptr);
            CHECK(*target.find(second) == "second");
            CHECK(target.size() == 1);
            CHECK(target.free_slot_count() == 1);
        }

        CHECK(source.size() == 0);
        CHECK(source.empty());
        CHECK(source.slot_count() == 0);
        CHECK(source.free_slot_count() == 0);
        CHECK(source.retired_slot_count() == 0);
        CHECK(source.find(second) == nullptr);

        const slot_key again = source.insert("again");
        CHECK(again.index == 0);
        CHECK(again.generation == 1);
        REQUIRE(source.find(again) != nullptr);
        CHECK(*source.find(again) == "again");
        CHECK(source.size() == 1);
        CHECK(source.slot_count() == 1);
    }

    TEST_CASE("slot_map - property test against an unordered_map oracle") {
        std::mt19937_64 random(0x5EED);
        slot_map<u64> map = {};
        std::unordered_map<u64, u64> oracle = {};   // key.to_u64() -> value
        std::vector<slot_key> live = {};
        std::vector<slot_key> stale = {};

        for (int step = 0; step < 20000; ++step) {
            const u64 roll = random() % 100;
            if (roll < 50 || live.empty()) {
                const u64 value = random();
                const slot_key key = map.insert(value);
                REQUIRE(key.is_valid());
                REQUIRE(oracle.emplace(key.to_u64(), value).second);
                live.push_back(key);
            } else if (roll < 85) {
                const usize pick = static_cast<usize>(random() % live.size());
                const slot_key key = live[pick];
                REQUIRE(map.erase(key));
                REQUIRE(oracle.erase(key.to_u64()) == 1);
                live[pick] = live.back();
                live.pop_back();
                stale.push_back(key);
            } else if (roll < 95 && !stale.empty()) {
                const slot_key key = stale[static_cast<usize>(random() % stale.size())];
                REQUIRE(map.find(key) == nullptr);
                REQUIRE_FALSE(map.erase(key));
            } else if (roll == 99 && step % 1000 == 0) {
                map.clear();
                oracle.clear();
                for (const slot_key key : live) {
                    stale.push_back(key);
                }
                live.clear();
            }

            REQUIRE(map.size() == oracle.size());
            if (step % 500 == 0) {
                for (const auto &[key, value] : map) {
                    const auto found = oracle.find(key.to_u64());
                    REQUIRE(found != oracle.end());
                    REQUIRE(found->second == value);
                }
                for (const slot_key key : live) {
                    REQUIRE(map.find(key) != nullptr);
                    REQUIRE(*map.find(key) == oracle.at(key.to_u64()));
                }
            }
        }

        for (const slot_key key : stale) {
            CHECK(map.find(key) == nullptr);
        }
    }

    TEST_CASE("slot_map - concurrent readers see a consistent map") {
        slot_map<int> map = {};
        std::vector<slot_key> keys = {};
        for (int value = 0; value < 1000; ++value) {
            keys.push_back(map.insert(value));
        }
        for (usize index = 0; index < keys.size(); index += 2) {
            REQUIRE(map.erase(keys[index]));
        }

        const slot_map<int> &shared = map;
        std::vector<std::thread> threads = {};
        std::vector<int> failures(8, 0);
        for (usize threadIndex = 0; threadIndex < 8; ++threadIndex) {
            threads.emplace_back([&shared, &keys, &failures, threadIndex] {
                for (int repeat = 0; repeat < 50; ++repeat) {
                    for (usize index = 0; index < keys.size(); ++index) {
                        const int *found = shared.find(keys[index]);
                        const bool expectedLive = index % 2 == 1;
                        if ((found != nullptr) != expectedLive) {
                            ++failures[threadIndex];
                        } else if (found != nullptr && *found != static_cast<int>(index)) {
                            ++failures[threadIndex];
                        }
                    }
                    int sum = 0;
                    for (const auto &[key, value] : shared) {
                        sum += value;
                    }
                    if (sum != 250000) {
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
}

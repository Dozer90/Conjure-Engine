#include "conjure/testing/doctest.h"

#include "tracked_value.h"

#include "conjure/core/containers/flat_map.h"

#include <functional>
#include <map>
#include <random>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::core;

namespace {
    /// <summary>
    /// True when every lookup of Map accepts KeyLike (requires a transparent comparator).
    /// </summary>
    template <typename Map, typename KeyLike>
    concept heterogeneous_lookup = requires(Map map, const Map constMap, const KeyLike &key) {
        map.find(key);
        constMap.find(key);
        constMap.contains(key);
        constMap.count(key);
        map.lower_bound(key);
        constMap.lower_bound(key);
        map.upper_bound(key);
        constMap.upper_bound(key);
        map.at(key);
        constMap.at(key);
        map.erase(key);
    };

    static_assert(heterogeneous_lookup<flat_map<std::string, int, std::less<>>, std::string_view>);
    static_assert(heterogeneous_lookup<flat_map<std::string, int, std::less<>>, const char *>);
    static_assert(!heterogeneous_lookup<flat_map<std::string, int>, std::string_view>);
    static_assert(heterogeneous_lookup<flat_map<std::string, int>, std::string>);

    template <typename Map>
    std::vector<int> keys_of(const Map &container) {
        std::vector<int> keys = {};
        for (const auto &[key, value] : container) {
            keys.push_back(key);
        }
        return keys;
    }
}

TEST_SUITE("core/flat_map") {

    TEST_CASE("flat_map - default constructed is empty") {
        flat_map<int, int> map = {};
        CHECK(map.empty());
        CHECK(map.size() == 0);
        CHECK(map.capacity() == 0);
        CHECK(map.begin() == map.end());
        CHECK(map.find(1) == map.end());
        CHECK_FALSE(map.contains(1));
        CHECK(map.count(1) == 0);
        CHECK_FALSE(map.erase(1));
        CHECK(map.lower_bound(1) == map.end());
        CHECK(map.entries().empty());
    }

    TEST_CASE("flat_map - insert keeps keys sorted and unique") {
        flat_map<int, std::string> map = {};
        auto inserted = map.insert({5, "five"});
        CHECK(inserted.second);
        CHECK(inserted.first->first == 5);
        inserted = map.insert(std::make_pair(1, std::string("one")));
        CHECK(inserted.second);
        inserted = map.insert({3, "three"});
        CHECK(inserted.second);

        inserted = map.insert({3, "duplicate"});
        CHECK_FALSE(inserted.second);
        CHECK(inserted.first->second == "three");

        CHECK(map.size() == 3);
        CHECK(keys_of(map) == std::vector<int>{1, 3, 5});

        const std::vector<std::pair<int, std::string>> more = {{2, "two"}, {4, "four"}, {5, "again"}};
        map.insert(more.begin(), more.end());
        CHECK(keys_of(map) == std::vector<int>{1, 2, 3, 4, 5});
        CHECK(map.at(5) == "five");

        auto emplaced = map.emplace(6, "six");
        CHECK(emplaced.second);
        CHECK(map.at(6) == "six");
    }

    TEST_CASE("flat_map - find, contains, count, lower_bound and upper_bound") {
        flat_map<int, int> map = {{10, 1}, {20, 2}, {30, 3}};
        REQUIRE(map.find(20) != map.end());
        CHECK(map.find(20)->second == 2);
        CHECK(map.find(25) == map.end());
        CHECK(map.contains(10));
        CHECK_FALSE(map.contains(11));
        CHECK(map.count(30) == 1);
        CHECK(map.count(31) == 0);

        CHECK(map.lower_bound(20)->first == 20);
        CHECK(map.lower_bound(21)->first == 30);
        CHECK(map.lower_bound(31) == map.end());
        CHECK(map.upper_bound(20)->first == 30);
        CHECK(map.upper_bound(5)->first == 10);
        CHECK(map.upper_bound(30) == map.end());

        const flat_map<int, int> &constMap = map;
        CHECK(constMap.find(10)->second == 1);
        CHECK(constMap.find(0) == constMap.end());
        CHECK(constMap.lower_bound(15)->first == 20);
        CHECK(constMap.upper_bound(15)->first == 20);

        map.find(10)->second = 100;
        CHECK(constMap.at(10) == 100);
    }

    TEST_CASE("flat_map - try_emplace only constructs the value when the key is absent") {
        testing::tracked_value::reset();
        flat_map<int, testing::tracked_value> map = {};
        auto first = map.try_emplace(1, 10);
        CHECK(first.second);
        CHECK(first.first->second.value() == 10);
        const i64 constructedAfterFirst = testing::tracked_value::constructed_count();

        auto second = map.try_emplace(1, 20);
        CHECK_FALSE(second.second);
        CHECK(second.first->second.value() == 10);
        CHECK(testing::tracked_value::constructed_count() == constructedAfterFirst);

        int key = 2;
        auto third = map.try_emplace(key, 30);
        CHECK(third.second);
        CHECK(map.size() == 2);
        CHECK(testing::tracked_value::live_count() == 2);
    }

    TEST_CASE("flat_map - insert_or_assign overwrites and operator[] default-inserts") {
        flat_map<std::string, int> map = {};
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

    TEST_CASE("flat_map - at returns the value of a present key") {
        // A missing key is fatal (assert + terminate, like result::value()), so only the success
        // path can run inside the test process.
        flat_map<int, int> map = {{1, 1}};
        const flat_map<int, int> &constMap = map;
        CHECK(map.at(1) == 1);
        CHECK(constMap.at(1) == 1);
        map.at(1) = 5;
        CHECK(constMap.at(1) == 5);
    }

    TEST_CASE("flat_map - erase by key, iterator and range") {
        flat_map<int, int> map = {{1, 1}, {2, 2}, {3, 3}, {4, 4}, {5, 5}};
        CHECK(map.erase(3));
        CHECK_FALSE(map.erase(3));
        CHECK(keys_of(map) == std::vector<int>{1, 2, 4, 5});

        auto next = map.erase(map.find(2));
        CHECK(next->first == 4);
        CHECK(keys_of(map) == std::vector<int>{1, 4, 5});

        next = map.erase(map.begin(), map.begin() + 2);
        CHECK(next->first == 5);
        CHECK(keys_of(map) == std::vector<int>{5});

        next = map.erase(map.cbegin());
        CHECK(next == map.end());
        CHECK(map.empty());

        testing::tracked_value::reset();
        {
            testing::leak_check leaks = {};
            flat_map<int, testing::tracked_value> tracked = {};
            for (int key = 0; key < 20; ++key) {
                tracked.try_emplace(key, key);
            }
            for (int key = 0; key < 20; key += 2) {
                CHECK(tracked.erase(key));
            }
            CHECK(testing::tracked_value::live_count() == 10);
            tracked.clear();
            CHECK(testing::tracked_value::live_count() == 0);
        }
    }

    TEST_CASE("flat_map - initializer list keeps the first of duplicate keys and iterates in order") {
        const flat_map<int, std::string> map = {{3, "c"}, {1, "a"}, {2, "b"}, {1, "duplicate"}};
        CHECK(map.size() == 3);
        CHECK(keys_of(map) == std::vector<int>{1, 2, 3});
        CHECK(map.at(1) == "a");

        std::vector<int> reversed = {};
        for (auto iterator = map.rbegin(); iterator != map.rend(); ++iterator) {
            reversed.push_back(iterator->first);
        }
        CHECK(reversed == std::vector<int>{3, 2, 1});

        const std::span<const std::pair<int, std::string>> entries = map.entries();
        CHECK(entries.size() == 3);
        CHECK(entries[2].second == "c");
    }

    TEST_CASE("flat_map - transparent comparators allow heterogeneous lookup") {
        flat_map<std::string, int, std::less<>> map = {{"alpha", 1}, {"beta", 2}};
        const std::string_view view = "beta";
        REQUIRE(map.find(view) != map.end());
        CHECK(map.find(view)->second == 2);
        CHECK(map.contains(view));
        CHECK_FALSE(map.contains(std::string_view("gamma")));
        CHECK(map.find("alpha")->second == 1);

        const flat_map<std::string, int, std::less<>> &constMap = map;
        CHECK(constMap.find(std::string_view("alpha")) != constMap.end());
        CHECK(constMap.contains("beta"));

        SUBCASE("count, lower_bound, upper_bound, at and erase take the key-like type too") {
            CHECK(map.count(view) == 1);
            CHECK(map.count(std::string_view("gamma")) == 0);
            CHECK(constMap.count("alpha") == 1);

            CHECK(map.lower_bound(std::string_view("b"))->first == "beta");
            CHECK(map.lower_bound(std::string_view("beta"))->first == "beta");
            CHECK(map.lower_bound(std::string_view("c")) == map.end());
            CHECK(constMap.lower_bound(std::string_view("a"))->first == "alpha");
            CHECK(map.upper_bound(std::string_view("alpha"))->first == "beta");
            CHECK(map.upper_bound(std::string_view("beta")) == map.end());
            CHECK(constMap.upper_bound(std::string_view(""))->first == "alpha");

            CHECK(map.at(view) == 2);
            CHECK(constMap.at(std::string_view("alpha")) == 1);
            map.at(std::string_view("alpha")) = 10;
            CHECK(constMap.at("alpha") == 10);

            CHECK(map.erase(view));
            CHECK_FALSE(map.erase(view));
            CHECK_FALSE(map.erase(std::string_view("gamma")));
            CHECK(map.size() == 1);
            CHECK_FALSE(map.contains(view));
            CHECK(map.contains(std::string_view("alpha")));
        }
    }

    TEST_CASE("flat_map - custom comparator controls the order") {
        flat_map<int, int, std::greater<int>> map(std::greater<int>{});
        map.insert({1, 1});
        map.insert({3, 3});
        map.insert({2, 2});
        CHECK(keys_of(map) == std::vector<int>{3, 2, 1});
        CHECK(map.lower_bound(2)->first == 2);
        CHECK(map.upper_bound(2)->first == 1);
        CHECK(map.find(3)->second == 3);
        CHECK(map.key_comp()(3, 1));
    }

    TEST_CASE("flat_map - reserve, clear, shrink_to_fit and swap") {
        flat_map<int, int> map = {};
        map.reserve(50);
        CHECK(map.capacity() >= 50);
        map.insert({1, 1});
        map.clear();
        CHECK(map.empty());
        CHECK(map.capacity() >= 50);
        map.shrink_to_fit();
        CHECK(map.capacity() == 0);

        flat_map<int, int> a = {{1, 1}};
        flat_map<int, int> b = {{2, 2}, {3, 3}};
        a.swap(b);
        CHECK(a.size() == 2);
        CHECK(b.size() == 1);
        swap(a, b);
        CHECK(a.size() == 1);
        CHECK(a.contains(1));
    }

    TEST_CASE("flat_map - copy, move and equality") {
        flat_map<int, std::string> source = {{1, "a"}, {2, "b"}};
        flat_map<int, std::string> copy = source;
        CHECK(copy == source);
        copy[1] = "changed";
        CHECK(copy != source);
        CHECK(source.at(1) == "a");

        flat_map<int, std::string> moved = std::move(source);
        CHECK(moved.size() == 2);
        CHECK(moved.at(2) == "b");

        flat_map<int, std::string> assigned = {};
        assigned = copy;
        CHECK(assigned == copy);
        assigned = std::move(moved);
        CHECK(assigned.at(1) == "a");
    }

    TEST_CASE("flat_map - property test against a std::map oracle") {
        std::mt19937_64 random(0x5EED);
        flat_map<int, u64> map = {};
        std::map<int, u64> oracle = {};

        for (int step = 0; step < 20000; ++step) {
            const u64 roll = random() % 100;
            const int key = static_cast<int>(random() % 500);
            const u64 value = random();
            if (roll < 40) {
                const bool inserted = map.insert({key, value}).second;
                CHECK(inserted == oracle.insert({key, value}).second);
            } else if (roll < 55) {
                map.insert_or_assign(key, value);
                oracle.insert_or_assign(key, value);
            } else if (roll < 65) {
                CHECK(map.try_emplace(key, value).second == oracle.try_emplace(key, value).second);
            } else if (roll < 90) {
                CHECK(map.erase(key) == (oracle.erase(key) == 1));
            } else if (roll < 97) {
                const auto found = map.find(key);
                const auto expected = oracle.find(key);
                REQUIRE((found == map.end()) == (expected == oracle.end()));
                if (found != map.end()) {
                    REQUIRE(found->second == expected->second);
                }
            } else {
                const auto found = map.lower_bound(key);
                const auto expected = oracle.lower_bound(key);
                REQUIRE((found == map.end()) == (expected == oracle.end()));
                if (found != map.end()) {
                    REQUIRE(found->first == expected->first);
                }
            }

            REQUIRE(map.size() == oracle.size());
            if (step % 250 == 0) {
                auto expected = oracle.begin();
                for (const auto &[mapKey, mapValue] : map) {
                    REQUIRE(expected != oracle.end());
                    REQUIRE(mapKey == expected->first);
                    REQUIRE(mapValue == expected->second);
                    ++expected;
                }
                REQUIRE(expected == oracle.end());
            }
        }
    }
}

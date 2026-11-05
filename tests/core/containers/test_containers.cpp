#include "conjure/testing/doctest.h"

#include "conjure/core/containers/containers.h"

#include <format>
#include <string>
#include <type_traits>
#include <utility>

using namespace conjure;
using namespace conjure::core;

namespace {
    struct component_desc {
        std::string name = {};
        small_vector<u32, 4> fieldOffsets = {};
    };

    // The umbrella header must expose every container type and the slot_key text helpers.
    static_assert(std::is_same_v<slot_map<int>::key_type, slot_key>);
    static_assert(small_vector<int, 4>::INLINE_CAPACITY == 4);
    static_assert(stable_vector<int>::CHUNK_SIZE == 64);
    static_assert(std::is_same_v<flat_hash_map<int, int>::value_type, std::pair<const int, int>>);
    static_assert(std::is_same_v<flat_hash_set<int>::value_type, int>);
    static_assert(std::is_same_v<flat_map<int, int>::value_type, std::pair<int, int>>);
    static_assert(std::is_same_v<decltype(parse_slot_key("")), std::optional<slot_key>>);
}

TEST_SUITE("core/containers") {

    TEST_CASE("containers - every container from the umbrella header is usable") {
        slot_map<int> slots = {};
        const slot_key key = slots.insert(1);
        CHECK(slots.find(key) != nullptr);
        CHECK(std::format("{}", key) == "0:1");
        CHECK(parse_slot_key(to_string(key)) == key);

        small_vector<int, 4> small = {1, 2};
        small.push_back(3);
        CHECK(small.size() == 3);

        stable_vector<int> stable = {};
        CHECK(stable.emplace_back(4) == 4);

        flat_hash_map<int, int> hashed = {{1, 2}};
        CHECK(hashed.at(1) == 2);

        flat_hash_set<int> set = {7};
        CHECK(set.contains(7));

        flat_map<int, int> sorted = {{3, 4}};
        CHECK(sorted.at(3) == 4);
    }

    TEST_CASE("containers - containers compose: registry of slots indexed by name") {
        stable_vector<component_desc, 8> descriptions = {};
        flat_hash_map<std::string, u32> indexByName = {};
        flat_map<u32, slot_key> slotByIndex = {};
        slot_map<component_desc *> live = {};

        for (u32 index = 0; index < 20; ++index) {
            component_desc &description = descriptions.emplace_back();
            description.name = "component_" + std::to_string(index);
            for (u32 field = 0; field < index % 6; ++field) {
                description.fieldOffsets.push_back(field * 4);
            }
            indexByName.try_emplace(description.name, index);
            const slot_key key = live.insert(&description);
            slotByIndex.insert_or_assign(index, key);
        }

        const u32 index = indexByName.at("component_7");
        const slot_key key = slotByIndex.at(index);
        REQUIRE(live.find(key) != nullptr);
        component_desc *description = *live.find(key);
        REQUIRE(description != nullptr);
        CHECK(description->name == "component_7");
        CHECK(description->fieldOffsets.size() == 1);
        CHECK(description == &descriptions[7]);

        CHECK(live.erase(key));
        CHECK(live.find(key) == nullptr);
        CHECK(slotByIndex.erase(index));
        CHECK(indexByName.erase("component_7"));
        CHECK(descriptions.size() == 20);   // stable storage keeps the slot; the registry forgot it
        CHECK(live.size() == 19);
        CHECK(slotByIndex.size() == 19);
        CHECK(indexByName.size() == 19);
    }
}

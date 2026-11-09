#include "conjure/testing/doctest.h"

#include "conjure/testing/assert_capture.h"

#include "conjure/ecs/component.h"

#include <unordered_map>
#include <vector>

using namespace conjure;
using namespace conjure::ecs;

TEST_SUITE("ecs/component_mask") {

    TEST_CASE("component_mask - set clear test") {
        component_mask mask = {};
        CHECK(mask.none());
        CHECK_FALSE(mask.test(component_id{3}));
        mask.set(component_id{3});
        CHECK(mask.test(component_id{3}));
        CHECK_FALSE(mask.test(component_id{4}));
        CHECK_FALSE(mask.none());
        mask.clear(component_id{3});
        CHECK_FALSE(mask.test(component_id{3}));
        CHECK(mask.none());
        CHECK_FALSE(mask.test(component_id{}));
    }

    TEST_CASE("component_mask - invalid ids assert and are ignored") {
        component_mask mask = {};
        CONJURE_CHECK_ASSERTS(mask.set(component_id{}));
        CONJURE_CHECK_ASSERTS(mask.clear(component_id{static_cast<u16>(MAX_COMPONENTS)}));
        CHECK(mask.none());
    }

    TEST_CASE("component_mask - contains_all and contains_any") {
        component_mask a = {};
        a.set(component_id{1});
        a.set(component_id{70});
        a.set(component_id{900});
        component_mask b = {};
        b.set(component_id{1});
        b.set(component_id{900});
        component_mask c = {};
        c.set(component_id{2});

        CHECK(a.contains_all(b));
        CHECK_FALSE(b.contains_all(a));
        CHECK(a.contains_all(component_mask{}));
        CHECK(a.contains_any(b));
        CHECK_FALSE(a.contains_any(c));
        CHECK_FALSE(a.contains_any(component_mask{}));
    }

    TEST_CASE("component_mask - count and none") {
        component_mask mask = {};
        CHECK(mask.count() == 0);
        CHECK(mask.none());
        for (u16 id = 0; id < 100; id = static_cast<u16>(id + 7)) {
            mask.set(component_id{id});
        }
        CHECK(mask.count() == 15);
        CHECK_FALSE(mask.none());
        CHECK((~mask).count() == MAX_COMPONENTS - 15);
    }

    TEST_CASE("component_mask - equal masks hash equal") {
        component_mask a = {};
        component_mask b = {};
        a.set(component_id{5});
        a.set(component_id{511});
        b.set(component_id{511});
        b.set(component_id{5});
        CHECK(a == b);
        CHECK(a.hash() == b.hash());
        CHECK(component_mask::hasher{}(a) == component_mask::hasher{}(b));

        component_mask c = a;
        c.set(component_id{6});
        CHECK_FALSE(a == c);
        CHECK(a.hash() != c.hash());
        CHECK(component_mask{}.hash() != a.hash());

        std::unordered_map<component_mask, int, component_mask::hasher> table = {};
        table[a] = 1;
        table[c] = 2;
        CHECK(table.size() == 2);
        CHECK(table[b] == 1);
    }

    TEST_CASE("component_mask - bit 1023 works") {
        component_mask mask = {};
        const component_id last{static_cast<u16>(MAX_COMPONENTS - 1)};
        mask.set(last);
        CHECK(mask.test(last));
        CHECK(mask.count() == 1);
        CHECK(mask.words()[component_mask::WORD_COUNT - 1] == (u64{1} << 63));
        std::vector<component_id> visited = {};
        mask.for_each_set([&](component_id id) {
            visited.push_back(id);
        });
        REQUIRE(visited.size() == 1);
        CHECK(visited[0] == last);
    }

    TEST_CASE("component_mask - for_each_set visits ascending") {
        component_mask mask = {};
        const u16 ids[] = {1000, 64, 0, 63, 65, 513, 7};
        for (const u16 id : ids) {
            mask.set(component_id{id});
        }
        std::vector<u16> visited = {};
        mask.for_each_set([&](component_id id) {
            visited.push_back(id.value);
        });
        CHECK(visited == std::vector<u16>{0, 7, 63, 64, 65, 513, 1000});
    }

    TEST_CASE("component_mask - bitwise operators") {
        component_mask a = {};
        component_mask b = {};
        a.set(component_id{1});
        a.set(component_id{2});
        b.set(component_id{2});
        b.set(component_id{3});

        const component_mask both = a | b;
        CHECK(both.count() == 3);
        const component_mask common = a & b;
        CHECK(common.count() == 1);
        CHECK(common.test(component_id{2}));
        const component_mask onlyA = a & ~b;
        CHECK(onlyA.count() == 1);
        CHECK(onlyA.test(component_id{1}));

        component_mask accumulated = a;
        accumulated |= b;
        CHECK(accumulated == both);
        accumulated &= b;
        CHECK(accumulated == b);
    }
}

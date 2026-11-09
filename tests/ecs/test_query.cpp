#include "conjure/testing/doctest.h"

#include "ecs_test_fixture.h"

#include "conjure/testing/allocation_scope.h"

#include "conjure/ecs/query.h"
#include "conjure/ecs/world.h"

#include <unordered_set>
#include <vector>

using namespace conjure;
using namespace conjure::ecs;
using namespace ecs_test;

TEST_SUITE("ecs/query") {

    TEST_CASE("query - with without optional matching") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const core::result<entity> onlyPosition = world.create_entity(fixture.mask<position>());
        const core::result<entity> both = world.create_entity(fixture.mask<position, velocity>());
        const core::result<entity> tagged = world.create_entity(fixture.mask<position, player_tag>());
        const core::result<entity> onlyVelocity = world.create_entity(fixture.mask<velocity>());
        CONJURE_REQUIRE_OK(onlyPosition);
        CONJURE_REQUIRE_OK(both);
        CONJURE_REQUIRE_OK(tagged);
        CONJURE_REQUIRE_OK(onlyVelocity);

        query_desc desc = {};
        desc.with = fixture.mask<position>();
        query positions = world.create_query(desc);
        CHECK(positions.entity_count() == 3);
        CHECK(positions.matches().size() == 3);
        CHECK(positions.desc().with == desc.with);

        desc.without = fixture.mask<player_tag>();
        query untagged = world.create_query(desc);
        CHECK(untagged.entity_count() == 2);

        desc.optional = fixture.mask<velocity>();
        query withOptional = world.create_query(desc);
        std::vector<entity> seen = {};
        u32 withVelocity = 0;
        withOptional.for_each_chunk([&](chunk_view &chunk) {
            CHECK(chunk.has(fixture.id<position>()));
            const std::span<const velocity> velocities = chunk.get_optional<const velocity>();
            if (!velocities.empty()) {
                withVelocity += chunk.count();
            }
            for (const entity id : chunk.entities()) {
                seen.push_back(id);
            }
        });
        CHECK(seen.size() == 2);
        CHECK(withVelocity == 1);

        query_desc nothing = {};
        nothing.with = fixture.mask<health>();
        query empty = world.create_query(nothing);
        CHECK(empty.entity_count() == 0);
        CHECK(empty.chunk_count() == 0);
        CHECK(empty.matches().empty());
        CHECK(&empty.owner() == &world);
        CHECK(&empty.registry() == &fixture.registry);
    }

    TEST_CASE("query - incremental refresh sees new archetypes") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        query_desc desc = {};
        desc.with = fixture.mask<position>();
        query positions = world.create_query(desc);
        CHECK(positions.entity_count() == 0);

        CONJURE_REQUIRE_OK(world.create_entity(fixture.mask<position>()));
        CHECK(positions.entity_count() == 1);
        CHECK(positions.matches().size() == 1);
        CONJURE_REQUIRE_OK(world.create_entity(fixture.mask<position, velocity>()));
        CONJURE_REQUIRE_OK(world.create_entity(fixture.mask<velocity>()));
        CHECK(positions.matches().size() == 2);
        CHECK(positions.entity_count() == 2);

        // Refreshing again adds nothing twice.
        positions.refresh();
        positions.refresh();
        CHECK(positions.matches().size() == 2);

        positions.reset(query_desc{});
        CHECK(positions.matches().size() == world.archetypes().size());
    }

    TEST_CASE("query - entity_count across chunks") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        std::vector<entity> ids(1500);
        CONJURE_REQUIRE_OK(world.create_entities(fixture.mask<position>(), ids));
        std::vector<entity> more(400);
        CONJURE_REQUIRE_OK(world.create_entities(fixture.mask<position, health>(), more));

        query_desc desc = {};
        desc.with = fixture.mask<position>();
        query positions = world.create_query(desc);
        CHECK(positions.entity_count() == 1900);
        CHECK(positions.chunk_count() > 2);
        u32 counted = 0;
        positions.for_each_chunk([&](chunk_view &chunk) {
            counted += chunk.count();
        });
        CHECK(counted == 1900);
    }

    TEST_CASE("query - empty archetypes skipped") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        std::vector<entity> ids(100);
        CONJURE_REQUIRE_OK(world.create_entities(fixture.mask<position>(), ids));
        CONJURE_REQUIRE_OK(world.create_entity(fixture.mask<position, velocity>()));
        for (const entity id : ids) {
            CONJURE_REQUIRE_OK(world.destroy_entity(id));
        }

        query_desc desc = {};
        desc.with = fixture.mask<position>();
        query positions = world.create_query(desc);
        CHECK(positions.matches().size() == 2);
        u32 chunks = 0;
        positions.for_each_chunk([&](chunk_view &chunk) {
            ++chunks;
            CHECK(chunk.count() == 1);
        });
        CHECK(chunks == 1);
        CHECK(positions.chunk_count() == 1);
    }

    TEST_CASE("query - chunk index flat over matches") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        std::vector<entity> first(2000);
        std::vector<entity> second(1500);
        CONJURE_REQUIRE_OK(world.create_entities(fixture.mask<position>(), first));
        CONJURE_REQUIRE_OK(world.create_entities(fixture.mask<position, velocity>(), second));

        query_desc desc = {};
        desc.with = fixture.mask<position>();
        query positions = world.create_query(desc);
        const u32 count = positions.chunk_count();
        REQUIRE(count >= 4);

        const iteration_scope scope = world.lock_iteration();
        std::unordered_set<u64> seen = {};
        u32 total = 0;
        for (u32 index = 0; index < count; ++index) {
            chunk_view chunk = positions.chunk(index);
            REQUIRE(chunk.is_valid());
            CHECK(chunk.count() > 0);
            total += chunk.count();
            for (const entity id : chunk.entities()) {
                CHECK(seen.insert(id.to_u64()).second);
            }
        }
        CHECK(total == 3500);
        CHECK_FALSE(positions.chunk(count).is_valid());
        CHECK(positions.chunk(count).count() == 0);
    }

    TEST_CASE("query - 10k entities visited exactly once") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        std::vector<entity> a(4000);
        std::vector<entity> b(3000);
        std::vector<entity> c(3000);
        CONJURE_REQUIRE_OK(world.create_entities(fixture.mask<position>(), a));
        CONJURE_REQUIRE_OK(world.create_entities(fixture.mask<position, velocity>(), b));
        CONJURE_REQUIRE_OK(world.create_entities(fixture.mask<position, health, player_tag>(), c));

        query_desc desc = {};
        desc.with = fixture.mask<position>();
        query positions = world.create_query(desc);
        std::vector<u32> visits(10000, 0);
        positions.for_each_chunk([&](chunk_view &chunk) {
            for (const entity id : chunk.entities()) {
                ++visits[id.index];
            }
        });
        for (u32 index = 0; index < visits.size(); ++index) {
            CAPTURE(index);
            CHECK(visits[index] == 1);
        }
    }

    TEST_CASE("query - iteration locks the world and allocates nothing") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        std::vector<entity> ids(2000);
        CONJURE_REQUIRE_OK(world.create_entities(fixture.mask<position>(), ids));
        query_desc desc = {};
        desc.with = fixture.mask<position>();
        query positions = world.create_query(desc);
        positions.refresh();

        bool lockedInside = false;
        testing::allocation_scope allocations = {};
        positions.for_each_chunk([&](chunk_view &) {
            lockedInside = world.is_locked();
        });
        const u64 allocated = allocations.allocation_count();
        CHECK(allocated == 0);
        CHECK(lockedInside);
        CHECK_FALSE(world.is_locked());
    }
}

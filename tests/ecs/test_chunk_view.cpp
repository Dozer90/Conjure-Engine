#include "conjure/testing/doctest.h"

#include "ecs_test_fixture.h"

#include "conjure/testing/assert_capture.h"

#include "conjure/ecs/chunk_view.h"
#include "conjure/ecs/query.h"
#include "conjure/ecs/world.h"

#include <vector>

using namespace conjure;
using namespace conjure::ecs;
using namespace ecs_test;

namespace {
    query position_query(ecs_fixture &fixture) {
        query_desc desc = {};
        desc.with = fixture.mask<position>();
        return fixture.world.create_query(desc);
    }
}

TEST_SUITE("ecs/chunk_view") {

    TEST_CASE("chunk_view - default view is invalid and empty") {
        const chunk_view view = {};
        CHECK_FALSE(view.is_valid());
        CHECK(view.count() == 0);
        CHECK(view.entities().empty());
        CHECK(view.owner() == nullptr);
        CHECK_FALSE(view.has(component_id{0}));
        CHECK(view.column_stride(component_id{0}) == 0);
        CHECK(view.column_version(component_id{0}) == 0);
        chunk_view mutableView = {};
        CHECK(mutableView.column_data(component_id{0}, true) == nullptr);
        CHECK(mutableView.get_optional<position>().empty());
    }

    TEST_CASE("chunk_view - count entities and typed spans") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        std::vector<entity> ids(10);
        CONJURE_REQUIRE_OK(world.create_entities(fixture.mask<position, velocity, player_tag>(), ids));
        for (u32 index = 0; index < ids.size(); ++index) {
            CONJURE_REQUIRE_OK(world.set(ids[index], velocity{static_cast<f32>(index), 0.0f, 0.0f}));
        }

        query positions = position_query(fixture);
        u32 chunks = 0;
        positions.for_each_chunk([&](chunk_view &chunk) {
            ++chunks;
            CHECK(chunk.is_valid());
            CHECK(chunk.count() == 10);
            CHECK(chunk.entities().size() == 10);
            CHECK(chunk.entities()[3] == ids[3]);
            CHECK(chunk.owner() != nullptr);
            CHECK(chunk.chunk_index() == 0);
            CHECK(chunk.has(fixture.id<player_tag>()));
            CHECK_FALSE(chunk.has(fixture.id<health>()));

            const std::span<const velocity> velocities = chunk.get<const velocity>();
            REQUIRE(velocities.size() == 10);
            CHECK(velocities[7].x == 7.0f);
            std::span<position> positionsSpan = chunk.get<position>();
            REQUIRE(positionsSpan.size() == 10);
            positionsSpan[2].y = 42.0f;
            CHECK(chunk.column_stride(fixture.id<position>()) == sizeof(position));
            CHECK(chunk.column_stride(fixture.id<player_tag>()) == 0);
            CHECK(chunk.column_stride(fixture.id<health>()) == 0);
        });
        CHECK(chunks == 1);
        CHECK(world.get<position>(ids[2])->y == 42.0f);
    }

    TEST_CASE("chunk_view - get of an absent component asserts and yields empty") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        CONJURE_REQUIRE_OK(world.create_entity(fixture.mask<position>()));
        query positions = position_query(fixture);
        positions.for_each_chunk([&](chunk_view &chunk) {
            testing::expect_assert guard = {};
            const std::span<velocity> missing = chunk.get<velocity>();
            CHECK(missing.empty());
            const std::span<player_tag> tag = chunk.get<player_tag>();
            CHECK(tag.empty());
            CHECK(guard.count() == 2 * ASSERTS_PER_VIOLATION);
            CHECK(chunk.get_optional<velocity>().empty());
            CHECK(chunk.column_data(fixture.id<velocity>(), false) == nullptr);
        });
    }

    TEST_CASE("chunk_view - column_data stamps only for write") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        CONJURE_REQUIRE_OK(world.create_entity(fixture.mask<position, velocity>()));
        world.bump_version();
        world.bump_version();
        query positions = position_query(fixture);
        positions.for_each_chunk([&](chunk_view &chunk) {
            const component_id positionId = fixture.id<position>();
            const component_id velocityId = fixture.id<velocity>();
            const u64 before = chunk.column_version(positionId);
            CHECK(before < chunk.stamp_version());
            CHECK(chunk.column_data(positionId, false) != nullptr);
            CHECK(chunk.column_version(positionId) == before);
            const chunk_view &constChunk = chunk;
            CHECK(constChunk.column_data(positionId) != nullptr);
            CHECK(chunk.column_version(positionId) == before);

            CHECK(chunk.column_data(positionId, true) != nullptr);
            CHECK(chunk.column_version(positionId) == chunk.stamp_version());
            CHECK(chunk.column_version(velocityId) == before);
            CHECK(chunk.column_data(velocityId, false) == constChunk.column_data(velocityId));
        });
    }

    TEST_CASE("chunk_view - copies share the chunk") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        CONJURE_REQUIRE_OK(world.create_entity(fixture.mask<position>()));
        query positions = position_query(fixture);
        chunk_view copy = {};
        positions.for_each_chunk([&](chunk_view &chunk) {
            copy = chunk;
        });
        CHECK(copy.is_valid());
        CHECK(copy.count() == 1);
        const iteration_scope scope = world.lock_iteration();
        CHECK(copy.get<const position>().size() == 1);
    }
}

#include "conjure/testing/doctest.h"

#include "ecs_test_fixture.h"

#include "conjure/testing/allocation_scope.h"
#include "conjure/testing/assert_capture.h"

#include "conjure/ecs/ecs_error.h"
#include "conjure/ecs/view.h"
#include "conjure/ecs/world.h"

#include <type_traits>
#include <vector>

using namespace conjure;
using namespace conjure::ecs;
using namespace ecs_test;

TEST_SUITE("ecs/view") {

    TEST_CASE("view - for_each const non const and maybe") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const core::result<entity> moving = world.create_entity(fixture.mask<position, velocity>());
        const core::result<entity> still = world.create_entity(fixture.mask<position>());
        CONJURE_REQUIRE_OK(moving);
        CONJURE_REQUIRE_OK(still);
        CONJURE_REQUIRE_OK(world.set(moving.value(), velocity{1.0f, 2.0f, 3.0f}));

        u32 withVelocity = 0;
        u32 without = 0;
        world.view<position, maybe<const velocity>>().for_each(
          [&](entity id, position &place, const velocity *speed) {
              static_assert(std::is_same_v<decltype(speed), const velocity *>);
              if (speed != nullptr) {
                  CHECK(id == moving.value());
                  place.x += speed->x;
                  place.y += speed->y;
                  ++withVelocity;
              } else {
                  CHECK(id == still.value());
                  ++without;
              }
          }
        );
        CHECK(withVelocity == 1);
        CHECK(without == 1);
        CHECK(world.get<position>(moving.value())->x == 1.0f);
        CHECK(world.get<position>(moving.value())->y == 2.0f);
        CHECK(world.get<position>(still.value())->x == 0.0f);

        u32 reads = 0;
        world.view<const position, const velocity>().for_each(
          [&](entity, const position &position, const velocity &velocity) {
              CHECK(position.x == velocity.x);
              ++reads;
          }
        );
        CHECK(reads == 1);
        CHECK(world.view<const position>().entity_count() == 2);
        CHECK(world.view<const health>().entity_count() == 0);
    }

    TEST_CASE("view - builder chaining") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const core::result<entity> player = world.create_entity(fixture.mask<position, player_tag>());
        const core::result<entity> frozenPlayer =
          world.create_entity(fixture.mask<position, player_tag, frozen_tag>());
        const core::result<entity> plain = world.create_entity(fixture.mask<position>());
        CONJURE_REQUIRE_OK(player);
        CONJURE_REQUIRE_OK(frozenPlayer);
        CONJURE_REQUIRE_OK(plain);

        view<const position> players = world.view<const position>();
        CHECK(players.entity_count() == 3);
        CHECK(players.with<player_tag>().entity_count() == 2);
        CHECK(players.without<frozen_tag>().entity_count() == 1);
        std::vector<entity> seen = {};
        players.for_each([&](entity id, const position &) {
            seen.push_back(id);
        });
        REQUIRE(seen.size() == 1);
        CHECK(seen[0] == player.value());

        const u64 version = world.version();
        CHECK(players.changed<position>(version).entity_count() == 0);
        CHECK(players.changed<position>(0).entity_count() == 1);
        CHECK(players.underlying().desc().with.test(fixture.id<player_tag>()));
        CHECK(players.underlying().desc().without.test(fixture.id<frozen_tag>()));
    }

    TEST_CASE("view - access masks derived from pack") {
        ecs_fixture fixture = {};
        const view<const position, velocity, maybe<const health>, maybe<name_component>>
          mixed(fixture.world);
        const view_access access = mixed.access();
        CHECK(access.reads.test(fixture.id<position>()));
        CHECK(access.reads.test(fixture.id<health>()));
        CHECK_FALSE(access.reads.test(fixture.id<velocity>()));
        CHECK(access.writes.test(fixture.id<velocity>()));
        CHECK(access.writes.test(fixture.id<name_component>()));
        CHECK_FALSE(access.writes.test(fixture.id<position>()));
        CHECK(access.reads.count() == 2);
        CHECK(access.writes.count() == 2);

        view<const position, velocity, maybe<const health>, maybe<name_component>>
          builder(fixture.world);
        const query_desc &desc = builder.underlying().desc();
        CHECK(desc.with == fixture.mask<position, velocity>());
        CHECK(desc.optional == fixture.mask<health, name_component>());
        CHECK(desc.writes == fixture.mask<velocity, name_component>());
    }

    TEST_CASE("view - a tag in the pack is rejected at compile time") {
        // A tag has no column to hand to the callback; view<player_tag> used to compile and index an
        // empty span. The pack rejects it (plain or optional), and with<Tag>() is the way to require it.
        static_assert(!conjure::ecs::detail::HAS_COLUMN<player_tag>);
        static_assert(!conjure::ecs::detail::HAS_COLUMN<const player_tag>);
        static_assert(!conjure::ecs::detail::HAS_COLUMN<maybe<const player_tag>>);
        static_assert(!conjure::ecs::detail::HAS_COLUMN<maybe<frozen_tag>>);
        static_assert(conjure::ecs::detail::HAS_COLUMN<const position>);
        static_assert(conjure::ecs::detail::HAS_COLUMN<maybe<velocity>>);

        ecs_fixture fixture = {};
        world &world = fixture.world;
        CONJURE_REQUIRE_OK(world.create_entity(fixture.mask<position, player_tag>()));
        CONJURE_REQUIRE_OK(world.create_entity(fixture.mask<position>()));
        u32 tagged = 0;
        world.view<const position>().with<player_tag>().for_each([&](entity, const position &) {
            ++tagged;
        });
        CHECK(tagged == 1);
    }

    TEST_CASE("view - structural change during for_each is STRUCTURE_LOCKED") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const core::result<entity> created = world.create_entity(fixture.mask<position>());
        CONJURE_REQUIRE_OK(created);

        testing::expect_assert guard = {};
        u32 visited = 0;
        world.view<position>().for_each([&](entity id, position &) {
            ++visited;
            CHECK(world.is_locked());
            CONJURE_CHECK_ERROR(world.create_entity(), ecs_error::STRUCTURE_LOCKED);
            CONJURE_CHECK_ERROR(world.destroy_entity(id), ecs_error::STRUCTURE_LOCKED);
            CONJURE_CHECK_ERROR(world.add<velocity>(id), ecs_error::STRUCTURE_LOCKED);
        });
        CHECK(visited == 1);
        CHECK(guard.count() == 3 * ASSERTS_PER_VIOLATION);
        CHECK_FALSE(world.is_locked());
        CHECK(world.is_alive(created.value()));
        CHECK(world.entity_count() == 1);
    }

    TEST_CASE("regression - review 6 unregistered component matches nothing") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        CONJURE_REQUIRE_OK(world.create_entity(fixture.mask<position>()));
        CONJURE_REQUIRE_OK(world.create_entity());
        REQUIRE_FALSE(fixture.registry.id_of<unreflected_component>().is_valid());

        testing::expect_assert guard = {};
        u32 visited = 0;
        view<const unreflected_component> missing(world);
        CHECK(guard.count() == 1 * ASSERTS_PER_VIOLATION);
        CHECK_FALSE(missing.is_valid());
        CHECK(missing.entity_count() == 0);
        missing.for_each([&](entity, const unreflected_component &) {
            ++visited;
        });
        CHECK(visited == 0);
        CHECK(missing.access().reads.none());

        // A required pack member poisons the whole view; a builder call does too.
        view<position, const unreflected_component> mixed(world);
        CHECK(guard.count() == 2 * ASSERTS_PER_VIOLATION);
        CHECK(mixed.entity_count() == 0);
        view<const position> builder(world);
        CHECK(builder.with<unreflected_component>().entity_count() == 0);
        CHECK(guard.count() == 3 * ASSERTS_PER_VIOLATION);
        CHECK(world.view<const position>().without<unreflected_component>().entity_count() == 0);
        CHECK(world.view<const position>().changed<unreflected_component>(0).entity_count() == 0);
        CHECK(guard.count() == 5 * ASSERTS_PER_VIOLATION);

        // An optional unregistered component is simply absent everywhere.
        u32 optionalVisits = 0;
        world.view<const position, maybe<const unreflected_component>>().for_each(
          [&](entity, const position &, const unreflected_component *absent) {
              CHECK(absent == nullptr);
              ++optionalVisits;
          }
        );
        CHECK(optionalVisits == 1);
        CHECK(guard.count() == 5 * ASSERTS_PER_VIOLATION);
    }

    TEST_CASE("view - for_each_chunk hands out chunk views") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        std::vector<entity> ids(2000);
        CONJURE_REQUIRE_OK(world.create_entities(fixture.mask<position>(), ids));
        u32 chunks = 0;
        u32 rows = 0;
        world.view<position>().for_each_chunk([&](chunk_view &chunk) {
            ++chunks;
            rows += chunk.count();
            for (position &value : chunk.get<position>()) {
                value.z = 1.0f;
            }
        });
        CHECK(chunks >= 2);
        CHECK(rows == 2000);
        CHECK(world.get<position>(ids[1999])->z == 1.0f);
    }

    TEST_CASE("view - a second run of a view built per call allocates nothing") {
        // The documented idiom builds the view inside the system body on every run (system.h); the
        // query behind it keeps its matches inline, so constructing, iterating and counting a fresh view
        // over a handful of archetypes never touches the heap once the world is warm.
        ecs_fixture fixture = {};
        world &world = fixture.world;
        std::vector<entity> plain(700);
        std::vector<entity> moving(600);
        std::vector<entity> tagged(300);
        CONJURE_REQUIRE_OK(world.create_entities(fixture.mask<position>(), plain));
        CONJURE_REQUIRE_OK(world.create_entities(fixture.mask<position, velocity>(), moving));
        CONJURE_REQUIRE_OK(world.create_entities(fixture.mask<position, velocity, player_tag>(), tagged));

        u32 visited = 0;
        const auto run = [&world, &visited]() {
            visited = 0;
            world.view<position, maybe<const velocity>>().for_each([&visited](entity, position &p, const velocity *v) {
                p.x += v == nullptr ? 1.0f : v->x;
                ++visited;
            });
            visited += world.view<const position>().with<player_tag>().entity_count();
            world.view<const velocity>().for_each_chunk([&visited](chunk_view &chunk) {
                visited += chunk.count();
            });
        };
        run();
        CHECK(visited == 1600 + 300 + 900);
        CONJURE_CHECK_NO_ALLOCATIONS(run());
        CHECK(visited == 1600 + 300 + 900);
    }
}

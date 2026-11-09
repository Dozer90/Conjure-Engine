#include "conjure/testing/doctest.h"

#include "ecs_test_fixture.h"

#include "conjure/testing/assert_capture.h"
#include "conjure/testing/test_environment.h"

#include "conjure/core/random.h"
#include "conjure/ecs/ecs_error.h"
#include "conjure/ecs/world.h"

#include <algorithm>
#include <thread>
#include <unordered_set>
#include <vector>

using namespace conjure;
using namespace conjure::ecs;
using namespace ecs_test;

TEST_SUITE("ecs/world_entities") {

    TEST_CASE("world - create destroy alive") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const core::result<entity> created = world.create_entity();
        CONJURE_REQUIRE_OK(created);
        CHECK_FALSE(created.value().is_null());
        CHECK(created.value().generation == 1);
        CHECK(world.is_alive(created.value()));
        CHECK(world.entity_count() == 1);
        CHECK(world.mask_of(created.value()).none());

        CONJURE_REQUIRE_OK(world.destroy_entity(created.value()));
        CHECK_FALSE(world.is_alive(created.value()));
        CHECK(world.entity_count() == 0);
        CHECK_FALSE(world.is_alive(NULL_ENTITY));
        CHECK_FALSE(world.is_alive(entity{9999, 1}));
    }

    TEST_CASE("world - destroy twice is ENTITY_NOT_ALIVE") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const core::result<entity> created = world.create_entity();
        CONJURE_REQUIRE_OK(created);
        CONJURE_REQUIRE_OK(world.destroy_entity(created.value()));
        CONJURE_CHECK_ERROR(world.destroy_entity(created.value()), ecs_error::ENTITY_NOT_ALIVE);
        CONJURE_CHECK_ERROR(world.destroy_entity(NULL_ENTITY), ecs_error::ENTITY_NOT_ALIVE);
        CONJURE_CHECK_ERROR(world.destroy_entity(entity{12345, 3}), ecs_error::ENTITY_NOT_ALIVE);
    }

    TEST_CASE("world - stale id operations fail") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const core::result<entity> first = world.create_entity();
        CONJURE_REQUIRE_OK(first);
        CONJURE_REQUIRE_OK(world.destroy_entity(first.value()));
        const core::result<entity> second = world.create_entity();
        CONJURE_REQUIRE_OK(second);
        REQUIRE(second.value().index == first.value().index);
        CHECK(second.value().generation == 2);

        const entity stale = first.value();
        CHECK_FALSE(world.is_alive(stale));
        CHECK(world.is_alive(second.value()));
        CONJURE_CHECK_ERROR(world.add<position>(stale), ecs_error::ENTITY_NOT_ALIVE);
        CONJURE_CHECK_ERROR(world.remove<position>(stale), ecs_error::ENTITY_NOT_ALIVE);
        CONJURE_CHECK_ERROR(world.set(stale, position{}), ecs_error::ENTITY_NOT_ALIVE);
        CONJURE_CHECK_ERROR(world.destroy_entity(stale), ecs_error::ENTITY_NOT_ALIVE);
        CONJURE_CHECK_ERROR(world.add_raw(stale, fixture.id<position>()), ecs_error::ENTITY_NOT_ALIVE);
        CONJURE_CHECK_ERROR(world.remove_raw(stale, fixture.id<position>()), ecs_error::ENTITY_NOT_ALIVE);
        CHECK(world.get<position>(stale) == nullptr);
        CHECK(world.get_raw(stale, fixture.id<position>(), true) == nullptr);
        CHECK_FALSE(world.has<position>(stale));
        CHECK(world.mask_of(stale).none());
        CHECK(world.is_alive(second.value()));
    }

    TEST_CASE("world - index reuse after destroy") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        std::vector<entity> ids(10);
        CONJURE_REQUIRE_OK(world.create_entities(component_mask{}, ids));
        CONJURE_REQUIRE_OK(world.destroy_entity(ids[3]));
        CONJURE_REQUIRE_OK(world.destroy_entity(ids[7]));

        const core::result<entity> a = world.create_entity();
        const core::result<entity> b = world.create_entity();
        const core::result<entity> c = world.create_entity();
        CONJURE_REQUIRE_OK(a);
        CONJURE_REQUIRE_OK(b);
        CONJURE_REQUIRE_OK(c);
        CHECK(a.value().index == 7);
        CHECK(a.value().generation == 2);
        CHECK(b.value().index == 3);
        CHECK(b.value().generation == 2);
        CHECK(c.value().index == 10);
        CHECK(c.value().generation == 1);
        CHECK(world.entity_count() == 11);
    }

    TEST_CASE("world - 100k churn keeps record vector at peak") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        std::vector<entity> ids(1000);
        u32 highestIndex = 0;
        for (u32 round = 0; round < 100; ++round) {
            CONJURE_REQUIRE_OK(world.create_entities(fixture.mask<position>(), ids));
            for (const entity id : ids) {
                highestIndex = std::max(highestIndex, id.index);
            }
            for (const entity id : ids) {
                CONJURE_REQUIRE_OK(world.destroy_entity(id));
            }
        }
        CHECK(highestIndex == 999);
        CHECK(world.entity_count() == 0);
        CHECK(ids[0].generation == 100);
        CHECK(world.stats().chunks <= 2);
    }

    TEST_CASE("world - entity_count and stats") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        std::vector<entity> ids(300);
        CONJURE_REQUIRE_OK(world.create_entities(fixture.mask<position, velocity>(), ids));
        CHECK(world.entity_count() == 300);
        CHECK(world.stats().aliveEntities == 300);
        CHECK(world.stats().archetypes == 2);
        for (u32 index = 0; index < 100; ++index) {
            CONJURE_REQUIRE_OK(world.destroy_entity(ids[index]));
        }
        CHECK(world.entity_count() == 200);
        CHECK(world.instance_count(fixture.id<position>()) == 200);
        CHECK(world.instance_count(fixture.id<health>()) == 0);
    }

    TEST_CASE("world - clear fires observers and keeps the id space") {
        tracked::reset_counters();
        ecs_fixture fixture = {};
        world &world = fixture.world;
        std::vector<entity> ids(50);
        CONJURE_REQUIRE_OK(world.create_entities(fixture.mask<tracked, position>(), ids));
        CONJURE_REQUIRE_OK(world.create_entity(fixture.mask<velocity>()));

        u32 removedPositions = 0;
        u32 destroyed = 0;
        const observer_id removed = world.observe(
          fixture.id<position>(),
          component_event::REMOVED,
          [&](ecs::world &, entity, component_id, command_buffer &) {
              ++removedPositions;
          }
        );
        const observer_id onDestroyed = world.observe_destroyed([&](ecs::world &, entity, command_buffer &) {
            ++destroyed;
        });
        CHECK(removed.is_valid());
        CHECK(onDestroyed.is_valid());

        world.clear();
        CHECK(removedPositions == 50);
        CHECK(destroyed == 51);
        CHECK(world.entity_count() == 0);
        CHECK(tracked::live() == 0);
        CHECK(world.stats().chunks == 0);
        CHECK(world.archetypes().size() == 3);
        for (const entity id : ids) {
            CHECK_FALSE(world.is_alive(id));
        }

        // Destroyed indices come back with a bumped generation (the old handles stay stale) and the
        // observers survive.
        const core::result<entity> fresh = world.create_entity(fixture.mask<position>());
        CONJURE_REQUIRE_OK(fresh);
        CHECK(fresh.value().generation == 2);
        for (const entity id : ids) {
            CHECK_FALSE(world.is_alive(id));
        }
        CONJURE_REQUIRE_OK(world.destroy_entity(fresh.value()));
        CHECK(removedPositions == 51);
        CHECK(destroyed == 52);
    }

    TEST_CASE("regression - review 12 clear keeps ids reserved by command buffers valid") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        std::vector<entity> ids(10);
        CONJURE_REQUIRE_OK(world.create_entities(fixture.mask<position>(), ids));

        // A buffer that outlives clear() (a scheduler system's, a script runtime's) holds reserved ids
        // and commands against them.
        command_buffer commands(world);
        const entity reserved = commands.create_entity();
        commands.add<health>(reserved, health{3, 3});
        world.clear();
        CHECK(world.entity_count() == 0);

        // Entities created after the clear never alias the reservation, and the flush still lands.
        std::vector<entity> fresh(70);
        CONJURE_REQUIRE_OK(world.create_entities(fixture.mask<velocity>(), fresh));
        for (const entity id : fresh) {
            CHECK_FALSE(id == reserved);
            CHECK_FALSE(world.has<health>(id));
        }
        CHECK(world.flush(commands).applied == 2);
        REQUIRE(world.is_alive(reserved));
        CHECK(world.get<health>(reserved)->value == 3);
        CHECK(world.entity_count() == 71);
        CONJURE_CHECK_OK(world.validate());
    }

    TEST_CASE("world - wrong thread is refused") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const core::result<entity> created = world.create_entity();
        CONJURE_REQUIRE_OK(created);
        CONJURE_REQUIRE_OK(world.set_owner_thread(std::thread::id()));

        testing::expect_assert guard = {};
        CONJURE_CHECK_ERROR(world.create_entity(), ecs_error::WRONG_THREAD);
        CONJURE_CHECK_ERROR(world.destroy_entity(created.value()), ecs_error::WRONG_THREAD);
        CONJURE_CHECK_ERROR(world.add<position>(created.value()), ecs_error::WRONG_THREAD);
        CONJURE_CHECK_ERROR(world.remove_raw(created.value(), fixture.id<position>()), ecs_error::WRONG_THREAD);
        command_buffer commands(world);
        commands.destroy_entity(created.value());
        CHECK(world.flush(commands).applied == 0);
        CHECK(world.observe_destroyed([](ecs::world &, entity, command_buffer &) {}).is_valid() == false);
        CHECK(guard.count() == 6 * ASSERTS_PER_VIOLATION);
        CHECK(world.is_alive(created.value()));

        CONJURE_REQUIRE_OK(world.set_owner_thread(std::this_thread::get_id()));
        commands.clear();
    }

    TEST_CASE("world - validate after seeded random churn") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        core::random::engine engine(testing::environment().seed);
        std::vector<entity> alive = {};
        const component_mask masks[] = {
            component_mask{},
            fixture.mask<position>(),
            fixture.mask<position, velocity>(),
            fixture.mask<name_component, player_tag>(),
            fixture.mask<tracked, health, odd_component>(),
        };

        for (u32 step = 0; step < 20000; ++step) {
            const u32 action = core::random::uniform(engine, 0u, 9u);
            if (action < 4 || alive.empty()) {
                const u32 which = core::random::uniform(engine, 0u, 4u);
                const core::result<entity> created = world.create_entity(masks[which]);
                CONJURE_REQUIRE_OK(created);
                alive.push_back(created.value());
            } else if (action < 7) {
                const usize victim = core::random::uniform(engine, usize{0}, alive.size() - 1);
                CONJURE_REQUIRE_OK(world.destroy_entity(alive[victim]));
                alive[victim] = alive.back();
                alive.pop_back();
            } else if (action < 9) {
                const usize target = core::random::uniform(engine, usize{0}, alive.size() - 1);
                if (world.has<velocity>(alive[target])) {
                    CONJURE_REQUIRE_OK(world.remove<velocity>(alive[target]));
                } else {
                    CONJURE_REQUIRE_OK(world.add<velocity>(alive[target], velocity{1.0f, 2.0f, 3.0f}));
                }
            } else {
                const usize target = core::random::uniform(engine, usize{0}, alive.size() - 1);
                if (world.has<tracked>(alive[target])) {
                    CONJURE_REQUIRE_OK(world.remove<tracked>(alive[target]));
                } else {
                    CONJURE_REQUIRE_OK(world.add<tracked>(alive[target], tracked(7)));
                }
            }
            if (step % 2500 == 0) {
                CONJURE_CHECK_OK(world.validate());
            }
        }
        CHECK(world.entity_count() == alive.size());
        std::unordered_set<u64> unique = {};
        for (const entity id : alive) {
            CHECK(world.is_alive(id));
            CHECK(unique.insert(id.to_u64()).second);
        }
        CONJURE_CHECK_OK(world.validate());
    }

    TEST_CASE("world - create_entities fills a span and tolerates empty") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        CONJURE_REQUIRE_OK(world.create_entities(fixture.mask<position>(), std::span<entity>()));
        CHECK(world.entity_count() == 0);
        std::vector<entity> ids(700);
        CONJURE_REQUIRE_OK(world.create_entities(fixture.mask<position>(), ids));
        std::unordered_set<u64> unique = {};
        for (const entity id : ids) {
            CHECK(world.is_alive(id));
            CHECK(world.has<position>(id));
            CHECK(unique.insert(id.to_u64()).second);
        }
        CHECK(world.entity_count() == 700);
    }

    TEST_CASE("world - unknown component in a mask is UNKNOWN_COMPONENT") {
        ecs_fixture fixture = {};
        component_mask mask = {};
        mask.set(component_id{500});
        CONJURE_CHECK_ERROR(fixture.world.create_entity(mask), ecs_error::UNKNOWN_COMPONENT);
        CHECK(fixture.world.entity_count() == 0);
    }
}

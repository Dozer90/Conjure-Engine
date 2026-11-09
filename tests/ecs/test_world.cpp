#include "conjure/testing/doctest.h"

#include "ecs_test_fixture.h"

#include "conjure/testing/assert_capture.h"
#include "conjure/testing/memory_guard.h"

#include "conjure/ecs/ecs_error.h"
#include "conjure/ecs/entity_handle.h"
#include "conjure/ecs/world.h"

#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::ecs;
using namespace ecs_test;

TEST_SUITE("ecs/world") {

    TEST_CASE("world - construction captures the owner thread and the registry") {
        component_registry registry = {};
        world_desc desc = {};
        desc.name = "play";
        desc.initialEntityCapacity = 16;
        desc.chunksPerSlab = 4;
        world world(registry, desc);
        CHECK(world.name() == "play");
        CHECK(&world.registry() == &registry);
        CHECK(world.owner_thread() == std::this_thread::get_id());
        CHECK(world.entity_count() == 0);
        CHECK(world.version() == 1);
        CHECK_FALSE(world.is_locked());
        CHECK(world.archetypes().size() == 1);
        CHECK(world.archetypes()[0]->mask().none());
        CONJURE_CHECK_OK(world.validate());
    }

    TEST_CASE("world - destruction returns every chunk and frees components") {
        // Reflection descriptions and interned names are registered process-wide on first use.
        {
            const ecs_fixture warmUp = {};
        }
        tracked::reset_counters();
        testing::memory_guard guard = {};
        {
            ecs_fixture fixture = {};
            std::vector<entity> ids(500);
            CONJURE_REQUIRE_OK(fixture.world.create_entities(fixture.mask<tracked, name_component>(), ids));
            CHECK(tracked::live() == 500);
        }
        CHECK(tracked::live() == 0);
        guard.check();
    }

    TEST_CASE("world - iteration_scope locks and unlocks") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        {
            iteration_scope outer = world.lock_iteration();
            CHECK(world.is_locked());
            CHECK(outer.is_active());
            {
                const iteration_scope inner = world.lock_iteration();
                CHECK(world.is_locked());
            }
            CHECK(world.is_locked());

            iteration_scope moved = std::move(outer);
            CHECK_FALSE(outer.is_active());
            CHECK(moved.is_active());
            CHECK(world.is_locked());
            moved.release();
            CHECK_FALSE(world.is_locked());
            moved.release();
            CHECK_FALSE(world.is_locked());
        }
        CHECK_FALSE(world.is_locked());
        const iteration_scope empty = {};
        CHECK_FALSE(empty.is_active());
    }

    TEST_CASE("world - structural calls while locked assert and return STRUCTURE_LOCKED") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const core::result<entity> created = world.create_entity();
        CONJURE_REQUIRE_OK(created);
        const iteration_scope scope = world.lock_iteration();

        testing::expect_assert guard = {};
        CONJURE_CHECK_ERROR(world.create_entity(), ecs_error::STRUCTURE_LOCKED);
        CONJURE_CHECK_ERROR(world.destroy_entity(created.value()), ecs_error::STRUCTURE_LOCKED);
        CONJURE_CHECK_ERROR(world.add<position>(created.value()), ecs_error::STRUCTURE_LOCKED);
        CONJURE_CHECK_ERROR(world.remove<position>(created.value()), ecs_error::STRUCTURE_LOCKED);
        CONJURE_CHECK_ERROR(world.remove_from_all(fixture.id<position>()), ecs_error::STRUCTURE_LOCKED);
        CONJURE_CHECK_ERROR(world.set_owner_thread(std::this_thread::get_id()), ecs_error::STRUCTURE_LOCKED);
        std::vector<entity> batch(2);
        CONJURE_CHECK_ERROR(world.create_entities(component_mask{}, batch), ecs_error::STRUCTURE_LOCKED);
        command_buffer commands(world);
        commands.destroy_entity(created.value());
        const flush_report report = world.flush(commands);
        CHECK(report.applied == 0);
        CHECK(commands.command_count() == 1);
        world.clear();
        CHECK(world.is_alive(created.value()));
        CHECK(guard.count() == 9 * ASSERTS_PER_VIOLATION);

        // Reads are fine while locked.
        CHECK(world.is_alive(created.value()));
        CHECK(world.mask_of(created.value()).none());
        CHECK_FALSE(world.has<position>(created.value()));
    }

    TEST_CASE("world - set_owner_thread hands ownership over") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        CONJURE_REQUIRE_OK(world.set_owner_thread(std::thread::id()));
        CHECK(world.owner_thread() == std::thread::id());
        {
            testing::expect_assert guard = {};
            CONJURE_CHECK_ERROR(world.create_entity(), ecs_error::WRONG_THREAD);
            CHECK(guard.count() == 1 * ASSERTS_PER_VIOLATION);
        }
        CONJURE_REQUIRE_OK(world.set_owner_thread(std::this_thread::get_id()));
        CONJURE_REQUIRE_OK(world.create_entity());
    }

    TEST_CASE("world - version bumps on structural changes and flushes") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const u64 start = world.version();
        const core::result<entity> created = world.create_entity();
        CONJURE_REQUIRE_OK(created);
        CHECK(world.version() > start);
        const u64 afterCreate = world.version();
        CONJURE_REQUIRE_OK(world.add<position>(created.value()));
        CHECK(world.version() > afterCreate);
        const u64 afterAdd = world.version();
        command_buffer commands(world);
        commands.remove<position>(created.value());
        CHECK(world.flush(commands).applied == 1);
        CHECK(world.version() > afterAdd);
        CHECK(world.bump_version() == world.version());
    }

    TEST_CASE("world - create_query and archetypes expose the storage") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        CONJURE_REQUIRE_OK(world.create_entity(fixture.mask<position>()));
        CONJURE_REQUIRE_OK(world.create_entity(fixture.mask<velocity>()));
        query_desc desc = {};
        desc.with = fixture.mask<position>();
        query positions = world.create_query(desc);
        CHECK(positions.entity_count() == 1);
        CHECK(world.archetypes().size() == 3);
    }

    TEST_CASE("world - stats reflect the contents") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        std::vector<entity> ids(2500);
        CONJURE_REQUIRE_OK(world.create_entities(fixture.mask<position>(), ids));
        const world_stats stats = world.stats();
        CHECK(stats.aliveEntities == 2500);
        CHECK(stats.archetypes == 2);
        CHECK(stats.chunks >= 3);
        CHECK(stats.chunkBytes == static_cast<u64>(stats.chunks) * CHUNK_SIZE);
        CHECK(stats.version == world.version());
    }

    TEST_CASE("world - handle wraps an entity") {
        ecs_fixture fixture = {};
        const core::result<entity> created = fixture.world.create_entity();
        CONJURE_REQUIRE_OK(created);
        entity_handle handle = fixture.world.handle(created.value());
        CHECK(handle.id() == created.value());
        CHECK(handle.is_alive());
        CHECK(handle.owner() == &fixture.world);
    }

    TEST_CASE("world - maxChunkSlabs caps the pool and a refused creation changes nothing") {
        component_registry registry = {};
        register_test_components(registry);
        world_desc desc = {};
        desc.chunksPerSlab = 2;
        desc.maxChunkSlabs = 1;
        world world(registry, desc);
        component_mask positionMask = {};
        positionMask.set(registry.id_of<position>());
        component_mask velocityMask = {};
        velocityMask.set(registry.id_of<velocity>());

        // Two blocks in total: the third archetype to need one is refused and nothing is reserved.
        CONJURE_REQUIRE_OK(world.create_entity());
        const core::result<entity> placed = world.create_entity(positionMask);
        CONJURE_REQUIRE_OK(placed);
        CONJURE_CHECK_ERROR(world.create_entity(velocityMask), ecs_error::OUT_OF_MEMORY);
        std::vector<entity> batch(3);
        CONJURE_CHECK_ERROR(world.create_entities(velocityMask, batch), ecs_error::OUT_OF_MEMORY);
        CHECK(world.entity_count() == 2);
        CHECK(world.stats().chunks == 2);
        CONJURE_CHECK_OK(world.validate());

        // A drained chunk stays as its archetype's spare, so the pool is still full until clear().
        CONJURE_REQUIRE_OK(world.destroy_entity(placed.value()));
        CONJURE_CHECK_ERROR(world.create_entity(velocityMask), ecs_error::OUT_OF_MEMORY);
        world.clear();
        CONJURE_REQUIRE_OK(world.create_entity(velocityMask));
        CONJURE_CHECK_OK(world.validate());
    }

    TEST_CASE("world - default desc values") {
        const world_desc desc = {};
        CHECK(desc.name == "world");
        CHECK(desc.initialEntityCapacity == 1024);
        CHECK(desc.chunksPerSlab == 64);
        CHECK(desc.maxChunkSlabs == 0);
        const world_stats stats = {};
        CHECK(stats.aliveEntities == 0);
        CHECK(stats.version == 0);
    }
}

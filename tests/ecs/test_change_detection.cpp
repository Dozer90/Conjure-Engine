#include "conjure/testing/doctest.h"

#include "ecs_test_fixture.h"

#include "conjure/ecs/command_buffer.h"
#include "conjure/ecs/entity_handle.h"
#include "conjure/ecs/scheduler.h"
#include "conjure/ecs/system.h"
#include "conjure/ecs/view.h"
#include "conjure/ecs/world.h"

#include <vector>

using namespace conjure;
using namespace conjure::ecs;
using namespace ecs_test;

namespace {
    u64 column_version(const world &world, entity target, component_id id) {
        for (const archetype *owner : world.archetypes()) {
            if (owner->mask() != world.mask_of(target)) {
                continue;
            }
            for (u32 chunkIndex = 0; chunkIndex < owner->chunk_count(); ++chunkIndex) {
                for (const entity row : owner->chunk_entities(chunkIndex)) {
                    if (row == target) {
                        return owner->chunk_column_version(chunkIndex, id);
                    }
                }
            }
        }
        return 0;
    }

    u32 count_changed(ecs_fixture &fixture, u64 since) {
        return fixture.world.view<const position>().changed<position>(since).entity_count();
    }
}

TEST_SUITE("ecs/change_detection") {

    TEST_CASE("change_detection - non const span stamps chunk") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const core::result<entity> created = world.create_entity(fixture.mask<position, velocity>());
        CONJURE_REQUIRE_OK(created);
        const u64 creation = column_version(world, created.value(), fixture.id<position>());
        world.bump_version();
        const u64 since = world.version();
        world.bump_version();

        CHECK(count_changed(fixture, since) == 0);
        world.view<position>().for_each([](entity, position &value) {
            value.x = 1.0f;
        });
        CHECK(column_version(world, created.value(), fixture.id<position>()) > creation);
        CHECK(column_version(world, created.value(), fixture.id<position>()) > since);
        CHECK(column_version(world, created.value(), fixture.id<velocity>()) == creation);
        CHECK(count_changed(fixture, since) == 1);
    }

    TEST_CASE("change_detection - const span does not stamp") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const core::result<entity> created = world.create_entity(fixture.mask<position>());
        CONJURE_REQUIRE_OK(created);
        world.bump_version();
        const u64 since = world.version();
        world.bump_version();

        world.view<const position>().for_each([](entity, const position &) {});
        const ecs::world &constWorld = world;
        CHECK(constWorld.get<position>(created.value()) != nullptr);
        CHECK(count_changed(fixture, since) == 0);
        CHECK(column_version(world, created.value(), fixture.id<position>()) < since);
    }

    TEST_CASE("change_detection - changed filter sees chunk once") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        std::vector<entity> ids(1000);
        CONJURE_REQUIRE_OK(world.create_entities(fixture.mask<position>(), ids));
        world.bump_version();
        const u64 since = world.version();
        world.bump_version();

        // Write one entity: its whole chunk counts as changed, nothing else.
        world.get<position>(ids[500])->x = 2.0f;
        const archetype *owner = world.archetypes()[1];
        const u32 capacity = owner->layout().capacity;
        CHECK(count_changed(fixture, since) == owner->chunk_entity_count(500 / capacity));
        u32 chunks = 0;
        world.view<const position>().changed<position>(since).for_each_chunk([&](chunk_view &) {
            ++chunks;
        });
        CHECK(chunks == 1);

        // The next frame's since excludes it again.
        world.bump_version();
        const u64 later = world.version();
        CHECK(count_changed(fixture, later) == 0);
    }

    TEST_CASE("change_detection - moved row counts as changed") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        std::vector<entity> ids(20);
        CONJURE_REQUIRE_OK(world.create_entities(fixture.mask<position>(), ids));
        const core::result<entity> other = world.create_entity(fixture.mask<position, velocity>());
        CONJURE_REQUIRE_OK(other);
        world.bump_version();
        const u64 since = world.version();
        world.bump_version();
        CHECK(count_changed(fixture, since) == 0);

        // Adding a component moves ids[3] into the other archetype: the destination chunk is stamped,
        // and so is the source chunk whose last row filled the hole.
        CONJURE_REQUIRE_OK(world.add<velocity>(ids[3]));
        CHECK(count_changed(fixture, since) == 21);

        world.bump_version();
        const u64 afterMove = world.version();
        world.bump_version();
        // ids[19] filled the hole of ids[3], so ids[18] is now the last row: removing it moves nothing.
        CONJURE_REQUIRE_OK(world.destroy_entity(ids[18]));
        CHECK(count_changed(fixture, afterMove) == 0);
        // Removing the first row pulls the last one forward and stamps the chunk (17 rows remain).
        CONJURE_REQUIRE_OK(world.destroy_entity(ids[0]));
        CHECK(count_changed(fixture, afterMove) == 17);
    }

    TEST_CASE("change_detection - handle write stamps") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const core::result<entity> created = world.create_entity(fixture.mask<position>());
        CONJURE_REQUIRE_OK(created);
        world.bump_version();
        const u64 since = world.version();
        world.bump_version();

        entity_handle handle = world.handle(created.value());
        component_ref<position> reference = handle.component<position>();
        CHECK(reference.read() != nullptr);
        CHECK(count_changed(fixture, since) == 0);
        reference.write()->y = 3.0f;
        CHECK(count_changed(fixture, since) == 1);

        world.bump_version();
        const u64 next = world.version();
        world.bump_version();
        CHECK(count_changed(fixture, next) == 0);
        CONJURE_REQUIRE_OK(reference.set(position{1.0f, 1.0f, 1.0f}));
        CHECK(count_changed(fixture, next) == 1);

        world.bump_version();
        const u64 raw = world.version();
        world.bump_version();
        CHECK(world.get_raw(created.value(), fixture.id<position>(), true) != nullptr);
        CHECK(count_changed(fixture, raw) == 1);
    }

    TEST_CASE("change_detection - system does not see own previous writes") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        std::vector<entity> ids(50);
        CONJURE_REQUIRE_OK(world.create_entities(fixture.mask<position>(), ids));

        scheduler scheduler(world, nullptr);
        std::vector<u32> changedPerRun = {};
        CONJURE_REQUIRE_OK(scheduler.add_system(make_system<position>(
          core::string_id("test.writer"),
          system_phase::UPDATE,
          [&](system_context &context) {
              u32 changed = 0;
              context.view<position>().changed<position>(context.since_version()).for_each(
                [&](entity, position &value) {
                    value.x += 1.0f;
                    ++changed;
                }
              );
              changedPerRun.push_back(changed);
          }
        )));
        const frame_info frame = {};
        scheduler.run_phase(system_phase::UPDATE, frame);
        scheduler.run_phase(system_phase::UPDATE, frame);
        scheduler.run_phase(system_phase::UPDATE, frame);
        REQUIRE(changedPerRun.size() == 3);
        CHECK(changedPerRun[0] == 50);     // creation counts as a change
        CHECK(changedPerRun[1] == 0);      // its own writes are excluded
        CHECK(changedPerRun[2] == 0);

        // An outside writer at a later version is seen on the next run (every structural call, flush
        // and system run bumps the version, so a write between frames always lands past the last run).
        world.bump_version();
        world.get<position>(ids[7])->y = 1.0f;
        scheduler.run_phase(system_phase::UPDATE, frame);
        REQUIRE(changedPerRun.size() == 4);
        CHECK(changedPerRun[3] == 50);
        scheduler.run_phase(system_phase::UPDATE, frame);
        REQUIRE(changedPerRun.size() == 5);
        CHECK(changedPerRun[4] == 0);
    }

    TEST_CASE("change_detection - version is monotonic across flushes") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        u64 previous = world.version();
        command_buffer commands(world);
        for (u32 round = 0; round < 20; ++round) {
            const entity created = commands.create_entity();
            commands.add<position>(created);
            CHECK(world.flush(commands).applied == 2);
            const u64 now = world.version();
            CHECK(now > previous);
            previous = now;
        }
        CONJURE_REQUIRE_OK(world.create_entity());
        CHECK(world.version() > previous);
    }
}

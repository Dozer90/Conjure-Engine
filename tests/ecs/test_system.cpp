#include "conjure/testing/doctest.h"

#include "ecs_test_fixture.h"

#include "conjure/testing/assert_capture.h"

#include "conjure/core/string_id.h"
#include "conjure/ecs/command_buffer.h"
#include "conjure/ecs/system.h"
#include "conjure/ecs/system_phase.h"
#include "conjure/ecs/view.h"
#include "conjure/ecs/world.h"

#include <type_traits>
#include <vector>

using namespace conjure;
using namespace conjure::ecs;
using namespace ecs_test;

TEST_SUITE("ecs/system") {

    TEST_CASE("system_flags - thread modes and switches") {
        static_assert(static_cast<u32>(system_flags::NONE) == 0);
        const system_flags flags = system_flags::MAIN_THREAD | system_flags::RUNS_WHILE_PAUSED;
        CHECK(has_flag(flags, system_flags::MAIN_THREAD));
        CHECK(has_flag(flags, system_flags::RUNS_WHILE_PAUSED));
        CHECK_FALSE(has_flag(flags, system_flags::EXCLUSIVE));
        CHECK_FALSE(has_flag(flags, system_flags::DISABLED));
        CHECK(has_flag(flags, system_flags::NONE));
    }

    TEST_CASE("system_desc - defaults") {
        const system_desc desc = {};
        CHECK(desc.name.is_null());
        CHECK(desc.phase == system_phase::UPDATE);
        CHECK(desc.flags == system_flags::NONE);
        CHECK(desc.reads.none());
        CHECK(desc.writes.none());
        CHECK(desc.after.empty());
        CHECK(desc.before.empty());
        CHECK_FALSE(static_cast<bool>(desc.run));
        CHECK_FALSE(static_cast<bool>(desc.resolveAccess));
    }

    TEST_CASE("make_system - derives reads and writes from the pack") {
        ecs_fixture fixture = {};
        system_desc desc = make_system<const position, velocity, maybe<const health>, maybe<link>>(
          core::string_id("test.derived"),
          system_phase::LATE_UPDATE,
          [](system_context &) {}
        );
        CHECK(desc.name == core::string_id("test.derived"));
        CHECK(desc.phase == system_phase::LATE_UPDATE);
        CHECK(static_cast<bool>(desc.run));
        REQUIRE(static_cast<bool>(desc.resolveAccess));
        CHECK(desc.reads.none());
        desc.resolveAccess(fixture.registry, desc.reads, desc.writes);
        CHECK(desc.reads == fixture.mask<position, health>());
        CHECK(desc.writes == fixture.mask<velocity, link>());

        // Unregistered types are left out instead of poisoning the masks.
        component_registry empty = {};
        component_mask reads = {};
        component_mask writes = {};
        desc.resolveAccess(empty, reads, writes);
        CHECK(reads.none());
        CHECK(writes.none());
    }

    TEST_CASE("system_context - exposes its arguments") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        frame_info frame = {};
        frame.deltaTime = 0.25f;
        frame.fixedDeltaTime = 0.1f;
        frame.frameIndex = 7;
        frame.inFixedStep = true;
        system_desc desc =
          make_system<position>(core::string_id("test.context"), system_phase::UPDATE, [](system_context &) {});
        desc.resolveAccess(fixture.registry, desc.reads, desc.writes);
        system_scratch scratch = {};
        command_buffer commands(world);
        const u64 runVersion = world.bump_version();
        system_context context(world, frame, 3, runVersion, commands, nullptr, desc, scratch, false, 512);

        CHECK(&context.world() == &world);
        CHECK(&static_cast<const system_context &>(context).world() == &world);
        CHECK(context.delta_time() == 0.25f);
        CHECK(context.frame().frameIndex == 7);
        CHECK(context.frame().inFixedStep);
        CHECK(context.since_version() == 3);
        CHECK(context.run_version() == runVersion);
        CHECK(&context.commands() == &commands);
        CHECK(context.jobs() == nullptr);
        CHECK(&context.desc() == &desc);
        CHECK(context.last_grain_count() == 0);

        // A validated view works and stamps with the run version.
        const core::result<entity> created = world.create_entity(fixture.mask<position>());
        CONJURE_REQUIRE_OK(created);
        context.view<position>().for_each([](entity, position &value) {
            value.x = 1.0f;
        });
        CHECK(world.get<position>(created.value())->x == 1.0f);
    }

    TEST_CASE("system_context - undeclared access asserts in Debug") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const frame_info frame = {};
        system_desc desc =
          make_system<const position>(core::string_id("test.reader"), system_phase::UPDATE, [](system_context &) {});
        desc.resolveAccess(fixture.registry, desc.reads, desc.writes);
        system_scratch scratch = {};
        command_buffer commands(world);
        system_context context(world, frame, 0, world.bump_version(), commands, nullptr, desc, scratch, false, 512);
        CONJURE_REQUIRE_OK(world.create_entity(fixture.mask<position, velocity>()));

        testing::expect_assert guard = {};
        // Declared read only: a writing view, a view of an undeclared component and a raw write all assert.
        static_cast<void>(context.view<position>());
        CHECK(guard.count() == 1 * ASSERTS_PER_VIOLATION);
        static_cast<void>(context.view<const velocity>());
        CHECK(guard.count() == 2 * ASSERTS_PER_VIOLATION);
        static_cast<void>(context.view<const position>());
        CHECK(guard.count() == 2 * ASSERTS_PER_VIOLATION);

        query_desc query = {};
        query.with = fixture.mask<position>();
        ecs::query positions = world.create_query(query);
        context.parallel_for_chunks(positions, [&](chunk_view &chunk, command_buffer &) {
            static_cast<void>(chunk.get<const position>());
            CHECK(guard.count() == 2 * ASSERTS_PER_VIOLATION);
            static_cast<void>(chunk.get<position>());
            CHECK(guard.count() == 3 * ASSERTS_PER_VIOLATION);
            static_cast<void>(chunk.column_data(fixture.id<velocity>(), true));
            CHECK(guard.count() == 4 * ASSERTS_PER_VIOLATION);
        });
    }

    TEST_CASE("system_context - a view is bound to its context in every configuration") {
        // The access checks compile out in Distribution; the binding must not, because the view's chunk
        // views stamp the run version through it, whatever the world version has become by then.
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const frame_info frame = {};
        system_desc desc =
          make_system<position>(core::string_id("test.bound"), system_phase::UPDATE, [](system_context &) {});
        desc.resolveAccess(fixture.registry, desc.reads, desc.writes);
        system_scratch scratch = {};
        command_buffer commands(world);
        const u64 runVersion = world.bump_version();
        system_context context(world, frame, 0, runVersion, commands, nullptr, desc, scratch, false, 512);
        CHECK(context.view<const position>().underlying().context() == &context);
        CHECK(world.view<const position>().underlying().context() == nullptr);

        // A structural call after the run started moves the world version past the run version.
        const core::result<entity> created = world.create_entity(fixture.mask<position>());
        CONJURE_REQUIRE_OK(created);
        REQUIRE(world.version() > runVersion);
        u64 stamped = 0;
        context.view<position>().for_each_chunk([&](chunk_view &chunk) {
            CHECK(chunk.stamp_version() == runVersion);
            static_cast<void>(chunk.get<position>());
            stamped = chunk.column_version(fixture.id<position>());
        });
        CHECK(stamped == runVersion);

        // A plain world view stamps the current world version.
        world.view<position>().for_each_chunk([&](chunk_view &chunk) {
            static_cast<void>(chunk.get<position>());
            stamped = chunk.column_version(fixture.id<position>());
        });
        CHECK(stamped == world.version());
    }

    TEST_CASE("system_scratch - grain buffers are retained across dispatches") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        std::vector<entity> ids(3000);
        CONJURE_REQUIRE_OK(world.create_entities(fixture.mask<position>(), ids));
        const frame_info frame = {};
        system_desc desc =
          make_system<position>(core::string_id("test.scratch"), system_phase::UPDATE, [](system_context &) {});
        desc.resolveAccess(fixture.registry, desc.reads, desc.writes);
        system_scratch scratch = {};
        command_buffer commands(world);
        query_desc query = {};
        query.with = fixture.mask<position>();
        ecs::query positions = world.create_query(query);

        for (u32 round = 0; round < 3; ++round) {
            system_context context(world, frame, 0, world.bump_version(), commands, nullptr, desc, scratch, true, 100);
            context.parallel_for_chunks(positions, [](chunk_view &, command_buffer &) {});
            CHECK(scratch.grainBuffers.size() == context.last_grain_count());
            CHECK(scratch.chunks.size() == positions.chunk_count());
        }
        for (const command_buffer &grain : scratch.grainBuffers) {
            CHECK(grain.empty());
        }
    }
}

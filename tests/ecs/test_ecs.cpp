#include "conjure/testing/doctest.h"

#include "ecs_test_fixture.h"

#include "conjure/ecs/ecs.h"

using namespace conjure;
using namespace conjure::ecs;
using namespace ecs_test;

TEST_SUITE("ecs/ecs") {

    TEST_CASE("ecs - umbrella header exposes the whole module") {
        ecs_fixture fixture = {};
        const core::result<entity> created = fixture.world.create_entity(fixture.mask<position>());
        CONJURE_REQUIRE_OK(created);

        // One type from every header: handle, view, command buffer, scheduler, archive, hash, phase.
        entity_handle handle = fixture.world.handle(created.value());
        CHECK(handle.has<position>());

        u32 visited = 0;
        fixture.world.view<const position>().for_each([&](entity, const position &) {
            ++visited;
        });
        CHECK(visited == 1);

        command_buffer commands(fixture.world);
        const entity deferred = commands.create_entity();
        CHECK_FALSE(deferred.is_null());
        const flush_report report = fixture.world.flush(commands);
        CHECK(report.applied == 1);

        scheduler scheduler(fixture.world, nullptr);
        CHECK(scheduler.stages(system_phase::UPDATE).empty());

        const core::result<entity_archive> archive =
          entity_archive::capture(fixture.world, std::span<const entity>(&created.value(), 1));
        CONJURE_REQUIRE_OK(archive);
        CHECK(archive.value().entity_count() == 1);
        CHECK(world_hash(fixture.world) != 0);
        CHECK(to_string(system_phase::UPDATE) == "UPDATE");
        CHECK(ecs_error_name(ecs_error::NONE) == "NONE");
    }
}

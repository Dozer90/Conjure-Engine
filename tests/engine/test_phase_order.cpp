#include "conjure/testing/doctest.h"

#include "engine_test_support.h"

#include "conjure/core/string_id.h"
#include "conjure/core/types.h"
#include "conjure/ecs/command_buffer.h"
#include "conjure/ecs/scheduler.h"
#include "conjure/ecs/system.h"
#include "conjure/ecs/world.h"
#include "conjure/engine/application.h"
#include "conjure/engine/engine_config.h"

#include <memory>
#include <string>
#include <vector>

// The frame sequence (docs/design/06_engine_loop.md 3.1, D10, R2): probe systems record the exact phase
// order with two fixed steps at a forced 1/30 delta, structural changes become visible at the next phase,
// PAUSED skips UPDATE / LATE_UPDATE except for RUNS_WHILE_PAUSED systems, STEP_ONCE runs one frame.

using namespace conjure;
using namespace conjure::engine;

namespace {

    [[nodiscard]] engine_config thirty_hz_frames() {
        engine_config config = engine_test::inline_config();
        config.time.forcedFrameDeltaSeconds = 1.0f / 30.0f;
        return config;
    }
}

TEST_SUITE("engine/phase_order") {

    TEST_CASE("phase_order - probe systems record the exact sequence with two fixed steps at 1/30") {
        std::vector<std::string> log = {};
        application app(engine_test::make_desc(thirty_hz_frames()));
        app.layers().push_layer(std::make_unique<engine_test::probe_layer>("L", log));
        CONJURE_REQUIRE_OK(app.initialise());
        engine_test::add_phase_probes(*app.context().scheduler, log);
        CONJURE_REQUIRE_OK(app.context().scheduler->finalise());

        // Frame 1 runs one fixed step (the first frame measures nothing); frame 2 runs two.
        CONJURE_REQUIRE_OK(app.step_frame());
        log.clear();
        CONJURE_REQUIRE_OK(app.step_frame());
        CHECK(app.time().fixedStepsThisFrame == 2);

        std::vector<std::string> expected = {
            "L.frame_begin", "FRAME_BEGIN",
            "FIXED_UPDATE", "PHYSICS_STEP", "FIXED_END",
            "FIXED_UPDATE", "PHYSICS_STEP", "FIXED_END",
            "UPDATE", "L.update", "LATE_UPDATE", "TRANSFORM", "SYNC",
        };
#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
        // Without a renderer the EXTRACT phase is skipped entirely (06 7); the layers still get their imgui hooks.
#endif
        expected.insert(expected.end(), {"L.imgui", "L.imgui_end", "FRAME_END", "L.frame_end"});
        CHECK(log == expected);
        app.shutdown();
    }

    TEST_CASE("phase_order - an entity created through the command buffer is visible next phase, not the same") {
        application app(engine_test::make_desc(engine_test::inline_config()));
        CONJURE_REQUIRE_OK(app.initialise());
        ecs::scheduler &scheduler = *app.context().scheduler;
        ecs::world &world = *app.context().world;

        u32 countInCreator = 0;
        u32 countSamePhase = 0;
        u32 countNextPhase = 0;
        ecs::entity created = {};

        ecs::system_desc creator = {};
        creator.name = core::string_id::intern("probe.creator");
        creator.phase = ecs::system_phase::UPDATE;
        creator.flags = ecs::system_flags::EXCLUSIVE;
        creator.run = [&](ecs::system_context &context) {
            created = context.commands().create_entity();
            countInCreator = world.entity_count();
        };
        CONJURE_REQUIRE_OK(scheduler.add_system(std::move(creator)));

        ecs::system_desc samePhase = {};
        samePhase.name = core::string_id::intern("probe.same_phase");
        samePhase.phase = ecs::system_phase::UPDATE;
        samePhase.flags = ecs::system_flags::EXCLUSIVE;
        samePhase.after.push_back(core::string_id::intern("probe.creator"));
        samePhase.run = [&](ecs::system_context &) {
            countSamePhase = world.entity_count();
        };
        CONJURE_REQUIRE_OK(scheduler.add_system(std::move(samePhase)));

        ecs::system_desc nextPhase = {};
        nextPhase.name = core::string_id::intern("probe.next_phase");
        nextPhase.phase = ecs::system_phase::LATE_UPDATE;
        nextPhase.flags = ecs::system_flags::EXCLUSIVE;
        nextPhase.run = [&](ecs::system_context &) {
            countNextPhase = world.entity_count();
        };
        CONJURE_REQUIRE_OK(scheduler.add_system(std::move(nextPhase)));
        CONJURE_REQUIRE_OK(scheduler.finalise());

        CONJURE_REQUIRE_OK(app.step_frame());
        CHECK(countInCreator == 0);
        // Exclusive systems each form their own stage: the buffer flushes between them.
        CHECK(countSamePhase >= countInCreator);
        CHECK(countNextPhase == countInCreator + 1);
        CHECK(world.is_alive(created));
        app.shutdown();
    }

    TEST_CASE("phase_order - PAUSED skips UPDATE but runs RUNS_WHILE_PAUSED systems and the later phases") {
        std::vector<std::string> log = {};
        application app(engine_test::make_desc(thirty_hz_frames()));
        CONJURE_REQUIRE_OK(app.initialise());
        engine_test::add_phase_probes(*app.context().scheduler, log, "plain", false);
        engine_test::add_phase_probes(*app.context().scheduler, log, "paused", true);
        CONJURE_REQUIRE_OK(app.context().scheduler->finalise());
        CONJURE_REQUIRE_OK(app.step_frame());

        app.set_simulation_mode(simulation_mode::PAUSED);
        log.clear();
        CONJURE_REQUIRE_OK(app.step_frame());
        CHECK(app.time().fixedStepsThisFrame == 0);
        CHECK(engine_test::filter_log(log, "FIXED_UPDATE").empty());
        CHECK(engine_test::filter_log(log, "PHYSICS_STEP").empty());
        CHECK(engine_test::filter_log(log, "FIXED_END").empty());
        CHECK(engine_test::filter_log(log, "UPDATE:plain").empty());
        CHECK(engine_test::filter_log(log, "LATE_UPDATE:plain").empty());
        CHECK(engine_test::filter_log(log, "UPDATE:paused").size() == 1);
        CHECK(engine_test::filter_log(log, "LATE_UPDATE:paused").size() == 1);
        CHECK(engine_test::filter_log(log, "FRAME_BEGIN:plain").size() == 1);
        CHECK(engine_test::filter_log(log, "TRANSFORM:plain").size() == 1);
        CHECK(engine_test::filter_log(log, "SYNC:plain").size() == 1);
        CHECK(engine_test::filter_log(log, "FRAME_END:plain").size() == 1);
        app.shutdown();
    }

    TEST_CASE("phase_order - STEP_ONCE runs one full frame then pauses") {
        std::vector<std::string> log = {};
        application app(engine_test::make_desc(thirty_hz_frames()));
        CONJURE_REQUIRE_OK(app.initialise());
        engine_test::add_phase_probes(*app.context().scheduler, log);
        CONJURE_REQUIRE_OK(app.context().scheduler->finalise());
        CONJURE_REQUIRE_OK(app.step_frame());

        app.set_simulation_mode(simulation_mode::STEP_ONCE);
        log.clear();
        CONJURE_REQUIRE_OK(app.step_frame());
        CHECK(engine_test::filter_log(log, "FIXED_UPDATE").size() == 2);
        CHECK(engine_test::filter_log(log, "UPDATE").size() == 1);
        CHECK(app.current_simulation_mode() == simulation_mode::PAUSED);

        log.clear();
        CONJURE_REQUIRE_OK(app.step_frame());
        CHECK(engine_test::filter_log(log, "FIXED_UPDATE").empty());
        CHECK(engine_test::filter_log(log, "UPDATE").empty());
        app.shutdown();
    }

    TEST_CASE("phase_order - TRANSFORM and SYNC run while paused") {
        std::vector<std::string> log = {};
        application app(engine_test::make_desc(engine_test::inline_config()));
        CONJURE_REQUIRE_OK(app.initialise());
        engine_test::add_phase_probes(*app.context().scheduler, log);
        CONJURE_REQUIRE_OK(app.context().scheduler->finalise());
        app.set_simulation_mode(simulation_mode::PAUSED);
        CONJURE_REQUIRE_OK(app.step_frame());
        const std::vector<std::string> expected = {"FRAME_BEGIN", "TRANSFORM", "SYNC", "FRAME_END"};
        CHECK(log == expected);
        app.shutdown();
    }

    TEST_CASE("phase_order - frame statistics record the steps and the phases that ran") {
        application app(engine_test::make_desc(thirty_hz_frames()));
        CONJURE_REQUIRE_OK(app.initialise());
        CONJURE_REQUIRE_OK(app.step_frame());
        CONJURE_REQUIRE_OK(app.step_frame());
        const frame_stats &stats = app.stats();
        CHECK(stats.sample_count() == 2);
        CHECK(stats.latest().fixedSteps == 2);
        CHECK(stats.sample_at(1).fixedSteps == 1);
        CHECK(stats.latest().frameMilliseconds >= 0.0f);
        CHECK_FALSE(stats.latest().droppedFixedTime);
        app.shutdown();
    }
}

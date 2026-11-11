#include "conjure/testing/doctest.h"

#include "conjure/testing/approx.h"
#include "engine_test_support.h"

#include "conjure/core/math.h"
#include "conjure/core/string_id.h"
#include "conjure/core/types.h"
#include "conjure/ecs/world.h"
#include "conjure/engine/application.h"
#include "conjure/engine/engine_config.h"
#include "conjure/scene/components.h"
#include "conjure/scene/scene.h"
#include "conjure/scene/scene_manager.h"

#include <cmath>

// Render transforms (docs/design/06_engine_loop.md 3.2, R3, R47): the engine feeds interpolationAlpha to
// scene.propagate_transforms, which blends interpolated_transform into render_world_transform while
// world_transform stays the simulation truth.

using namespace conjure;
using namespace conjure::engine;
using namespace conjure::testing;

namespace {

    /// <summary>
    /// Frames of 1/120 s at a 60 Hz fixed step: frame 1 runs one step (alpha 0), frame 2 adds half a step
    /// (alpha 0.5), frame 3 completes the step (alpha 0).
    /// </summary>
    [[nodiscard]] engine_config half_step_frames(bool interpolate) {
        engine_config config = engine_test::inline_config();
        config.time.forcedFrameDeltaSeconds = 1.0f / 120.0f;
        config.time.interpolatePhysics = interpolate;
        config.physics.enabled = false;
        return config;
    }

    struct posed_entities {
        ecs::entity parent = {};
        ecs::entity child = {};
    };

    [[nodiscard]] posed_entities create_posed(application &app) {
        scene::scene &active = app.context().scenes->active();
        ecs::world &world = *app.context().world;
        posed_entities result = {};
        const core::result<ecs::entity> parent = active.create_entity(core::string_id("body"));
        CONJURE_REQUIRE_OK(parent);
        result.parent = parent.value();
        scene::interpolated_transform poses = {};
        poses.previous.position = math::vec3(0.0f, 0.0f, 0.0f);
        poses.current.position = math::vec3(0.0f, 2.0f, 0.0f);
        CONJURE_REQUIRE_OK(world.add<scene::interpolated_transform>(result.parent, poses));

        const core::result<ecs::entity> child = active.create_entity(core::string_id("wheel"), result.parent);
        CONJURE_REQUIRE_OK(child);
        result.child = child.value();
        scene::local_transform offset = {};
        offset.position = math::vec3(1.0f, 0.0f, 0.0f);
        CONJURE_REQUIRE_OK(active.set_local_transform(result.child, offset));
        return result;
    }

    [[nodiscard]] math::vec3 render_position(const ecs::world &world, ecs::entity target) {
        const scene::render_world_transform *render = world.get<scene::render_world_transform>(target);
        REQUIRE(render != nullptr);
        return math::vec3(render->matrix[3]);
    }

    [[nodiscard]] math::vec3 world_position(const ecs::world &world, ecs::entity target) {
        const scene::world_transform *value = world.get<scene::world_transform>(target);
        REQUIRE(value != nullptr);
        return math::vec3(value->matrix[3]);
    }
}

TEST_SUITE("engine/render_transform") {

    TEST_CASE("render_transform - an interpolated entity sits between previous and current at alpha one half") {
        application app(engine_test::make_desc(half_step_frames(true)));
        CONJURE_REQUIRE_OK(app.initialise());
        const posed_entities posed = create_posed(app);
        const ecs::world &world = *app.context().world;

        CONJURE_REQUIRE_OK(app.step_frame());
        CHECK(app.time().interpolationAlpha == 0.0f);
        CHECK(approx_equal(render_position(world, posed.parent), math::vec3(0.0f, 0.0f, 0.0f)));

        CONJURE_REQUIRE_OK(app.step_frame());
        CHECK(std::abs(app.time().interpolationAlpha - 0.5f) < 1.0e-3f);
        CHECK(approx_equal(render_position(world, posed.parent), math::vec3(0.0f, 1.0f, 0.0f), 1.0e-3f));
        // The simulation truth never moves: nothing wrote local_transform.
        CHECK(approx_equal(world_position(world, posed.parent), math::vec3(0.0f)));
        app.shutdown();
    }

    TEST_CASE("render_transform - a child inherits the interpolated parent pose") {
        application app(engine_test::make_desc(half_step_frames(true)));
        CONJURE_REQUIRE_OK(app.initialise());
        const posed_entities posed = create_posed(app);
        const ecs::world &world = *app.context().world;
        CONJURE_REQUIRE_OK(app.step_frame());
        CONJURE_REQUIRE_OK(app.step_frame());
        CHECK(approx_equal(render_position(world, posed.child), math::vec3(1.0f, 1.0f, 0.0f), 1.0e-3f));
        CHECK(approx_equal(world_position(world, posed.child), math::vec3(1.0f, 0.0f, 0.0f)));
        app.shutdown();
    }

    TEST_CASE("render_transform - interpolatePhysics off yields the current pose") {
        application app(engine_test::make_desc(half_step_frames(false)));
        CONJURE_REQUIRE_OK(app.initialise());
        CHECK_FALSE(app.context().scenes->transforms().interpolation_enabled());
        const posed_entities posed = create_posed(app);
        const ecs::world &world = *app.context().world;
        CONJURE_REQUIRE_OK(app.step_frame());
        CONJURE_REQUIRE_OK(app.step_frame());
        CHECK(approx_equal(render_position(world, posed.parent), math::vec3(0.0f, 2.0f, 0.0f), 1.0e-3f));
        CHECK(approx_equal(render_position(world, posed.child), math::vec3(1.0f, 2.0f, 0.0f), 1.0e-3f));
        app.shutdown();
    }

    TEST_CASE("render_transform - a root without a pose renders where it is") {
        application app(engine_test::make_desc(half_step_frames(true)));
        CONJURE_REQUIRE_OK(app.initialise());
        scene::scene &active = app.context().scenes->active();
        const core::result<ecs::entity> plain = active.create_entity(core::string_id("rock"));
        CONJURE_REQUIRE_OK(plain);
        scene::local_transform where = {};
        where.position = math::vec3(3.0f, 4.0f, 5.0f);
        CONJURE_REQUIRE_OK(active.set_local_transform(plain.value(), where));
        const ecs::world &world = *app.context().world;
        CONJURE_REQUIRE_OK(app.step_frame());
        CONJURE_REQUIRE_OK(app.step_frame());
        CHECK(approx_equal(render_position(world, plain.value()), math::vec3(3.0f, 4.0f, 5.0f)));
        CHECK(approx_equal(world_position(world, plain.value()), math::vec3(3.0f, 4.0f, 5.0f)));
        app.shutdown();
    }
}

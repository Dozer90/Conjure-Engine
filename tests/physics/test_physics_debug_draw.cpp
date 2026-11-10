#include "conjure/testing/doctest.h"
#include "physics_test_fixture.h"

#include "conjure/physics/physics_debug_draw.h"

#include <string>
#include <vector>

using namespace conjure;
using namespace physics_test;

namespace {

    /// <summary>
    /// Counts what the world draws.
    /// </summary>
    class counting_sink final : public physics::physics_debug_draw {
        public:
            u32 lines = 0;
            u32 triangles = 0;
            u32 texts = 0;
            std::vector<std::string> labels = {};

        public:
            counting_sink() = default;
            ~counting_sink() override = default;

        public:
            void line(const math::vec3 &, const math::vec3 &, u32) override {
                ++lines;
            }

            void triangle(const math::vec3 &, const math::vec3 &, const math::vec3 &, u32) override {
                ++triangles;
            }

            void text(const math::vec3 &, std::string_view label, u32, f32) override {
                ++texts;
                labels.emplace_back(label);
            }
    };

    /// <summary>
    /// A sink without text(): the default implementation must draw nothing and not crash.
    /// </summary>
    class minimal_sink final : public physics::physics_debug_draw {
        public:
            u32 primitives = 0;

        public:
            minimal_sink() = default;
            ~minimal_sink() override = default;

        public:
            void line(const math::vec3 &, const math::vec3 &, u32) override {
                ++primitives;
            }

            void triangle(const math::vec3 &, const math::vec3 &, const math::vec3 &, u32) override {
                ++primitives;
            }
    };
}

TEST_SUITE("physics/physics_debug_draw") {

    TEST_CASE("physics_debug_draw_flags - flag operators compose") {
        using physics::physics_debug_draw_flags;
        const physics_debug_draw_flags flags = physics_debug_draw_flags::SHAPES | physics_debug_draw_flags::WIREFRAME;
        CHECK(has_flag(flags, physics_debug_draw_flags::SHAPES));
        CHECK_FALSE(has_flag(flags, physics_debug_draw_flags::CONTACTS));
        CHECK(to_underlying(physics_debug_draw_flags::CONTACTS) == (1u << 7));
    }

    TEST_CASE("physics_debug_draw - the default text implementation is a no-op") {
        minimal_sink sink = {};
        sink.text(math::vec3(0.0f), "label", 0xFFFFFFFFu, 0.5f);
        CHECK(sink.primitives == 0);
    }

    TEST_CASE("physics_debug_draw - a counting sink receives primitives for two bodies and nothing for NONE") {
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs);
        const ecs::entity ground = fixture.spawn_ground();
        const ecs::entity box = fixture.spawn_box(math::vec3(0.0f, 2.0f, 0.0f));
        fixture.run_step();
        REQUIRE(fixture.physics.has_body(ground));
        REQUIRE(fixture.physics.has_body(box));

        counting_sink nothing = {};
        fixture.physics.debug_draw(nothing, physics::physics_debug_draw_flags::NONE);
        CHECK(nothing.lines == 0);
        CHECK(nothing.triangles == 0);
        CHECK(nothing.texts == 0);

        counting_sink shapes = {};
        fixture.physics.debug_draw(
          shapes,
          physics::physics_debug_draw_flags::SHAPES | physics::physics_debug_draw_flags::BOUNDING_BOXES
        );
#if CONJURE_DISTRIBUTION
        CHECK(shapes.lines + shapes.triangles == 0);
#else
        // Jolt was built with its debug renderer in Debug and Development: two boxes produce triangles
        // (shape faces) and lines (bounding boxes).
        CHECK(shapes.lines > 0);
        CHECK(shapes.triangles > 0);
        counting_sink wire = {};
        fixture.physics.debug_draw(
          wire,
          physics::physics_debug_draw_flags::SHAPES | physics::physics_debug_draw_flags::WIREFRAME
        );
        CHECK(wire.lines > 0);
        counting_sink stats = {};
        fixture.physics.debug_draw(
          stats,
          physics::physics_debug_draw_flags::SLEEP_STATE | physics::physics_debug_draw_flags::VELOCITIES
        );
        CHECK(stats.lines + stats.texts + stats.triangles > 0);
#endif
    }

    TEST_CASE("physics_debug_draw - characters and contacts draw through the sink") {
        core::jobs::job_system jobs = {};
        physics::physics_settings settings = test_settings();
        settings.allowSleeping = false;
        physics_fixture fixture(jobs, settings);
        (void)fixture.spawn_ground();
        const ecs::entity box = fixture.spawn_box(math::vec3(0.0f, 0.5f, 0.0f));
        (void)fixture.spawn_character(math::vec3(3.0f, 0.0f, 0.0f));
        fixture.run_steps(10);
        REQUIRE(fixture.physics.has_body(box));
        REQUIRE_FALSE(fixture.physics.contact_events().empty());

        counting_sink contacts = {};
        fixture.physics.debug_draw(contacts, physics::physics_debug_draw_flags::CONTACTS);
#if CONJURE_DISTRIBUTION
        CHECK(contacts.lines == 0);
#else
        CHECK(contacts.lines > 0);
        counting_sink characters = {};
        fixture.physics.debug_draw(characters, physics::physics_debug_draw_flags::CHARACTERS);
        CHECK(characters.triangles + characters.lines > 0);
#endif
    }
}

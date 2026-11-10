#include "conjure/testing/doctest.h"
#include "conjure/testing/log_capture.h"

#include "conjure/core/json/document.h"
#include "conjure/core/log.h"
#include "conjure/core/string_id.h"
#include "conjure/physics/physics_error.h"
#include "conjure/physics/physics_settings.h"

#include <string>

using namespace conjure;
using physics::physics_error;
using physics::physics_settings;

namespace {

    /// <summary>
    /// The project.json "physics" sample of 06_engine_loop.md 2.10 plus every snake_case key.
    /// </summary>
    constexpr std::string_view FULL_SECTION = R"({
        "enabled": true,
        "gravity": [0.0, -3.7, 0.0],
        "max_bodies": 1024,
        "num_body_mutexes": 8,
        "max_body_pairs": 2048,
        "max_contact_constraints": 512,
        "collision_steps": 2,
        "temp_allocator_bytes": 4194304,
        "max_jobs": 128,
        "max_barriers": 4,
        "max_contact_events_per_step": 256,
        "max_queued_commands": 32,
        "max_characters": 8,
        "deterministic": false,
        "allow_sleeping": false,
        "time_before_sleep_seconds": 1.5,
        "point_velocity_sleep_threshold": 0.1,
        "num_velocity_steps": 6,
        "num_position_steps": 3,
        "speculative_contact_distance": 0.05,
        "penetration_slop": 0.01,
        "estimate_contact_impulses": false
    })";

    [[nodiscard]] core::json::document parse(std::string_view text) {
        core::result<core::json::document> parsed = core::json::document::parse(text);
        REQUIRE(parsed.has_value());
        return std::move(parsed).value();
    }

    void check_rejected(const physics_settings &settings, std::string_view field) {
        const core::result<void> outcome = settings.validate();
        CONJURE_CHECK_ERROR(outcome, physics_error::INVALID_SETTINGS);
        if (!outcome.has_value()) {
            CHECK(outcome.get_error().message().find(field) != std::string::npos);
        }
    }
}

TEST_SUITE("physics/physics_settings") {

    TEST_CASE("physics_settings - defaults validate") {
        const physics_settings settings = {};
        CONJURE_CHECK_OK(settings.validate());
        CHECK(settings.gravity == math::vec3(0.0f, -9.81f, 0.0f));
        CHECK(settings.maxBodies == 65536);
        CHECK(settings.layers.count() == 1);
        CHECK(settings.deterministic);
    }

    TEST_CASE("physics_settings - every rejected field produces INVALID_SETTINGS naming it") {
        physics_settings settings = {};
        settings.maxBodies = 0;
        check_rejected(settings, "maxBodies");
        settings = {};
        settings.maxBodies = (u32{1} << 23) + 1;
        check_rejected(settings, "maxBodies");
        // Jolt's caches assert below four entries.
        settings = {};
        settings.maxBodyPairs = 0;
        check_rejected(settings, "maxBodyPairs");
        settings = {};
        settings.maxBodyPairs = 3;
        check_rejected(settings, "maxBodyPairs");
        settings = {};
        settings.maxBodyPairs = (u32{1} << 24) + 1;
        check_rejected(settings, "maxBodyPairs");
        settings = {};
        settings.maxContactConstraints = 0;
        check_rejected(settings, "maxContactConstraints");
        settings = {};
        settings.maxContactConstraints = 3;
        check_rejected(settings, "maxContactConstraints");
        settings = {};
        settings.maxContactConstraints = (u32{1} << 24) + 1;
        check_rejected(settings, "maxContactConstraints");
        settings = {};
        settings.maxBodyPairs = 4;
        settings.maxContactConstraints = 4;
        CONJURE_CHECK_OK(settings.validate());
        settings = {};
        settings.collisionSteps = 0;
        check_rejected(settings, "collisionSteps");
        settings = {};
        settings.tempAllocatorBytes = MIB - 1;
        check_rejected(settings, "tempAllocatorBytes");
        settings = {};
        settings.maxJobs = 63;
        check_rejected(settings, "maxJobs");
        // Jolt's FixedSizeFreeList pages by maxJobs and asserts on anything but a power of two.
        settings = {};
        settings.maxJobs = 100;
        check_rejected(settings, "maxJobs");
        settings = {};
        settings.maxJobs = 128;
        CONJURE_CHECK_OK(settings.validate());
        settings = {};
        settings.maxBarriers = 0;
        check_rejected(settings, "maxBarriers");
        settings = {};
        settings.maxCharacters = 0;
        check_rejected(settings, "maxCharacters");
        settings = {};
        settings.maxContactEventsPerStep = 0;
        check_rejected(settings, "maxContactEventsPerStep");
        settings = {};
        settings.maxQueuedCommands = 0;
        check_rejected(settings, "maxQueuedCommands");
        settings = {};
        settings.numVelocitySteps = 0;
        check_rejected(settings, "numVelocitySteps");
        settings = {};
        settings.numPositionSteps = 0;
        check_rejected(settings, "numPositionSteps");
        settings = {};
        settings.timeBeforeSleepSeconds = -0.1f;
        check_rejected(settings, "timeBeforeSleepSeconds");
        settings = {};
        settings.pointVelocitySleepThreshold = -1.0f;
        check_rejected(settings, "pointVelocitySleepThreshold");
        settings = {};
        settings.speculativeContactDistance = -1.0f;
        check_rejected(settings, "speculativeContactDistance");
        settings = physics::physics_settings();
        settings.penetrationSlop = -0.001f;
        check_rejected(settings, "penetrationSlop");
        settings = {};
        settings.gravity.y = std::numeric_limits<f32>::infinity();
        check_rejected(settings, "gravity");
    }

    TEST_CASE("physics_settings - validate accepts hand-built layer tables and from_json forwards layer errors") {
        // The public table API cannot produce an invalid table (add_layer rejects duplicates and
        // set_collides stays symmetric), so validate() passes for anything built by hand ...
        physics_settings settings = {};
        CONJURE_REQUIRE_OK(settings.layers.add_layer(core::string_id("a")));
        CONJURE_REQUIRE_OK(settings.layers.add_layer(core::string_id("b")));
        settings.layers.set_collides(1, 2, false);
        settings.layers.set_collides(0, 7, false);   // out of range: ignored, the table stays valid
        CONJURE_CHECK_OK(settings.validate());
        CONJURE_CHECK_ERROR(settings.layers.add_layer(core::string_id("a")), physics_error::DUPLICATE_LAYER);
        CONJURE_CHECK_OK(settings.validate());

        // ... and the layer errors of a project.json reach the caller through from_json.
        const core::json::document duplicated = parse(R"({"names": ["default", "default"]})");
        CONJURE_CHECK_ERROR(physics_settings::from_json({}, duplicated.root()), physics_error::DUPLICATE_LAYER);
        const core::json::document unknown = parse(R"({"names": ["default"], "ignore": [["default", "ghost"]]})");
        CONJURE_CHECK_ERROR(physics_settings::from_json({}, unknown.root()), physics_error::UNKNOWN_LAYER);
        const core::json::document empty = parse(R"({"names": []})");
        CONJURE_CHECK_ERROR(physics_settings::from_json({}, empty.root()), physics_error::LAYER_CONFIG_INVALID);
    }

    TEST_CASE("physics_settings - from_json reads the engine loop sample") {
        const core::json::document project = parse(R"({
            "physics": { "gravity": [0.0, -9.81, 0.0], "max_bodies": 65536 },
            "extensions": { "physics_layers": { } } })");
        testing::log_capture logs = {};
        const core::result<physics_settings> read = physics_settings::from_json(
          project.root()["physics"],
          project.root()["extensions"]["physics_layers"]
        );
        // An empty physics_layers object has no "names": the section is malformed, as the design wants
        // (content errors are loud).
        CONJURE_CHECK_ERROR(read, physics_error::LAYER_CONFIG_INVALID);

        const core::result<physics_settings> withoutLayers = physics_settings::from_json(project.root()["physics"], {});
        CONJURE_REQUIRE_OK(withoutLayers);
        CHECK(withoutLayers.value().gravity == math::vec3(0.0f, -9.81f, 0.0f));
        CHECK(withoutLayers.value().maxBodies == 65536);
        CHECK(withoutLayers.value().layers.count() == 1);
        CONJURE_CHECK_OK(withoutLayers.value().validate());
        CHECK(logs.count(core::log_level::WARN) == 0);
    }

    TEST_CASE("physics_settings - from_json reads every key and the layer table") {
        const core::json::document section = parse(FULL_SECTION);
        const core::json::document layers =
          parse(R"({"names": ["default", "player"], "ignore": [["player", "player"]]})");
        const core::result<physics_settings> read = physics_settings::from_json(section.root(), layers.root());
        CONJURE_REQUIRE_OK(read);
        const physics_settings &settings = read.value();
        CHECK(settings.gravity == math::vec3(0.0f, -3.7f, 0.0f));
        CHECK(settings.maxBodies == 1024);
        CHECK(settings.numBodyMutexes == 8);
        CHECK(settings.maxBodyPairs == 2048);
        CHECK(settings.maxContactConstraints == 512);
        CHECK(settings.collisionSteps == 2);
        CHECK(settings.tempAllocatorBytes == 4194304);
        CHECK(settings.maxJobs == 128);
        CHECK(settings.maxBarriers == 4);
        CHECK(settings.maxContactEventsPerStep == 256);
        CHECK(settings.maxQueuedCommands == 32);
        CHECK(settings.maxCharacters == 8);
        CHECK_FALSE(settings.deterministic);
        CHECK_FALSE(settings.allowSleeping);
        CHECK(settings.timeBeforeSleepSeconds == doctest::Approx(1.5f));
        CHECK(settings.pointVelocitySleepThreshold == doctest::Approx(0.1f));
        CHECK(settings.numVelocitySteps == 6);
        CHECK(settings.numPositionSteps == 3);
        CHECK(settings.speculativeContactDistance == doctest::Approx(0.05f));
        CHECK(settings.penetrationSlop == doctest::Approx(0.01f));
        CHECK_FALSE(settings.estimateContactImpulses);
        CHECK(settings.layers.count() == 2);
        CHECK_FALSE(settings.layers.collides(1, 1));
        CONJURE_CHECK_OK(settings.validate());
    }

    TEST_CASE("physics_settings - unknown keys are ignored with a WARN") {
        const core::json::document section = parse(R"({"max_bodies": 10, "wind": 3})");
        testing::log_capture logs = {};
        const core::result<physics_settings> read = physics_settings::from_json(section.root(), {});
        CONJURE_REQUIRE_OK(read);
        CHECK(read.value().maxBodies == 10);
        CHECK(logs.count(core::log_level::WARN) == 1);
        CHECK(logs.contains(core::log_level::WARN, "wind"));
    }

    TEST_CASE("physics_settings - wrong value types are INVALID_SETTINGS") {
        const char *texts[] = {
            R"({"gravity": 3})",
            R"({"gravity": [1, 2]})",
            R"({"max_bodies": "many"})",
            R"({"max_bodies": -1})",
            R"({"deterministic": 1})",
            R"({"time_before_sleep_seconds": "soon"})",
            R"({"temp_allocator_bytes": true})",
            R"([])",
        };
        for (const char *text : texts) {
            CAPTURE(text);
            const core::json::document section = parse(text);
            CONJURE_CHECK_ERROR(physics_settings::from_json(section.root(), {}), physics_error::INVALID_SETTINGS);
        }
    }

    TEST_CASE("physics_settings - a missing physics section yields the defaults") {
        const core::result<physics_settings> read = physics_settings::from_json({}, {});
        CONJURE_REQUIRE_OK(read);
        CHECK(read.value().maxBodies == physics_settings().maxBodies);
        CHECK(read.value().layers == physics::physics_layer_table());
    }
}

#include "conjure/testing/doctest.h"

#include "conjure/core/jobs/job_system.h"
#include "conjure/core/platform_detect.h"
#include "conjure/core/types.h"
#include "conjure/engine/engine_config.h"
#include "conjure/engine/engine_error.h"

#if defined(CONJURE_WITH_AUDIO) && CONJURE_WITH_AUDIO
    #include "conjure/audio/audio_engine.h"
    #include "conjure/audio/audio_types.h"
#endif
#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS
    #include "conjure/physics/physics_settings.h"
#endif

#include <array>
#include <limits>
#include <string>
#include <type_traits>

// engine_config.h (docs/design/06_engine_loop.md 2.3, D8, D9, D13): defaults, headless defaults, every
// validate() rule, the worker-count resolution and the names of the two sections that feed a module's settings.

using namespace conjure;
using namespace conjure::engine;

TEST_SUITE("engine/engine_config") {

    TEST_CASE("engine_config - defaults validate") {
        const engine_config config = engine_config::defaults();
        CONJURE_CHECK_OK(config.validate());
        CHECK_FALSE(config.headless);
        CHECK(config.rendering.enabled);
        CHECK(config.time.fixedDeltaSeconds == 1.0f / 60.0f);
        CHECK(config.window.width == 1600);
        CHECK(config.window.height == 900);
    }

    TEST_CASE("engine_config - headless defaults validate and pick the null devices") {
        const engine_config config = engine_config::headless_defaults();
        CONJURE_CHECK_OK(config.validate());
        CHECK(config.headless);
        CHECK_FALSE(config.rendering.enabled);
        CHECK(config.rendering.backend == rhi_backend_choice::NULL_DEVICE);
        CHECK(config.audio.backend == audio_backend_choice::NULL_DEVICE);
        CHECK_FALSE(config.scripting.enabled);
        CHECK_FALSE(config.assets.hotReload);
        CHECK(config.window.width == 1280);
        CHECK(config.window.height == 720);
        CHECK(config.time.forcedFrameDeltaSeconds == 1.0f / 60.0f);
    }

    TEST_CASE("engine_config - asset hot reload is on by default and off headless") {
        // The file watcher's stat pass runs on a job thread every 500 ms and allocates there; a headless frame
        // (tests, servers, benchmarks) must not carry it (09 item 15a). Interactive runs keep the editor behaviour.
        CHECK(engine_config::defaults().assets.hotReload);
        CHECK_FALSE(engine_config::headless_defaults().assets.hotReload);
        engine_config config = engine_config::headless_defaults();
        config.assets.hotReload = true;
        CONJURE_CHECK_OK(config.validate());
        CHECK(config != engine_config::headless_defaults());
    }

    TEST_CASE("regression - code sweep F626 the physics and audio sections share no name with the module settings") {
        static_assert(std::is_same_v<decltype(engine_config::physics), physics_config>);
        static_assert(std::is_same_v<decltype(engine_config::audio), audio_config>);
        const engine_config config = engine_config::defaults();
        CHECK(config.audio.deviceName.empty());

#if defined(CONJURE_WITH_AUDIO) && CONJURE_WITH_AUDIO
        {
            // A file that pulls in both namespaces names the module struct without qualification. While the
            // engine section was called audio_settings too, the name was ambiguous and this block did not
            // compile; the field the subsystem copies is deviceName on both sides.
            using namespace conjure::audio;
            static_assert(std::is_same_v<audio_settings, conjure::audio::audio_settings>);
            static_assert(std::is_same_v<decltype(audio_config::deviceName), decltype(audio_settings::deviceName)>);
            const audio_settings moduleDefaults = {};
            CHECK(config.audio.sampleRate == moduleDefaults.sampleRate);
            CHECK(config.audio.masterGain == moduleDefaults.masterGain);
            CHECK(config.audio.dopplerFactor == moduleDefaults.dopplerFactor);
            // engine_config.h builds without the audio module, so it repeats the constant as a literal.
            CHECK(config.audio.speedOfSound == DEFAULT_SPEED_OF_SOUND);
        }
#else
        MESSAGE("built without the audio module: the audio_settings half of the name check does not apply");
#endif

#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS
        {
            using namespace conjure::physics;
            static_assert(std::is_same_v<physics_settings, conjure::physics::physics_settings>);
            const physics_settings moduleDefaults = {};
            CHECK(config.physics.gravity == moduleDefaults.gravity);
            CHECK(config.physics.maxBodies == moduleDefaults.maxBodies);
        }
#else
        MESSAGE("built without the physics module: the physics_settings half of the name check does not apply");
#endif
    }

    TEST_CASE("engine_config - validate rejects a non-positive fixed delta") {
        engine_config config = engine_config::defaults();
        config.time.fixedDeltaSeconds = 0.0f;
        CONJURE_CHECK_ERROR(config.validate(), engine_error::INVALID_CONFIG);
        CHECK(config.validate().get_error().message().find("fixedDeltaSeconds") != std::string::npos);
    }

    TEST_CASE("engine_config - validate rejects a max frame delta below the fixed delta") {
        engine_config config = engine_config::defaults();
        config.time.maxFrameDeltaSeconds = config.time.fixedDeltaSeconds * 0.5f;
        CONJURE_CHECK_ERROR(config.validate(), engine_error::INVALID_CONFIG);
        CHECK(config.validate().get_error().message().find("maxFrameDeltaSeconds") != std::string::npos);
    }

    TEST_CASE("engine_config - validate rejects zero fixed steps per frame") {
        engine_config config = engine_config::defaults();
        config.time.maxFixedStepsPerFrame = 0;
        CONJURE_CHECK_ERROR(config.validate(), engine_error::INVALID_CONFIG);
    }

    TEST_CASE("engine_config - validate rejects a negative forced delta and a negative time scale") {
        engine_config config = engine_config::defaults();
        config.time.forcedFrameDeltaSeconds = -0.1f;
        CONJURE_CHECK_ERROR(config.validate(), engine_error::INVALID_CONFIG);
        config = engine_config::defaults();
        config.time.timeScale = -1.0f;
        CONJURE_CHECK_ERROR(config.validate(), engine_error::INVALID_CONFIG);
        config = engine_config::defaults();
        config.time.timeScale = std::numeric_limits<f32>::quiet_NaN();
        CONJURE_CHECK_ERROR(config.validate(), engine_error::INVALID_CONFIG);
    }

    TEST_CASE("engine_config - validate rejects a forced frame delta above maxFrameDeltaSeconds") {
        // begin_frame clamps every delta to maxFrameDeltaSeconds, so a larger forced delta would silently shrink.
        engine_config config = engine_config::defaults();
        config.time.forcedFrameDeltaSeconds = 0.5f;
        const core::result<void> rejected = config.validate();
        CONJURE_CHECK_ERROR(rejected, engine_error::INVALID_CONFIG);
        REQUIRE_FALSE(rejected.has_value());
        CHECK(rejected.get_error().message().find("forcedFrameDeltaSeconds") != std::string::npos);
        config.time.forcedFrameDeltaSeconds = config.time.maxFrameDeltaSeconds;
        CONJURE_CHECK_OK(config.validate());
    }

    TEST_CASE("engine_config - validate rejects a zero window size unless headless") {
        engine_config config = engine_config::defaults();
        config.window.width = 0;
        CONJURE_CHECK_ERROR(config.validate(), engine_error::INVALID_CONFIG);
        engine_config headless = engine_config::headless_defaults();
        headless.window.width = 0;
        headless.window.height = 0;
        CONJURE_CHECK_OK(headless.validate());
    }

    TEST_CASE("engine_config - validate rejects the null device unless headless") {
        engine_config config = engine_config::defaults();
        config.rendering.backend = rhi_backend_choice::NULL_DEVICE;
        CONJURE_CHECK_ERROR(config.validate(), engine_error::INVALID_CONFIG);
    }

    TEST_CASE("engine_config - validate rejects a real backend on a headless application") {
        engine_config config = engine_config::headless_defaults();
        config.rendering.enabled = true;
        config.rendering.backend = rhi_backend_choice::D3D12;
        CONJURE_CHECK_ERROR(config.validate(), engine_error::INVALID_CONFIG);
        config.rendering.backend = rhi_backend_choice::NULL_DEVICE;
        CONJURE_CHECK_OK(config.validate());
    }

    TEST_CASE("engine_config - validate rejects a thread block larger than the arena") {
        engine_config config = engine_config::defaults();
        config.memory.frameThreadBlockBytes = config.memory.frameArenaBytes + 1;
        CONJURE_CHECK_ERROR(config.validate(), engine_error::INVALID_CONFIG);
    }

    TEST_CASE("engine_config - validate rejects a negative master gain and too many workers") {
        engine_config config = engine_config::defaults();
        config.audio.masterGain = -1.0f;
        CONJURE_CHECK_ERROR(config.validate(), engine_error::INVALID_CONFIG);
        config = engine_config::defaults();
        config.jobs.workerCount = 257;
        CONJURE_CHECK_ERROR(config.validate(), engine_error::INVALID_CONFIG);
    }

    TEST_CASE("engine_config - the worker-count bound is job_system's MAX_WORKER_COUNT") {
        // validate() rejects exactly what job_system::start would refuse and names the bound in its message.
        engine_config config = engine_config::headless_defaults();
        config.jobs.workerCount = core::jobs::MAX_WORKER_COUNT;
        CONJURE_CHECK_OK(config.validate());
        config.jobs.workerCount = core::jobs::MAX_WORKER_COUNT + 1;
        const core::result<void> rejected = config.validate();
        CONJURE_CHECK_ERROR(rejected, engine_error::INVALID_CONFIG);
        REQUIRE_FALSE(rejected.has_value());
        const std::string message = rejected.get_error().message();
        CHECK(message.find("jobs.workerCount") != std::string::npos);
        CHECK(message.find(std::to_string(core::jobs::MAX_WORKER_COUNT)) != std::string::npos);
    }

    TEST_CASE("engine_config - debug builds force GPU validation on") {
        const engine_config config = engine_config::defaults();
#if CONJURE_DEBUG
        CHECK(config.rendering.gpuValidation);
#else
        CHECK_FALSE(config.rendering.gpuValidation);
#endif
    }

    TEST_CASE("engine_config - automatic worker count is hardware minus one clamped to the range") {
        jobs_settings automatic = {};
        CHECK(automatic.workerCount == AUTO_WORKER_COUNT);
        CHECK(resolve_worker_count(automatic, 1) == MIN_AUTO_WORKER_COUNT);
        CHECK(resolve_worker_count(automatic, 0) == MIN_AUTO_WORKER_COUNT);
        CHECK(resolve_worker_count(automatic, 9) == 8);
        CHECK(resolve_worker_count(automatic, 64) == MAX_AUTO_WORKER_COUNT);
        jobs_settings explicitCount = {};
        explicitCount.workerCount = 3;
        CHECK(resolve_worker_count(explicitCount, 64) == 3);
        CHECK(resolve_worker_count(automatic) >= MIN_AUTO_WORKER_COUNT);
    }

    TEST_CASE("engine_config - a worker count of zero is the inline job mode and validates") {
        // 08_testing.md D4: 0 = every job runs on the calling thread, the determinism baseline.
        jobs_settings inlineJobs = {};
        inlineJobs.workerCount = 0;
        CHECK(resolve_worker_count(inlineJobs, 64) == 0);
        engine_config config = engine_config::headless_defaults();
        config.jobs.workerCount = 0;
        CONJURE_CHECK_OK(config.validate());
        config.jobs.workerCount = AUTO_WORKER_COUNT;
        CONJURE_CHECK_OK(config.validate());
    }

    TEST_CASE("engine_config - enum names round-trip through parse and to_string") {
        const std::array<rhi_backend_choice, 4> rhiChoices = {
            rhi_backend_choice::AUTO,
            rhi_backend_choice::D3D12,
            rhi_backend_choice::VULKAN,
            rhi_backend_choice::NULL_DEVICE,
        };
        for (const rhi_backend_choice value : rhiChoices) {
            const core::result<rhi_backend_choice> parsed = parse_rhi_backend_choice(to_string(value));
            CONJURE_REQUIRE_OK(parsed);
            CHECK(parsed.value() == value);
        }
        const std::array<audio_backend_choice, 3> audioChoices = {
            audio_backend_choice::AUTO, audio_backend_choice::MINIAUDIO, audio_backend_choice::NULL_DEVICE
        };
        for (const audio_backend_choice value : audioChoices) {
            const core::result<audio_backend_choice> parsed = parse_audio_backend_choice(to_string(value));
            CONJURE_REQUIRE_OK(parsed);
            CHECK(parsed.value() == value);
        }
        const std::array<script_export_mode, 2> exportModes = {
            script_export_mode::FRAMEWORK_DEPENDENT, script_export_mode::SELF_CONTAINED
        };
        for (const script_export_mode value : exportModes) {
            const core::result<script_export_mode> parsed = parse_script_export_mode(to_string(value));
            CONJURE_REQUIRE_OK(parsed);
            CHECK(parsed.value() == value);
        }
        CONJURE_CHECK_ERROR(parse_rhi_backend_choice("metal"), engine_error::BAD_ARGUMENTS);
        CONJURE_CHECK_ERROR(parse_audio_backend_choice("wasapi"), engine_error::BAD_ARGUMENTS);
        CONJURE_CHECK_ERROR(parse_script_export_mode("portable"), engine_error::BAD_ARGUMENTS);
    }
}

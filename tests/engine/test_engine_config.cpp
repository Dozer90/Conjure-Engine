#include "conjure/testing/doctest.h"

#include "conjure/core/jobs/job_system.h"
#include "conjure/core/platform_detect.h"
#include "conjure/core/types.h"
#include "conjure/engine/engine_config.h"
#include "conjure/engine/engine_error.h"

#include <array>
#include <limits>
#include <string>

// engine_config.h (docs/design/06_engine_loop.md 2.3, D8, D9): defaults, headless defaults, every validate()
// rule and the worker-count resolution.

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
        CHECK(config.window.width == 1280);
        CHECK(config.window.height == 720);
        CHECK(config.time.forcedFrameDeltaSeconds == 1.0f / 60.0f);
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

#include "conjure/testing/doctest.h"

#include "conjure/testing/assert_capture.h"
#include "conjure/testing/headless_engine.h"
#include "conjure/testing/log_capture.h"
#include "conjure/testing/log_category.h"
#include "conjure/testing/temp_directory.h"
#include "conjure/testing/test_environment.h"
#include "support_engine_child_runner.h"

#include "conjure/assets/asset_manager.h"
#include "conjure/core/clock.h"
#include "conjure/core/error.h"
#include "conjure/core/jobs/job_system.h"
#include "conjure/core/json/writer.h"
#include "conjure/core/log.h"
#include "conjure/core/result.h"
#include "conjure/core/string_id.h"
#include "conjure/core/types.h"
#include "conjure/core/uuid.h"
#include "conjure/ecs/entity.h"
#include "conjure/engine/application.h"
#include "conjure/engine/engine_context.h"
#include "conjure/engine/engine_time.h"
#include "conjure/engine/project.h"
#include "conjure/platform/input.h"
#include "conjure/platform/input_state.h"
#include "conjure/platform/key.h"
#include "conjure/platform/window.h"
#include "conjure/scene/scene.h"
#include "conjure/scene/scene_error.h"
#include "conjure/scene/scene_manager.h"
#include "conjure/scene/scene_serializer.h"

#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
    #include "conjure/renderer/renderer.h"
#endif
#if defined(CONJURE_WITH_SCRIPTING) && CONJURE_WITH_SCRIPTING
    #include "conjure/scripting/script_host.h"
#endif

#include <algorithm>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

// The managed SDK directory, defined by CMakeLists.txt here when the tree builds the SDK.
#ifndef CONJURE_SUPPORT_ENGINE_TEST_MANAGED_DIR
    #define CONJURE_SUPPORT_ENGINE_TEST_MANAGED_DIR ""
#endif

using namespace conjure;
using namespace conjure::testing;

namespace {
    // Constant-evaluated ids are hashed and never interned (string_id.h); find_entity hashes the same way.
    constexpr core::string_id ROOT_NAME("Root");
    constexpr core::string_id CHILD_NAME("Child");
    constexpr core::string_id OTHER_NAME("Other");

    constexpr std::string_view PROJECT_JSON = R"({
    "schema_version": 1,
    "name": "support_engine_project",
    "assets_root": "assets"
})";

    [[nodiscard]] headless_engine_options quiet_options() {
        headless_engine_options options = {};
        options.enableRenderer = false;
        options.enableAudio = false;
        return options;
    }

    /// <summary>
    /// Builds Root (with Child) and Other in the fixture's scene and returns the saved scene JSON.
    /// </summary>
    [[nodiscard]] std::string build_and_save_small_scene(headless_engine &engine) {
        scene::scene &draft = engine.active_scene();
        draft.set_name("small");
        const core::result<ecs::entity> root = draft.create_entity(ROOT_NAME);
        CONJURE_REQUIRE_OK(root);
        CONJURE_REQUIRE_OK(draft.create_entity(CHILD_NAME, root.value()));
        CONJURE_REQUIRE_OK(draft.create_entity(OTHER_NAME));
        core::json::writer out = {};
        CONJURE_REQUIRE_OK(scene::scene_serializer::save_scene(draft, out));
        return out.finish();
    }

    [[nodiscard]] std::string small_scene_json() {
        headless_engine_options options = quiet_options();
        options.checkMemory = false;   // the returned text outlives this authoring fixture
        headless_engine engine(options);
        return build_and_save_small_scene(engine);
    }

    /// <summary>
    /// Replaces every occurrence of 'from' in 'text' with 'to'.
    /// </summary>
    void replace_all(std::string &text, std::string_view from, std::string_view to) {
        usize position = text.find(from);
        while (position != std::string::npos) {
            text.replace(position, from.size(), to);
            position = text.find(from, position + to.size());
        }
    }

    /// <summary>
    /// Runs one failing child case and asserts that it failed, with the text expected and without a second,
    /// bogus memory_guard failure behind it.
    /// </summary>
    void check_single_failure(std::string_view caseName, std::string_view expectedText) {
        const core::process_result run = run_support_engine_child(caseName);
        CAPTURE(run.standardOutput);
        CHECK(run.exitCode != 0);
        CHECK(run.exitCode != SKIP_EXIT_CODE);
        CHECK(output_contains(run.standardOutput, expectedText));
        CHECK_FALSE(output_contains(run.standardOutput, "memory_guard:"));
        CHECK(failed_assertion_count(run.standardOutput) == std::optional<u32>(1));
    }
}

TEST_SUITE("testing/headless_engine") {

    /*-----------------------------------------------------------------------*/
    // Lifetime

    TEST_CASE("headless_engine - constructs and destroys cleanly, repeatedly") {
        for (u32 cycle = 0; cycle < 3; ++cycle) {
            CAPTURE(cycle);
            headless_engine engine(quiet_options());
            CHECK(engine.frame_index() == 0);
            CHECK(engine.fixed_update_count() == 0);
            CHECK(engine.application().state() == engine::application_state::RUNNING);
            CHECK(engine.context().world != nullptr);
            CHECK(engine.context().scenes != nullptr);
            CHECK(engine.context().checks != nullptr);
            CHECK(engine.context().jobs != nullptr);
            CHECK(engine.context().window != nullptr);
            CHECK(engine.context().input != nullptr);
            CHECK(&engine.world() == engine.context().world);
            CHECK(&engine.scenes() == engine.context().scenes);
            CHECK(&engine.active_scene() == &engine.scenes().active());
            CHECK(&engine.checks() == engine.context().checks);
            CHECK(engine.world().entity_count() == 0);
            CHECK_FALSE(engine.options().enableRenderer);
        }
    }

    TEST_CASE("headless_engine - construction stays under a regression ceiling") {
        // A regression ceiling 50x the 5 ms budget of 08_testing.md 8.2, not the budget itself: 14-26 ms is
        // typical in Debug today and a loaded CI runner must not flake, so only a gross regression fails here.
        // The precise budget belongs to a benchmark. The first start in a process also pays the warm-up
        // (headless_engine.cpp), so the second one is timed.
        constexpr f64 CONSTRUCTION_CEILING_SECONDS = 0.25;
        {
            const headless_engine warm(quiet_options());
        }
        f64 seconds = 0.0;
        {
            const core::stopwatch watch = {};
            const headless_engine engine(quiet_options());
            seconds = watch.elapsed_seconds();
        }

        // Reported after the fixture: doctest keeps a message's text for the rest of the case.
        MESSAGE("headless_engine construction took " << seconds * 1000.0 << " ms");
        CHECK_MESSAGE(
          seconds <= CONSTRUCTION_CEILING_SECONDS,
          "headless_engine construction took " << seconds * 1000.0 << " ms, above the "
            << CONSTRUCTION_CEILING_SECONDS * 1000.0 << " ms regression ceiling (budget 5 ms, 08_testing.md 8.2)"
        );
    }

    TEST_CASE("headless_engine - a construction failure is reported exactly once") {
        SUBCASE("an invalid fixed delta") {
            check_single_failure(
              "support_engine_child - an invalid fixed delta fails construction",
              "headless_engine: application start failed"
            );
        }
        SUBCASE("a missing project directory") {
            check_single_failure(
              "support_engine_child - a missing project directory fails construction",
              "headless_engine: project load failed"
            );
        }
    }

    TEST_CASE("headless_engine - an unallowed ERROR fails the case once with its text") {
        check_single_failure(
          "support_engine_child - an unallowed error logged during the run fails the case",
          "deliberate error text from the child"
        );
    }

    TEST_CASE("headless_engine - a leak during the run names the interning budget") {
        const core::process_result run = run_support_engine_child(
          "support_engine_child - a leak during the run is reported with the interning budget"
        );
        CAPTURE(run.standardOutput);
        CHECK(run.exitCode != 0);
        CHECK(output_contains(run.standardOutput, "headless_engine: 1 allocation(s) still live"));
        CHECK(output_contains(run.standardOutput, "name(s) were interned during the run, which tolerates up to"));
        CHECK(failed_assertion_count(run.standardOutput) == std::optional<u32>(1));
    }

    TEST_CASE("regression - names interned during the run, short and long, are not reported as leaks") {
        // Unique spellings, so they are new to the process-wide table whether this case runs alone (CTest)
        // or after every other case; the long ones exceed every standard library's small-string buffer.
        constexpr std::string_view LONG_ENTITY_NAME = "DirectionalLight_MainSceneKeyLight_Regression";
        constexpr std::string_view SHORT_ENTITY_NAME = "Spawn_Pt_R";
        constexpr std::string_view LONG_LOOSE_NAME = "PlayerSpawnPoint_RegressionProbe_InternedAlone";
        constexpr std::string_view LONG_JSON_NAME = "RootEntity_WithALongDescriptiveName_Regression";
        constexpr std::string_view LONG_JSON_CHILD_NAME = "ChildEntity_WithALongDescriptiveName_Regression";

        // The scene text is authored before the checked fixture so its storage is not part of the run. Its
        // entity names were hashed, never interned, so they are written as "#<hex>" (or as the plain name if
        // an earlier case of this process interned it): either way, to_string() spells the token to replace.
        std::string json = small_scene_json();
        replace_all(json, "\"" + ROOT_NAME.to_string() + "\"", "\"" + std::string(LONG_JSON_NAME) + "\"");
        replace_all(json, "\"" + CHILD_NAME.to_string() + "\"", "\"" + std::string(LONG_JSON_CHILD_NAME) + "\"");
        REQUIRE(json.find(LONG_JSON_NAME) != std::string::npos);

        headless_engine engine(quiet_options());
        CHECK(headless_engine::allocations_per_interned_name() >= 1);

        CONJURE_REQUIRE_OK(engine.active_scene().create_entity(core::string_id::intern(LONG_ENTITY_NAME)));
        CONJURE_REQUIRE_OK(engine.active_scene().create_entity(core::string_id::intern(SHORT_ENTITY_NAME)));
        CONJURE_UNUSED(core::string_id::intern(LONG_LOOSE_NAME));
        engine.step_frames(2);
        CHECK_FALSE(engine.find_entity(LONG_ENTITY_NAME).is_null());
        CHECK_FALSE(engine.find_entity(SHORT_ENTITY_NAME).is_null());

        CONJURE_REQUIRE_OK(engine.load_scene_json(json));
        CHECK(engine.world().entity_count() == 3);
        CHECK_FALSE(engine.find_entity(LONG_JSON_NAME).is_null());
        const std::string childPath = std::string(LONG_JSON_NAME) + "/" + std::string(LONG_JSON_CHILD_NAME);
        CHECK_FALSE(engine.find_entity(childPath).is_null());
        engine.step_frames(2);
    }

    TEST_CASE("headless_engine - allowErrors forwards to the log capture") {
        headless_engine_options options = quiet_options();
        options.allowErrors = true;
        headless_engine engine(options);
        CHECK(engine.logs().errors_allowed());

        CONJURE_LOG_ERROR(LOG_TESTING, "provoked on purpose");

        CHECK(engine.logs().contains(core::log_level::ERR, "provoked on purpose"));
    }

    TEST_CASE("headless_engine - job workers follow the environment by default and explicit counts are honoured") {
        {
            headless_engine engine(quiet_options());
            CHECK(engine.context().jobs->worker_count() == environment().jobThreadCount);
        }
        {
            headless_engine_options options = quiet_options();
            options.jobThreadCount = 3;
            headless_engine engine(options);
            CHECK(engine.context().jobs->worker_count() == 3);
        }
        {
            // 0 is the engine's inline job mode (08_testing.md D4): every job runs on the calling thread.
            headless_engine_options options = quiet_options();
            options.jobThreadCount = 0;
            headless_engine engine(options);
            CHECK(engine.context().jobs->worker_count() == 0);
            engine.step_frames(3);
            CHECK(engine.fixed_update_count() == 3);
        }
    }

    /*-----------------------------------------------------------------------*/
    // Stepping

    TEST_CASE("headless_engine - 60 frames give 60 fixed updates") {
        headless_engine engine = {};

        engine.step_frames(60);

        CHECK(engine.frame_index() == 60);
        CHECK(engine.fixed_update_count() == 60);
        CHECK(engine.application().time().frameIndex == 60);
        CHECK(engine.application().time().fixedDeltaTime == doctest::Approx(1.0f / 60.0f));
    }

    TEST_CASE("headless_engine - one frame at 0.1 s gives 6 fixed updates") {
        headless_engine engine(quiet_options());
        // Frame 0 always runs exactly one fixed step (engine_time.h); the explicit delta applies from frame 1.
        engine.step_frames(1);
        REQUIRE(engine.fixed_update_count() == 1);

        // The engine keeps the fixed delta as f32 (0.016666668 s), so exactly 0.1 s falls 1e-8 short of six
        // steps; a hair above the tenth of a second gives the six updates the design describes.
        engine.step_frames(1, 0.1001);

        CHECK(engine.frame_index() == 2);
        CHECK(engine.fixed_update_count() == 7);
        CHECK(engine.application().time().fixedStepsThisFrame == 6);
    }

    TEST_CASE("headless_engine - step_seconds rounds to whole frames") {
        headless_engine engine(quiet_options());

        engine.step_seconds(0.5);
        CHECK(engine.frame_index() == 30);

        engine.step_seconds(0.0);
        engine.step_seconds(-1.0);
        CHECK(engine.frame_index() == 30);

        engine.step_seconds(1.0 / 120.0);   // half a frame rounds up
        CHECK(engine.frame_index() == 31);
        CHECK(engine.fixed_update_count() == 31);
    }

    TEST_CASE("headless_engine - step_frames(0) is a no-op") {
        headless_engine engine(quiet_options());
        engine.input().key(0, platform::key::W, platform::button_action::PRESS);

        engine.step_frames(0);
        engine.step_frames(0, 0.5);

        CHECK(engine.frame_index() == 0);
        CHECK(engine.fixed_update_count() == 0);
        CHECK(engine.window().pending_event_count() == 0);   // frame 0's events were not delivered
    }

    TEST_CASE("regression - step_seconds asserts on non-finite or absurd durations and steps nothing") {
        headless_engine engine(quiet_options());
        engine.step_frames(1);

        CONJURE_CHECK_ASSERTS(engine.step_seconds(std::numeric_limits<f64>::infinity()));
        CONJURE_CHECK_ASSERTS(engine.step_seconds(std::numeric_limits<f64>::quiet_NaN()));
        CONJURE_CHECK_ASSERTS(engine.step_seconds(1.0e300));
        engine.step_seconds(-std::numeric_limits<f64>::infinity());   // negative: nothing to step, no assert

        CHECK(engine.frame_index() == 1);
        CHECK(engine.fixed_update_count() == 1);
    }

    TEST_CASE("headless_engine - a custom fixed delta is one fixed update per frame") {
        headless_engine_options options = quiet_options();
        options.fixedDeltaSeconds = 0.05;
        headless_engine engine(options);

        engine.step_frames(10);

        CHECK(engine.fixed_update_count() == 10);
        CHECK(engine.application().time().fixedDeltaTime == doctest::Approx(0.05f));
        CHECK(engine.application().time().fixedTime == doctest::Approx(0.5));
    }

    TEST_CASE("headless_engine - the window's simulate controls are delivered by the next frame") {
        headless_engine engine(quiet_options());

        engine.window().simulate_resize(640, 480);
        engine.step_frames(1);

        CHECK(engine.context().window->framebuffer_size() == math::uvec2(640, 480));
        CHECK(engine.window().pending_event_count() == 0);
    }

    /*-----------------------------------------------------------------------*/
    // Input

    TEST_CASE("headless_engine - input script events reach the input state on their frame") {
        headless_engine engine(quiet_options());
        engine.input().key(2, platform::key::W, platform::button_action::PRESS);
        engine.input().key(4, platform::key::W, platform::button_action::RELEASE);
        engine.input().mouse_move(3, 100.0f, 50.0f);
        const platform::input_state &state = engine.context().input->snapshot();

        engine.step_frames(2);
        CHECK_FALSE(state.is_key_down(platform::key::W));

        engine.step_frames(1);   // frame 2
        CHECK(state.is_key_pressed(platform::key::W));
        CHECK(state.is_key_down(platform::key::W));

        engine.step_frames(1);   // frame 3
        CHECK(state.is_key_held(platform::key::W));
        CHECK(state.mousePosition.x == doctest::Approx(100.0f));
        CHECK(state.mousePosition.y == doctest::Approx(50.0f));

        engine.step_frames(1);   // frame 4
        CHECK(state.is_key_released(platform::key::W));
        CHECK_FALSE(state.is_key_down(platform::key::W));
    }

    TEST_CASE("headless_engine - input scheduled at frame_index() after stepping is delivered by the next frame") {
        headless_engine engine(quiet_options());
        const platform::input_state &state = engine.context().input->snapshot();
        engine.step_frames(3);

        // Frames are absolute: frame_index() is the next frame to run, so it is still open.
        engine.input().key(engine.frame_index(), platform::key::W, platform::button_action::PRESS);
        engine.step_frames(1);

        CHECK(engine.input().late_event_count() == 0);
        CHECK(state.is_key_pressed(platform::key::W));
    }

    TEST_CASE("headless_engine - input scheduled for a frame that already ran fails the case once") {
        check_single_failure(
          "support_engine_child - input scheduled for a frame that already ran fails the case",
          "which already ran"
        );
    }

    /*-----------------------------------------------------------------------*/
    // Scenes

    TEST_CASE("headless_engine - load_scene_json rejects malformed json and a bad document") {
        headless_engine_options options = quiet_options();
        options.allowErrors = true;
        headless_engine engine(options);

        const core::result<scene::scene_id> malformed = engine.load_scene_json("{this is not json");
        CHECK_FALSE(malformed.has_value());

        const core::result<scene::scene_id> badFormat = engine.load_scene_json(R"({"format":"bogus","version":1})");
        CONJURE_CHECK_ERROR(badFormat, scene::scene_error::BAD_FORMAT);

        CHECK(engine.world().entity_count() == 0);
        CHECK(engine.scenes().loaded_scenes().empty());
    }

    TEST_CASE("headless_engine - load_scene_json loads a saved scene and find_entity resolves names and paths") {
        headless_engine engine(quiet_options());
        const std::string json = build_and_save_small_scene(engine);
        engine.scenes().unload_all();
        REQUIRE(engine.world().entity_count() == 0);

        const core::result<scene::scene_id> loaded = engine.load_scene_json(json);
        CONJURE_REQUIRE_OK(loaded);
        CHECK(loaded->is_valid());
        CHECK(engine.scenes().is_loaded(loaded.value()));
        CHECK(engine.world().entity_count() == 3);
        CHECK(engine.frame_index() == 0);   // loaders run zero frames

        const ecs::entity root = engine.find_entity("Root");
        const ecs::entity child = engine.find_entity("Root/Child");
        REQUIRE_FALSE(root.is_null());
        REQUIRE_FALSE(child.is_null());
        CHECK(engine.active_scene().tree().parent_of(child) == root);
        CHECK(engine.find_entity("Child") == child);
        CHECK_FALSE(engine.find_entity("Other").is_null());
        CHECK(engine.find_entity("Nobody").is_null());
        CHECK(engine.find_entity("Root/Nobody").is_null());
        CHECK(engine.find_entity("Other/Child").is_null());

        // A second load adds to the world in ADDITIVE mode and replaces it in REPLACE mode.
        CONJURE_REQUIRE_OK(engine.load_scene_json(json, scene::load_mode::ADDITIVE));
        CHECK(engine.world().entity_count() == 6);
        CONJURE_REQUIRE_OK(engine.load_scene_json(json, scene::load_mode::REPLACE));
        CHECK(engine.world().entity_count() == 3);
        CHECK_FALSE(engine.scenes().is_loaded(loaded.value()));

        engine.step_frames(2);
        CHECK(engine.world().entity_count() == 3);
    }

    TEST_CASE("headless_engine - load_scene accepts an absolute path and reports a missing file") {
        temp_directory scratch("headless_scene");
        CONJURE_REQUIRE_OK(scratch.write_text("small.scene.json", small_scene_json()));
        headless_engine engine(quiet_options());

        const core::result<scene::scene_id> loaded = engine.load_scene(scratch.file("small.scene.json").string());
        CONJURE_REQUIRE_OK(loaded);
        CHECK(engine.world().entity_count() == 3);
        CHECK_FALSE(engine.find_entity("Root").is_null());

        CONJURE_CHECK_ERROR(engine.load_scene(scratch.file("absent.scene.json").string()), core::error_code::NOT_FOUND);
        CONJURE_CHECK_ERROR(engine.load_scene("scenes/absent.scene.json"), core::error_code::NOT_FOUND);
        CHECK(engine.world().entity_count() == 3);

        CONJURE_REQUIRE_OK(engine.scenes().unload(loaded.value()));
        CHECK(engine.world().entity_count() == 0);
    }

    TEST_CASE("headless_engine - a project directory mounts its assets and scenes load by path, scheme and uuid") {
        temp_directory scratch("headless_project");
        CONJURE_REQUIRE_OK(scratch.write_text("project.json", PROJECT_JSON));
        CONJURE_REQUIRE_OK(scratch.write_text("assets/scenes/start.scene.json", small_scene_json()));
        headless_engine_options options = quiet_options();
        options.projectDirectory = &scratch.path();
        options.allowErrors = true;   // the unknown uuid below is reported by the asset manager as an ERROR
        headless_engine engine(options);
        REQUIRE(engine.context().project != nullptr);
        CHECK(engine.context().project->description().name == "support_engine_project");
        CHECK(engine.context().filesystem->exists("assets://scenes/start.scene.json"));

        CONJURE_REQUIRE_OK(engine.load_scene("scenes/start.scene.json"));
        CHECK(engine.world().entity_count() == 3);

        CONJURE_REQUIRE_OK(engine.load_scene("assets://scenes/start.scene.json"));
        CHECK(engine.world().entity_count() == 3);

        const auto asset = engine.context().assets->find("assets://scenes/start.scene.json");
        REQUIRE(asset.has_value());
        CONJURE_REQUIRE_OK(engine.load_scene(asset->to_string()));
        CHECK(engine.world().entity_count() == 3);
        CHECK_FALSE(engine.find_entity("Root").is_null());

        CONJURE_CHECK_ERROR(engine.load_scene(core::uuid::generate().to_string()), scene::scene_error::LOAD_FAILED);
        CHECK(engine.logs().contains(core::log_level::ERR, "is not in any source"));
        engine.step_frames(2);
    }

    TEST_CASE("headless_engine - load_scene with a uuid fails when no project is mounted") {
        headless_engine_options options = quiet_options();
        options.allowErrors = true;   // the asset manager reports the unknown asset as an ERROR
        headless_engine engine(options);
        CHECK(engine.context().project == nullptr);

        const core::result<scene::scene_id> loaded = engine.load_scene(core::uuid::generate().to_string());

        CONJURE_CHECK_ERROR(loaded, scene::scene_error::LOAD_FAILED);
        CHECK(engine.world().entity_count() == 0);
        CHECK(engine.scenes().loaded_scenes().empty());
    }

    /*-----------------------------------------------------------------------*/
    // Hashes and checks

    TEST_CASE("headless_engine - world_hash is stable for equal worlds and changes with the world") {
        // One fixture at a time: the platform allows one live window per process (headless_engine.h).
        u64 emptyElsewhere = 0;
        {
            headless_engine other(quiet_options());
            emptyElsewhere = other.world_hash();
        }
        headless_engine engine(quiet_options());
        const u64 empty = engine.world_hash();
        CHECK(empty == emptyElsewhere);

        const core::result<ecs::entity> created = engine.active_scene().create_entity(ROOT_NAME);
        CONJURE_REQUIRE_OK(created);
        const u64 populated = engine.world_hash();
        CHECK(populated != empty);
        CHECK(engine.world_hash() == populated);

        CONJURE_REQUIRE_OK(engine.active_scene().destroy_now(created.value()));
        CHECK(engine.world_hash() == empty);
    }

    TEST_CASE("headless_engine - the check registry records reports") {
        headless_engine engine(quiet_options());
        CHECK(engine.checks().all_passed());
        CHECK(engine.checks().record_count() == 0);

        engine.context().checks->report("support.pass", true, "", engine.frame_index());
        CHECK(engine.checks().all_passed());
        CHECK(engine.checks().record_count() == 1);

        engine.context().checks->report("support.fail", false, "expected", engine.frame_index());
        CHECK_FALSE(engine.checks().all_passed());
        CHECK(engine.checks().failed_count() == 1);
    }

    /*-----------------------------------------------------------------------*/
    // Subsystems

    TEST_CASE("headless_engine - disabled subsystems leave null pointers and default stats") {
        headless_engine_options options = quiet_options();
        options.enablePhysics = false;
        headless_engine engine(options);

        CHECK(engine.context().device == nullptr);
        CHECK(engine.context().renderer == nullptr);
        CHECK_FALSE(engine.renderer_enabled());
        CHECK(engine.context().audio == nullptr);
        CHECK(engine.context().physics == nullptr);
        CHECK(engine.context().scripting == nullptr);

        engine.step_frames(3);

        CHECK(engine.rhi_stats().commandListsSubmitted == 0);
        CHECK(engine.rhi_stats().markerNames.empty());
#if defined(CONJURE_WITH_AUDIO) && CONJURE_WITH_AUDIO
        CHECK(engine.audio_stats().framesRendered == 0);
        CHECK(engine.audio_stats().virtualTimeSeconds == doctest::Approx(0.0));
#endif
        CHECK(engine.frame_index() == 3);
    }

    TEST_CASE("headless_engine - renderer_enabled reports whether a requested renderer is running") {
        headless_engine engine = {};
        CHECK(engine.options().enableRenderer);

        CHECK(engine.renderer_enabled() == (engine.context().renderer != nullptr));
        if (!engine.renderer_enabled()) {
            // No shader index in this build (or the renderer compiled out): the fixture says so instead of
            // leaving a case to wonder why rhi_stats() stays zero.
            CHECK(engine.logs().contains(core::log_level::WARN, "the renderer was requested but is not running"));
            return;
        }
        CHECK_FALSE(engine.logs().contains(core::log_level::WARN, "the renderer was requested but is not running"));
    }

    TEST_CASE("headless_engine - destroying a fixture shuts its job system down before the next one starts") {
        {
            headless_engine_options options = quiet_options();
            options.jobThreadCount = 3;
            headless_engine engine(options);
            REQUIRE(engine.context().jobs->worker_count() == 3);
            engine.step_frames(2);
        }
        // The previous application and its workers are gone (the platform would refuse a second live window
        // otherwise); the new fixture owns a pool of its own size.
        headless_engine_options options = quiet_options();
        options.jobThreadCount = 1;
        headless_engine engine(options);
        CHECK(engine.context().jobs->worker_count() == 1);
        CHECK(core::jobs::job_system::current() == nullptr);   // the test thread is nobody's worker
        engine.step_frames(2);
        CHECK(engine.fixed_update_count() == 2);
    }

#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
    TEST_CASE("headless_engine - the renderer on the null device records submissions") {
        headless_engine engine = {};
        REQUIRE(engine.context().device != nullptr);
        REQUIRE(engine.context().renderer != nullptr);
        CHECK(engine.options().enableRenderer);

        engine.step_frames(5);

        CHECK(engine.context().renderer->frame_count() == 5);
        CHECK(engine.rhi_stats().commandListsSubmitted >= 5);
        CHECK(engine.rhi_stats().commandListsCreated >= 1);
        const u32 afterFive = engine.rhi_stats().commandListsSubmitted;

        engine.step_frames(1);
        CHECK(engine.rhi_stats().commandListsSubmitted > afterFive);
    }
#endif

#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS
    TEST_CASE("headless_engine - physics is attached when enabled") {
        headless_engine engine(quiet_options());
        REQUIRE(engine.context().physics != nullptr);
        engine.step_frames(3);
        CHECK(engine.fixed_update_count() == 3);
    }
#endif

#if defined(CONJURE_WITH_AUDIO) && CONJURE_WITH_AUDIO
    TEST_CASE("headless_engine - audio stats come from the null backend") {
        headless_engine_options options = quiet_options();
        options.enableAudio = true;
        headless_engine engine(options);
        REQUIRE(engine.context().audio != nullptr);

        engine.step_frames(60);

        CHECK(engine.audio_stats().virtualTimeSeconds == doctest::Approx(1.0).epsilon(0.05));
        CHECK(engine.audio_stats().renderCalls > 0);
        CHECK(engine.audio_stats().framesRendered > 0);
    }
#endif

#if defined(CONJURE_WITH_SCRIPTING) && CONJURE_WITH_SCRIPTING
    TEST_CASE("headless_engine - scripting on the null host attaches a runtime") {
        headless_engine_options options = quiet_options();
        options.enableScripting = true;
        options.useNullScriptHost = true;
        headless_engine engine(options);

        REQUIRE(engine.context().scripting != nullptr);
        engine.step_frames(2);
        CHECK(engine.fixed_update_count() == 2);
    }

    TEST_CASE("headless_engine - the DOTNET_RUNTIME probe holds exactly when hostfxr and the managed SDK are present") {
        // Without the probe the library installs, the precondition never holds and every [dotnet] case would
        // skip quietly; the expectation is computed independently from the SDK directory this tree built.
        const std::filesystem::path managed(CONJURE_SUPPORT_ENGINE_TEST_MANAGED_DIR);
        const std::filesystem::path managedAssembly = managed / "Conjure.Managed.dll";
        std::error_code probe = {};
        const bool sdkPresent = !managed.empty() && std::filesystem::is_regular_file(managedAssembly, probe);
        const bool expected = sdkPresent && scripting::is_runtime_available();
        CAPTURE(sdkPresent);

        CHECK(precondition_holds(precondition::DOTNET_RUNTIME) == expected);
    }

    TEST_CASE("headless_engine - [scripting] the real .NET host attaches a runtime and skips the memory check"
      * doctest::timeout(60)) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        headless_engine_options options = quiet_options();
        options.enableScripting = true;
        options.useNullScriptHost = false;
        headless_engine engine(options);   // the CLR keeps its heap: checkMemory is ignored for this run

        REQUIRE(engine.context().scripting != nullptr);
        engine.step_frames(2);
        CHECK(engine.fixed_update_count() == 2);
    }
#endif
}

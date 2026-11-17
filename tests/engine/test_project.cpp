#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/temp_directory.h"
#include "engine_test_support.h"

#include "conjure/core/json/document.h"
#include "conjure/core/log.h"
#include "conjure/core/types.h"
#include "conjure/engine/engine_config.h"
#include "conjure/engine/engine_error.h"
#include "conjure/engine/project.h"

#include <cmath>
#include <filesystem>
#include <string>

// project.h (docs/design/06_engine_loop.md 2.10, R39, R52, R53): load, validation errors, extensions
// preservation, fixed_hz round trips, apply_to and the module manifest listing.

using namespace conjure;
using namespace conjure::engine;
using namespace conjure::testing;

namespace {

    constexpr std::string_view FULL_PROJECT = R"({
  "schema_version": 1,
  "name": "full",
  "version": "1.2.3",
  "assets_root": "content",
  "cooked_root": "out/cooked",
  "startup_scene": "levels/main.scene.json",
  "modules": { "search_paths": ["plugins"], "engine_plugins": false, "required": ["com.studio.core"] },
  "window": { "title": "Full", "width": 1920, "height": 1080, "fullscreen": true, "resizable": false, "vsync": false },
  "time": { "fixed_hz": 120, "max_frame_delta_seconds": 0.25, "max_fixed_steps_per_frame": 4, "time_scale": 1.5,
            "interpolate_physics": false },
  "physics": { "enabled": true, "gravity": [0, -3.7, 0], "max_bodies": 1024, "collision_steps": 2 },
  "rendering": { "enabled": true, "backend": "vulkan", "gpu_validation": true },
  "audio": { "enabled": true, "backend": "miniaudio", "sample_rate": 44100, "device": "Speakers", "master_gain": 0.5,
             "doppler_factor": 1.0, "speed_of_sound": 340.0,
             "groups": [{ "name": "music", "gain": 0.8 }, { "name": "voice", "parent": "music", "gain": 0.9 }] },
  "assets": { "hot_reload": false },
  "scripting": { "enabled": true, "required": false, "assembly": "Scripts/bin/Game.dll", "hot_reload": false,
                 "export_mode": "self-contained", "debug_symbols": true },
  "layers": ["default", "player", "enemies"],
  "extensions": { "physics_layers": { "names": ["default", "player"] }, "build": "distribution" },
  "custom_tool": { "answer": 42 }
})";
}

TEST_SUITE("engine/project") {

    TEST_CASE("project - parse reads every section") {
        log_capture logs = {};
        const core::result<project> parsed = project::parse(FULL_PROJECT, std::filesystem::path("C:/projects/full"));
        CONJURE_REQUIRE_OK(parsed);
        const project_description &desc = parsed.value().description();
        CHECK(desc.name == "full");
        CHECK(desc.version == "1.2.3");
        CHECK(desc.assetsRoot.generic_string() == "content");
        CHECK(desc.cookedRoot.generic_string() == "out/cooked");
        CHECK(desc.startupScene.generic_string() == "levels/main.scene.json");
        REQUIRE(desc.modules.searchPaths.size() == 1);
        CHECK(desc.modules.searchPaths[0].generic_string() == "plugins");
        CHECK_FALSE(desc.modules.enginePlugins);
        CHECK(desc.modules.required == std::vector<std::string>{"com.studio.core"});
        CHECK(desc.window.width == 1920);
        CHECK(desc.window.fullscreen);
        CHECK_FALSE(desc.window.resizable);
        CHECK_FALSE(desc.window.vsync);
        CHECK(std::abs(desc.time.fixedDeltaSeconds - 1.0f / 120.0f) < 1.0e-7f);
        CHECK(desc.time.maxFrameDeltaSeconds == 0.25f);
        CHECK(desc.time.maxFixedStepsPerFrame == 4);
        CHECK(desc.time.timeScale == 1.5f);
        CHECK_FALSE(desc.time.interpolatePhysics);
        CHECK(desc.physics.gravity.y == -3.7f);
        CHECK(desc.physics.maxBodies == 1024);
        CHECK(desc.physicsSection.root()["collision_steps"].as_u64() == 2);
        CHECK(desc.rendering.backend == rhi_backend_choice::VULKAN);
        CHECK(desc.rendering.gpuValidation);
        CHECK(desc.audio.backend == audio_backend_choice::MINIAUDIO);
        CHECK(desc.audio.sampleRate == 44100);
        CHECK(desc.audio.deviceName == "Speakers");
        REQUIRE(desc.audio.groups.size() == 2);
        CHECK(desc.audio.groups[1].parent == "music");
        CHECK_FALSE(desc.assets.hotReload);
        CHECK(desc.scripting.gameAssembly.generic_string() == "Scripts/bin/Game.dll");
        CHECK(desc.scripting.exportMode == script_export_mode::SELF_CONTAINED);
        CHECK(desc.scripting.exportDebugSymbols);
        CHECK(desc.layers.size() == 3);
        CHECK(desc.extensions.root()["physics_layers"]["names"][1].as_string() == "player");
        CHECK(desc.extensions.root()["build"].as_string() == "distribution");
        // The unknown top-level key moved into extensions with a warning.
        CHECK(desc.extensions.root()["custom_tool"]["answer"].as_u64() == 42);
        CHECK(logs.contains(core::log_level::WARN, "custom_tool"));
        CHECK(parsed.value().assets_directory().generic_string() == "C:/projects/full/content");
        CHECK(parsed.value().cooked_directory().generic_string() == "C:/projects/full/out/cooked");
    }

    TEST_CASE("project - load reads the file and resolves relative paths against the root") {
        temp_directory directory("engine_project");
        const project loaded = engine_test::write_project(directory.path(), "loaded", "main.scene.json");
        CHECK(loaded.description().name == "loaded");
        CHECK(loaded.root_directory() == std::filesystem::absolute(directory.path()).lexically_normal());
        CHECK(loaded.assets_directory() == loaded.root_directory() / "assets");
        CHECK(loaded.file_path() == loaded.root_directory() / "project.json");
    }

    TEST_CASE("project - a missing file is PROJECT_NOT_FOUND") {
        temp_directory directory("engine_project_missing");
        CONJURE_CHECK_ERROR(project::load(directory.path() / "absent"), engine_error::PROJECT_NOT_FOUND);
    }

    TEST_CASE("project - malformed JSON is PROJECT_PARSE_FAILED and reports the position") {
        const core::result<project> parsed = project::parse("{ \"name\": \"x\", ", std::filesystem::path("."));
        CONJURE_CHECK_ERROR(parsed, engine_error::PROJECT_PARSE_FAILED);
        CHECK_FALSE(parsed.get_error().message().empty());
    }

    TEST_CASE("project - a newer schema is rejected") {
        CONJURE_CHECK_ERROR(
          project::parse(R"({"schema_version": 99, "name": "x"})", std::filesystem::path(".")),
          engine_error::PROJECT_SCHEMA_TOO_NEW
        );
    }

    TEST_CASE("project - a missing name and wrong types are PROJECT_INVALID") {
        const std::filesystem::path here(".");
        CONJURE_CHECK_ERROR(project::parse(R"({"schema_version": 1})", here), engine_error::PROJECT_INVALID);
        CONJURE_CHECK_ERROR(
          project::parse(R"({"schema_version": 1, "name": "x", "window": { "width": "wide" }})", here),
          engine_error::PROJECT_INVALID
        );
        CONJURE_CHECK_ERROR(
          project::parse(R"({"schema_version": 1, "name": "x", "time": { "fixed_hz": 0 }})", here),
          engine_error::PROJECT_INVALID
        );
        CONJURE_CHECK_ERROR(
          project::parse(R"({"schema_version": 1, "name": "x", "rendering": { "backend": "metal" }})", here),
          engine_error::PROJECT_INVALID
        );
        CONJURE_CHECK_ERROR(project::parse(R"([1, 2])", std::filesystem::path(".")), engine_error::PROJECT_INVALID);
    }

    TEST_CASE("project - unknown keys survive a save and fixed_hz round-trips") {
        log_capture logs = {};
        temp_directory directory("engine_project_save");
        {
            core::result<project> parsed = project::parse(FULL_PROJECT, directory.path());
            CONJURE_REQUIRE_OK(parsed);
            CONJURE_REQUIRE_OK(parsed.value().save());
        }
        const core::result<project> reloaded = project::load(directory.path());
        CONJURE_REQUIRE_OK(reloaded);
        const project_description &desc = reloaded.value().description();
        CHECK(desc.extensions.root()["custom_tool"]["answer"].as_u64() == 42);
        CHECK(desc.extensions.root()["build"].as_string() == "distribution");
        CHECK(std::abs(desc.time.fixedDeltaSeconds - 1.0f / 120.0f) < 1.0e-7f);
        CHECK(desc.physicsSection.root()["collision_steps"].as_u64() == 2);
        CHECK(desc.audio.groups.size() == 2);
        CHECK(desc.layers.size() == 3);
        const core::json::document written = reloaded.value().to_json();
        CHECK(written.root()["time"]["fixed_hz"].as_f64() == 120.0);
        CHECK(written.root()["cooked"].as_bool() == false);
        CHECK(written.root()["assets"]["hot_reload"].as_bool() == false);
        CHECK_FALSE(logs.contains(core::log_level::WARN, "'assets'"));
    }

    TEST_CASE("project - the audio device is the key \"device\" in the file and deviceName in the section") {
        // The section was renamed (code sweep F626); the file format was not.
        core::result<project> parsed = project::parse(FULL_PROJECT, std::filesystem::path("."));
        CONJURE_REQUIRE_OK(parsed);
        CHECK(parsed.value().description().audio.deviceName == "Speakers");

        parsed.value().description().audio.deviceName = "Headset";
        const core::json::document written = parsed.value().to_json();
        CHECK(written.root()["audio"]["device"].as_string() == "Headset");
        CHECK_FALSE(written.root()["audio"]["device_name"].is_valid());
        CHECK_FALSE(written.root()["audio"]["deviceName"].is_valid());

        engine_config config = engine_config::defaults();
        parsed.value().apply_to(config);
        CHECK(config.audio.deviceName == "Headset");

        // A wrong type is refused under the name the file uses.
        const core::result<project> wrong = project::parse(
          R"({"schema_version": 1, "name": "wrong", "audio": {"device": 7}})",
          std::filesystem::path(".")
        );
        CONJURE_CHECK_ERROR(wrong, engine_error::PROJECT_INVALID);
    }

    TEST_CASE("project - the assets section defaults to hot reload on and rejects a non-boolean") {
        const core::result<project> bare = project::parse(
          R"({"schema_version": 1, "name": "bare"})",
          std::filesystem::path(".")
        );
        CONJURE_REQUIRE_OK(bare);
        CHECK(bare.value().description().assets.hotReload);
        const core::result<project> bad = project::parse(
          R"({"schema_version": 1, "name": "bad", "assets": {"hot_reload": 1}})",
          std::filesystem::path(".")
        );
        CONJURE_CHECK_ERROR(bad, engine_error::PROJECT_INVALID);

        // Schema 1 reserves "assets": an unknown member of the section parses (no error, no WARN) and is dropped by
        // to_json like the unknown members of every other section; only a top-level unknown key reaches extensions.
        log_capture logs = {};
        const core::result<project> extra = project::parse(
          R"({"schema_version": 1, "name": "extra", "assets": {"hot_reload": false, "cache_dir": ".cache"}})",
          std::filesystem::path(".")
        );
        CONJURE_REQUIRE_OK(extra);
        CHECK_FALSE(extra.value().description().assets.hotReload);
        CHECK_FALSE(logs.contains(core::log_level::WARN, "cache_dir"));
        const core::json::document written = extra.value().to_json();
        CHECK(written.root()["assets"]["hot_reload"].as_bool() == false);
        CHECK_FALSE(written.root()["assets"]["cache_dir"].is_valid());
        CHECK_FALSE(written.root()["extensions"]["assets"].is_valid());
        CHECK_FALSE(written.root()["extensions"]["cache_dir"].is_valid());
    }

    TEST_CASE("project - apply_to copies the project settings and honours headless") {
        core::result<project> parsed = project::parse(FULL_PROJECT, std::filesystem::path("."));
        CONJURE_REQUIRE_OK(parsed);

        engine_config interactive = engine_config::defaults();
        parsed.value().apply_to(interactive);
        CHECK(interactive.window.width == 1920);
        CHECK(interactive.rendering.backend == rhi_backend_choice::VULKAN);
        CHECK(interactive.audio.backend == audio_backend_choice::MINIAUDIO);
        CHECK(interactive.scripting.enabled);
        CHECK_FALSE(interactive.assets.hotReload);
        CHECK(interactive.scripting.exportMode == script_export_mode::SELF_CONTAINED);
        CHECK(interactive.startupSceneOverride.generic_string() == "levels/main.scene.json");
        CHECK(interactive.modules.required == std::vector<std::string>{"com.studio.core"});

        engine_config headless = engine_config::headless_defaults();
        parsed.value().apply_to(headless);
        CHECK(headless.window.width == 1280);
        CHECK(headless.rendering.backend == rhi_backend_choice::NULL_DEVICE);
        CHECK(headless.audio.backend == audio_backend_choice::NULL_DEVICE);
        CHECK_FALSE(headless.scripting.enabled);
        CHECK(headless.time.forcedFrameDeltaSeconds == 1.0f / 60.0f);

        // A project asking for hot reload never switches the watcher on under headless (D8, 09 item 15a).
        core::result<project> watching = project::parse(
          R"({"schema_version": 1, "name": "watching", "assets": {"hot_reload": true}})",
          std::filesystem::path(".")
        );
        CONJURE_REQUIRE_OK(watching);
        engine_config headlessWatching = engine_config::headless_defaults();
        watching.value().apply_to(headlessWatching);
        CHECK_FALSE(headlessWatching.assets.hotReload);
        engine_config interactiveWatching = engine_config::defaults();
        interactiveWatching.assets.hotReload = false;
        watching.value().apply_to(interactiveWatching);
        CHECK(interactiveWatching.assets.hotReload);
        CHECK(std::abs(headless.time.fixedDeltaSeconds - 1.0f / 120.0f) < 1.0e-7f);

        engine_config overridden = engine_config::defaults();
        overridden.startupSceneOverride = "other.scene.json";
        parsed.value().apply_to(overridden);
        CHECK(overridden.startupSceneOverride.generic_string() == "other.scene.json");
    }

    TEST_CASE("project - a headless apply keeps the forced delta within the frame clamp") {
        // The headless 1/60 forced delta is above this project's 1/120 clamp: apply_to lowers it to the clamp,
        // the delta begin_frame would have used anyway, so the configuration still validates.
        const core::result<project> parsed = project::parse(
          R"({"schema_version": 1, "name": "fast", "time": {"fixed_hz": 240, "max_frame_delta_seconds": 0.008333333}})",
          std::filesystem::path(".")
        );
        CONJURE_REQUIRE_OK(parsed);
        engine_config headless = engine_config::headless_defaults();
        parsed.value().apply_to(headless);
        CHECK(headless.time.maxFrameDeltaSeconds == 0.008333333f);
        CHECK(headless.time.forcedFrameDeltaSeconds == headless.time.maxFrameDeltaSeconds);
        CONJURE_CHECK_OK(headless.validate());
    }

    TEST_CASE("project - saved floats are the shortest decimal that reads back as the same f32") {
        const core::result<project> parsed = project::parse(
          R"({"schema_version":1,"name":"x","time":{"max_frame_delta_seconds":0.1},"physics":{"gravity":[0,-9.81,0]}})",
          std::filesystem::path(".")
        );
        CONJURE_REQUIRE_OK(parsed);
        const std::string text = parsed.value().to_json().to_string();
        CHECK(text.find("0.1") != std::string::npos);
        CHECK(text.find("-9.81") != std::string::npos);
        CHECK(text.find("0.10000000149011612") == std::string::npos);
        CHECK(text.find("-9.8100004196167") == std::string::npos);

        const core::result<project> reread = project::parse(text, std::filesystem::path("."));
        CONJURE_REQUIRE_OK(reread);
        CHECK(reread.value().description().time.maxFrameDeltaSeconds == 0.1f);
        CHECK(reread.value().description().physics.gravity.y == -9.81f);
    }

    TEST_CASE("project - a vec3 component outside the f32 range is PROJECT_INVALID") {
        CONJURE_CHECK_ERROR(
          project::parse(
            R"({"schema_version": 1, "name": "x", "physics": {"gravity": [0, -1e39, 0]}})",
            std::filesystem::path(".")
          ),
          engine_error::PROJECT_INVALID
        );
        const core::result<project> earthly = project::parse(
          R"({"schema_version": 1, "name": "x", "physics": {"gravity": [0, -9.81, 0]}})",
          std::filesystem::path(".")
        );
        CONJURE_REQUIRE_OK(earthly);
        CHECK(earthly.value().description().physics.gravity.y == -9.81f);
    }

    TEST_CASE("project - an unknown top-level key never replaces the same member of extensions") {
        log_capture logs = {};
        const core::result<project> parsed = project::parse(
          R"({"schema_version": 1, "name": "g", "extensions": {"build": "dist"}, "build": 123})",
          std::filesystem::path(".")
        );
        CONJURE_REQUIRE_OK(parsed);
        CHECK(parsed.value().description().extensions.root()["build"].as_string() == "dist");
        CHECK(logs.contains(core::log_level::WARN, "'build'"));
        const core::json::document written = parsed.value().to_json();
        CHECK(written.root()["extensions"]["build"].as_string() == "dist");
    }

    TEST_CASE("project - a root given with a trailing separator names the game module after its folder") {
        temp_directory directory("engine_project_trailing");
        const project written = engine_test::write_project(directory.path(), "trailing");
        CHECK(written.description().name == "trailing");
        CONJURE_REQUIRE_OK(directory.write_text("module.json", "{}"));

        const core::result<project> loaded = project::load(directory.path() / "");
        CONJURE_REQUIRE_OK(loaded);
        CHECK(loaded.value().root_directory().filename() == directory.path().filename());
        const std::vector<project_module_entry> entries = loaded.value().module_manifests();
        REQUIRE(entries.size() == 1);
        CHECK(entries[0].name == directory.path().filename().generic_string());
    }

    TEST_CASE("project - the cooked flag and root are read") {
        const core::result<project> parsed = project::parse(
          R"({"schema_version": 1, "name": "shipped", "cooked": true, "cooked_root": "cooked"})",
          std::filesystem::path("D:/game")
        );
        CONJURE_REQUIRE_OK(parsed);
        CHECK(parsed.value().description().cooked);
        CHECK(parsed.value().cooked_directory().generic_string() == "D:/game/cooked");
    }

    TEST_CASE("project - module_manifests lists the game module and the search path children") {
        temp_directory directory("engine_project_modules");
        const project loaded = engine_test::write_project(
          directory.path(),
          "with_modules",
          "",
          R"("modules": { "search_paths": ["modules"], "required": ["beta"] })"
        );
        CONJURE_REQUIRE_OK(directory.write_text("module.json", "{}"));
        std::filesystem::create_directories(directory.path() / "modules" / "alpha");
        std::filesystem::create_directories(directory.path() / "modules" / "beta");
        std::filesystem::create_directories(directory.path() / "modules" / "no_manifest");
        CONJURE_REQUIRE_OK(directory.write_text("modules/alpha/module.json", "{}"));
        CONJURE_REQUIRE_OK(directory.write_text("modules/beta/module.json", "{}"));

        const std::vector<project_module_entry> entries = loaded.module_manifests();
        REQUIRE(entries.size() == 3);
        CHECK(entries[0].required);
        CHECK(entries[0].manifestPath == loaded.root_directory() / "module.json");
        CHECK(entries[1].name == "alpha");
        CHECK_FALSE(entries[1].required);
        CHECK(entries[2].name == "beta");
        CHECK(entries[2].required);
    }
}

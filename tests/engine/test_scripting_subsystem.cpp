#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/temp_directory.h"
#include "engine_test_support.h"

#include "conjure/core/math.h"
#include "conjure/core/types.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/scheduler.h"
#include "conjure/engine/application.h"
#include "conjure/engine/application_desc.h"
#include "conjure/engine/engine_config.h"
#include "conjure/engine/engine_error.h"
#include "conjure/engine/engine_time.h"
#include "conjure/engine/project.h"
#include "conjure/platform/input_state.h"
#include "conjure/platform/key.h"
#include "conjure/scripting/native_api.h"
#include "conjure/scripting/script_component.h"
#include "conjure/scripting/script_host.h"
#include "conjure/scripting/script_runtime.h"
#include "engine/subsystems/scripting_subsystem.h"

#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS
    #include "conjure/physics/contact_events.h"
#endif

#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// scripting_subsystem (docs/design/06_engine_loop.md 3.4 steps 14 and 17, D12, R27, R29): the runtime over the
// null host, the frame block written every frame from engine_time and the input snapshot, the contact to
// collision conversion, the error paths of a missing runtime, and the headless opt-in: a headless run of a
// scripted project starts the runtime only with --scripting and says so when it does not.

using namespace conjure;
using namespace conjure::engine;
using namespace conjure::testing;

namespace {

    [[nodiscard]] scripting_subsystem *find_scripting_subsystem(application &app) {
        for (const std::unique_ptr<engine_subsystem> &subsystem : app.subsystems()) {
            if (subsystem->name() == "scripting") {
                return dynamic_cast<scripting_subsystem *>(subsystem.get());
            }
        }
        return nullptr;
    }

    [[nodiscard]] engine_config null_host_config() {
        engine_config config = engine_test::inline_config();
        config.scripting.enabled = true;
        config.scripting.required = true;
        config.scripting.useNullHost = true;
        config.scripting.hotReload = false;
        return config;
    }

    constexpr std::string_view HEADLESS_HINT = "unless --scripting is given";
    constexpr std::string_view PLAIN_DISABLED = "scripting: disabled by configuration";

    /// <summary>
    /// The descriptor conjure_runtime would build for the arguments, over a base that picks the null script
    /// host (the merge never touches useNullHost), so the cases run where no .NET runtime is installed.
    /// </summary>
    [[nodiscard]] application_desc null_host_desc(const std::vector<std::string_view> &arguments) {
        engine_config base = engine_config::defaults();
        base.scripting.useNullHost = true;
        core::result<application_desc> desc =
          application_desc::from_command_line(std::span<const std::string_view>(arguments), base);
        CONJURE_REQUIRE_OK(desc);
        return std::move(desc).value();
    }
}

TEST_SUITE("engine/scripting_subsystem") {

    TEST_CASE("scripting_subsystem - the null host runtime initialises and writes the frame block every frame") {
        log_capture logs = {};
        application app(engine_test::make_desc(null_host_config()));
        CONJURE_REQUIRE_OK(app.initialise());
        REQUIRE(app.context().scripting != nullptr);
        CHECK(app.context().scripting->status() == scripting::script_host_status::READY);
        CHECK(app.context().scripting->mode() == scripting::script_mode::PLAY);
        CHECK(app.context().components->find(core::string_id("conjure.scripting.script_component")).is_valid());
        scripting_subsystem *subsystem = find_scripting_subsystem(app);
        REQUIRE(subsystem != nullptr);
        CHECK(subsystem->is_initialised());

        const u32 systemsBefore = app.context().scheduler->system_count();
        CHECK(systemsBefore >= 6);
        CONJURE_REQUIRE_OK(app.step_frame());
        CONJURE_REQUIRE_OK(app.step_frame());
        CHECK(subsystem->frame_blocks_written() == 2);
        const conjure_frame_block &block = app.context().scripting->frame_block();
        CHECK(block.frame_index == 2);
        CHECK(block.delta_time == app.time().deltaTime);
        CHECK(block.fixed_delta_time == app.time().fixedDeltaTime);
        CHECK(block.time_scale == 1.0f);
        CHECK(block.in_play_mode == 1);
        app.shutdown();
        CHECK(app.context().scripting == nullptr);
    }

    TEST_CASE("scripting_subsystem - the editor starts the runtime in EDIT mode") {
        engine_config config = null_host_config();
        config.editor = true;
        application app(engine_test::make_desc(config));
        CONJURE_REQUIRE_OK(app.initialise());
        REQUIRE(app.context().scripting != nullptr);
        CHECK(app.context().scripting->mode() == scripting::script_mode::EDIT);
        app.shutdown();
    }

    TEST_CASE("scripting_subsystem - a missing game assembly is fatal when required and a warning otherwise") {
        log_capture logs = {};
        logs.allow_errors();
        engine_config config = null_host_config();
        config.scripting.gameAssembly = "Scripts/bin/Missing.dll";
        application strict(engine_test::make_desc(config));
        CONJURE_CHECK_ERROR(strict.initialise(), engine_error::SCRIPT_ASSEMBLY_LOAD_FAILED);
        CHECK(strict.state() == application_state::CREATED);

        config.scripting.required = false;
        application lenient(engine_test::make_desc(config));
        CONJURE_REQUIRE_OK(lenient.initialise());
        CHECK(lenient.context().scripting != nullptr);
        CHECK(logs.contains(core::log_level::WARN, "Missing.dll"));
        lenient.shutdown();
    }

    TEST_CASE("scripting_subsystem - headless_holds_scripts_back names only a headless run of a scripted project") {
        // A scripted project enables scripting and names the game assembly its scripts come from.
        const auto parsed = [](std::string_view scriptingMember) {
            std::string text = R"({"schema_version": 1, "name": "probe")";
            text += scriptingMember;
            text += "}";
            core::result<project> loaded = project::parse(text, std::filesystem::path("."));
            CONJURE_REQUIRE_OK(loaded);
            return std::move(loaded).value();
        };
        const project scripted = parsed(R"(, "scripting": {"enabled": true, "assembly": "Scripts/bin/Game.dll"})");
        const project scriptedByDefault = parsed(R"(, "scripting": {"assembly": "Scripts/bin/Game.dll"})");
        const project unscripted = parsed(R"(, "scripting": {"enabled": false, "assembly": "Scripts/bin/Game.dll"})");
        // Neighbours with no script to hold back: scripting enabled without an assembly, and a project.json
        // without a "scripting" section, which enables scripting by default.
        const project enabledOnly = parsed(R"(, "scripting": {"enabled": true})");
        const project bare = parsed("");
        REQUIRE(bare.description().scripting.enabled);
        REQUIRE(bare.description().scripting.gameAssembly.empty());

        const engine_config headless = engine_config::headless_defaults();
        REQUIRE_FALSE(headless.scripting.enabled);
        CHECK(scripting_subsystem::headless_holds_scripts_back(headless, &scripted));
        CHECK(scripting_subsystem::headless_holds_scripts_back(headless, &scriptedByDefault));
        CHECK_FALSE(scripting_subsystem::headless_holds_scripts_back(headless, &unscripted));
        CHECK_FALSE(scripting_subsystem::headless_holds_scripts_back(headless, &enabledOnly));
        CHECK_FALSE(scripting_subsystem::headless_holds_scripts_back(headless, &bare));
        CHECK_FALSE(scripting_subsystem::headless_holds_scripts_back(headless, nullptr));

        engine_config optedIn = headless;
        optedIn.scripting.enabled = true;
        CHECK_FALSE(scripting_subsystem::headless_holds_scripts_back(optedIn, &scripted));

        engine_config windowed = engine_config::defaults();
        windowed.scripting.enabled = false;
        CHECK_FALSE(scripting_subsystem::headless_holds_scripts_back(windowed, &scripted));

        // The editor's headless mode picks its own script host, so a disabled one there was asked for.
        engine_config editorHeadless = headless;
        editorHeadless.editor = true;
        CHECK_FALSE(scripting_subsystem::headless_holds_scripts_back(editorHeadless, &scripted));
    }

    TEST_CASE("regression - code sweep F075 a headless run of a scripted project starts the runtime with --scripting") {
        // The whole path of `conjure_runtime --project <dir> --headless [--scripting]` short of the process: the
        // merge, then the application. Without the flag in the merge no headless run ever had a script runtime.
        log_capture logs = {};
        temp_directory directory("engine_headless_scripting");
        // The scripted project names a game assembly; the null host loads any file that exists under that name.
        const project scripted = engine_test::write_project(
          directory.path() / "scripted",
          "scripted",
          {},
          R"("scripting": {"enabled": true, "assembly": "Scripts/bin/Game.dll", "hot_reload": false}, )"
          R"("modules": {"engine_plugins": false})"
        );
        CONJURE_REQUIRE_OK(directory.write_text("scripted/Scripts/bin/Game.dll", "stand-in assembly"));
        REQUIRE(scripted.description().scripting.enabled);
        const project unscripted = engine_test::write_project(
          directory.path() / "unscripted",
          "unscripted",
          {},
          R"("scripting": {"enabled": false}, "modules": {"engine_plugins": false})"
        );
        REQUIRE_FALSE(unscripted.description().scripting.enabled);
        // No "scripting" section at all: scripting is enabled by default and there is no script to run.
        const project bare =
          engine_test::write_project(directory.path() / "bare", "bare", {}, R"("modules": {"engine_plugins": false})");
        REQUIRE(bare.description().scripting.enabled);
        const std::string scriptedArgument = (directory.path() / "scripted").string();
        const std::string unscriptedArgument = (directory.path() / "unscripted").string();
        const std::string bareArgument = (directory.path() / "bare").string();

        SUBCASE("--headless --scripting constructs the runtime and writes its frame block") {
            application app(
              null_host_desc({"--project", scriptedArgument, "--headless", "--scripting", "--jobs", "0"})
            );
            CONJURE_REQUIRE_OK(app.initialise());
            REQUIRE(app.context().scripting != nullptr);
            CHECK(app.context().scripting->status() == scripting::script_host_status::READY);
            CHECK(app.context().scripting->mode() == scripting::script_mode::PLAY);
            scripting_subsystem *subsystem = find_scripting_subsystem(app);
            REQUIRE(subsystem != nullptr);
            CONJURE_REQUIRE_OK(app.step_frame());
            CHECK(subsystem->frame_blocks_written() == 1);
            app.shutdown();
            CHECK_FALSE(logs.contains(HEADLESS_HINT));
            CHECK_FALSE(logs.contains(PLAIN_DISABLED));
        }
        SUBCASE("plain --headless starts without it and the INFO line names the flag") {
            application app(null_host_desc({"--project", scriptedArgument, "--headless", "--jobs", "0"}));
            CONJURE_REQUIRE_OK(app.initialise());
            CHECK(app.context().scripting == nullptr);
            CONJURE_REQUIRE_OK(app.step_frame());
            app.shutdown();
            CHECK(logs.contains(core::log_level::INFO, HEADLESS_HINT));
            CHECK_FALSE(logs.contains(PLAIN_DISABLED));
        }
        SUBCASE("a project that disables scripting gets the plain line, and --scripting still wins over it") {
            application off(null_host_desc({"--project", unscriptedArgument, "--headless", "--jobs", "0"}));
            CONJURE_REQUIRE_OK(off.initialise());
            CHECK(off.context().scripting == nullptr);
            off.shutdown();
            CHECK(logs.contains(core::log_level::INFO, PLAIN_DISABLED));
            CHECK_FALSE(logs.contains(HEADLESS_HINT));

            application on(
              null_host_desc({"--project", unscriptedArgument, "--headless", "--scripting", "--jobs", "0"})
            );
            CONJURE_REQUIRE_OK(on.initialise());
            CHECK(on.context().scripting != nullptr);
            on.shutdown();
        }
        SUBCASE("a project without a scripting section has no script to hold back and gets the plain line") {
            application app(null_host_desc({"--project", bareArgument, "--headless", "--jobs", "0"}));
            CONJURE_REQUIRE_OK(app.initialise());
            CHECK(app.context().scripting == nullptr);
            app.shutdown();
            CHECK(logs.contains(core::log_level::INFO, PLAIN_DISABLED));
            CHECK_FALSE(logs.contains(HEADLESS_HINT));
        }
    }

    TEST_CASE("scripting_subsystem - write_frame_block copies time and input") {
        conjure_frame_block block = {};
        engine_time time = {};
        time.time = 12.5;
        time.unscaledTime = 13.0;
        time.deltaTime = 0.02f;
        time.fixedDeltaTime = 0.01f;
        time.timeScale = 0.5f;
        time.frameIndex = 77;
        time.fixedStepIndex = 154;
        platform::input_state input = {};
        input.keysDown.words[1] = 0x5;
        input.keysPressed.words[0] = 0x8;
        input.mousePosition = math::vec2(3.0f, 4.0f);
        input.mouseDelta = math::vec2(-1.0f, 2.0f);
        input.scrollDelta = math::vec2(0.0f, 1.0f);
        input.mouseButtonsDown = 1;
        input.mouseButtonsPressed = 1;
        input.gamepads[2].connected = true;
        input.gamepads[2].buttonsDown = 0x10;
        input.gamepads[2].axes[3] = 0.75f;

        scripting_subsystem::write_frame_block(block, time, input, nullptr);
        CHECK(block.time_seconds == 12.5);
        CHECK(block.unscaled_time_seconds == 13.0);
        CHECK(block.delta_time == 0.02f);
        CHECK(block.fixed_delta_time == 0.01f);
        CHECK(block.time_scale == 0.5f);
        CHECK(block.frame_index == 77);
        CHECK(block.fixed_step_index == 154);
        CHECK(block.input.keys_down[1] == 0x5);
        CHECK(block.input.keys_pressed[0] == 0x8);
        CHECK(block.input.keys_released[0] == 0);
        CHECK(block.input.mouse_x == 3.0f);
        CHECK(block.input.mouse_y == 4.0f);
        CHECK(block.input.mouse_dx == -1.0f);
        CHECK(block.input.mouse_dy == 2.0f);
        CHECK(block.input.scroll_y == 1.0f);
        CHECK(block.input.mouse_buttons_down == 1);
        CHECK(block.input.gamepads[2].connected == 1);
        CHECK(block.input.gamepads[2].buttons_down == 0x10);
        CHECK(block.input.gamepads[2].axes[3] == 0.75f);
        CHECK(block.input.gamepads[0].connected == 0);
    }

#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS
    TEST_CASE("scripting_subsystem - convert_contacts maps the four kinds and drops STAY") {
        std::vector<physics::contact_event> contacts(5);
        contacts[0].kind = physics::contact_kind::ENTER;
        contacts[0].a.index = 1;
        contacts[0].b.index = 2;
        contacts[0].impulse = 3.0f;
        contacts[0].point = math::vec3(1.0f, 2.0f, 3.0f);
        contacts[1].kind = physics::contact_kind::STAY;
        contacts[2].kind = physics::contact_kind::EXIT;
        contacts[3].kind = physics::contact_kind::TRIGGER_ENTER;
        contacts[4].kind = physics::contact_kind::TRIGGER_EXIT;
        std::vector<scripting::collision_event> events = {};
        scripting_subsystem::convert_contacts(contacts, events);
        REQUIRE(events.size() == 4);
        CHECK(events[0].kind == scripting::collision_kind::COLLISION_ENTER);
        CHECK(events[0].a.index == 1);
        CHECK(events[0].b.index == 2);
        CHECK(events[0].impulse == 3.0f);
        CHECK(events[0].point == math::vec3(1.0f, 2.0f, 3.0f));
        CHECK(events[1].kind == scripting::collision_kind::COLLISION_EXIT);
        CHECK(events[2].kind == scripting::collision_kind::TRIGGER_ENTER);
        CHECK(events[3].kind == scripting::collision_kind::TRIGGER_EXIT);
    }
#endif
}

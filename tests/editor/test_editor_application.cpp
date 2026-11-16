#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/temp_directory.h"

#include "editor_test_support.h"

#include "conjure/core/json/document.h"
#include "conjure/editor/editor_application.h"
#include "conjure/editor/editor_error.h"
#include "conjure/editor/editor_options.h"
#include "conjure/editor/play_controller.h"
#include "conjure/editor/protocol/protocol_message.h"
#include "conjure/engine/application_desc.h"
#include "conjure/engine/engine_config.h"
#include "conjure/engine/engine_error.h"
#include "conjure/engine/exit_code.h"
#include "conjure/engine/project.h"

#if defined(CONJURE_EDITOR_TEST_HAS_BINDINGS) && CONJURE_EDITOR_TEST_HAS_BINDINGS
    #include "conjure/testing/test_environment.h"

    #include "conjure/bindings/bindings_subsystem.h"
    #include "conjure/bindings/call_counters.h"
    #include "conjure/ecs/entity.h"
    #include "conjure/editor/entity_ref.h"
    #include "conjure/engine/engine_subsystem.h"
    #include "conjure/scripting/native_api.h"
    #include "conjure/scripting/script_runtime.h"
#endif

#include <filesystem>
#include <format>
#include <fstream>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

// editor_application.h (docs/design/04_editor.md 3.1): the process object behind conjure_editor.

using namespace conjure;
using namespace conjure::editor;

namespace {
    /// <summary>
    /// Writes a minimal project.json and its assets folder; 'extraMembers' are further members of the root object
    /// (R"("startup_scene": "scenes/main.scene.json")").
    /// </summary>
    void write_project(
      const std::filesystem::path &directory,
      std::string_view name,
      std::string_view extraMembers = {}
    ) {
        std::error_code ignored = {};
        std::filesystem::create_directories(directory / "assets", ignored);
        std::ofstream stream(directory / "project.json", std::ios::binary | std::ios::trunc);
        REQUIRE(stream.is_open());
        stream << "{\n  \"schema_version\": 1,\n  \"name\": \"" << name << "\",\n  \"assets_root\": \"assets\"";
        if (!extraMembers.empty()) {
            stream << ",\n  " << extraMembers;
        }
        stream << "\n}\n";
    }

    /// <summary>
    /// The options of a headless `--project <project> --exec <commands>` run without a protocol server.
    /// </summary>
    [[nodiscard]] editor_options exec_options(
      const std::filesystem::path &project,
      const std::filesystem::path &commands
    ) {
        editor_options options = {};
        options.headless = true;
        options.aiPort = u16{0};
        options.projectPath = project;
        options.execFile = commands;
        options.engineArguments = {"--log-level", "warn"};
        return options;
    }

#if defined(CONJURE_EDITOR_TEST_HAS_BINDINGS) && CONJURE_EDITOR_TEST_HAS_BINDINGS
    /// <summary>
    /// The bindings subsystem of the editor's engine, or nullptr when none was added.
    /// </summary>
    [[nodiscard]] const bindings::scripting_bindings_subsystem *find_bindings(engine::application &engine) {
        for (const std::unique_ptr<engine::engine_subsystem> &subsystem : engine.subsystems()) {
            if (subsystem->name() == "bindings") {
                return dynamic_cast<const bindings::scripting_bindings_subsystem *>(subsystem.get());
            }
        }
        return nullptr;
    }

    /// <summary>
    /// The names of the bindings-owned native API slots still null in 'api' (empty = complete).
    /// </summary>
    [[nodiscard]] std::string null_bindings_slots(const conjure_native_api &api) {
        const std::span<void *const> slots = scripting::native_api_slots(api);
        REQUIRE(slots.size() == bindings::NATIVE_FUNCTION_COUNT);
        std::string missing = {};
        for (u32 index = scripting::NATIVE_API_CORE_ENTRY_COUNT; index < bindings::NATIVE_FUNCTION_COUNT; ++index) {
            if (slots[index] == nullptr) {
                if (!missing.empty()) {
                    missing += ", ";
                }
                missing += bindings::function_name(static_cast<bindings::native_function_id>(index));
            }
        }
        return missing;
    }
#endif
}

TEST_SUITE("editor/editor_application") {

    TEST_CASE("editor_application::make_engine_desc - headless options select null backends and the null script host") {
        editor_options options = {};
        options.headless = true;
        options.engineArguments = {"--frames", "3"};
        const core::result<engine::application_desc> desc = editor_application::make_engine_desc(options, {});
        CONJURE_REQUIRE_OK(desc);
        CHECK(desc.value().config.headless);
        CHECK(desc.value().config.editor);
        CHECK(desc.value().config.scripting.enabled);
        CHECK(desc.value().config.scripting.useNullHost);
        CHECK_FALSE(desc.value().config.scripting.required);
        CHECK_FALSE(desc.value().config.rendering.enabled);
        CHECK(desc.value().config.maxFrames == 3);
        options.noScripting = true;
        const core::result<engine::application_desc> noScripting = editor_application::make_engine_desc(options, {});
        CONJURE_REQUIRE_OK(noScripting);
        CHECK_FALSE(noScripting.value().config.scripting.enabled);
        options.engineArguments = {"--frames", "many"};
        CHECK_FALSE(editor_application::make_engine_desc(options, {}).has_value());
    }

    TEST_CASE("regression - F079 editor_application an unrecognised engine argument fails with BAD_ARGUMENTS") {
        testing::log_capture logs = {};
        editor_options options = {};
        options.headless = true;
        options.aiPort = u16{0};
        options.engineArguments = {"--log-level", "warn", "--bogus-flag", "value"};
        const core::result<engine::application_desc> desc = editor_application::make_engine_desc(options, {});
        CONJURE_CHECK_ERROR(desc, engine::engine_error::BAD_ARGUMENTS);
        REQUIRE_FALSE(desc.has_value());
        CHECK(desc.get_error().message().find("--bogus-flag value") != std::string::npos);
        // Refused before the merge loads the project: a missing project does not hide the bad option.
        testing::temp_directory directory = {};
        CONJURE_CHECK_ERROR(
          editor_application::make_engine_desc(options, directory.path() / "nowhere"),
          engine::engine_error::BAD_ARGUMENTS
        );
        editor_application application = {};
        const core::result<void> initialised = application.initialise(options, {});
        CONJURE_CHECK_ERROR(initialised, engine::engine_error::BAD_ARGUMENTS);
        CHECK_FALSE(application.is_initialised());
    }

    TEST_CASE("editor_application::make_engine_desc - --offscreen enables rendering") {
        editor_options options = {};
        options.headless = true;
        options.offscreen = true;
        const core::result<engine::application_desc> desc = editor_application::make_engine_desc(options, {});
        CONJURE_REQUIRE_OK(desc);
        CHECK(desc.value().config.rendering.enabled);
        CHECK(desc.value().config.headless);
    }

    TEST_CASE("editor_application - initialise, run frames, quit through the host and shut down") {
        testing::log_capture logs = {};
        testing::temp_directory directory = {};
        write_project(directory.path(), "app_test");
        editor_options options = {};
        options.headless = true;
        options.aiPort = u16{0};
        options.projectPath = directory.path();
        options.engineArguments = {"--log-level", "warn"};
        editor_application application = {};
        CHECK_FALSE(application.is_initialised());
        CONJURE_REQUIRE_OK(application.initialise(options, {}));
        CHECK(application.is_initialised());
        CHECK(application.options().headless);
        CHECK(application.context().has_project());
        CHECK(application.server().is_running());
        CHECK(application.server().bound_port() == 0);
        CONJURE_REQUIRE_OK(application.pump_frame());
        CONJURE_REQUIRE_OK(application.pump_frame());
        CHECK(application.ticks() == 2);
        CHECK(application.context().frame_index() >= 2);
        CHECK_FALSE(application.quit_requested());
        application.request_quit(5);
        CHECK(application.quit_requested());
        CHECK(application.exit_code() == 5);
        CHECK(application.run() == 5);
        application.shutdown();
        CHECK_FALSE(application.is_initialised());
        application.shutdown();
        CONJURE_CHECK_ERROR(application.pump_frame(), protocol_error_code::INTERNAL);
    }

    TEST_CASE("editor_application - initialising twice is INVALID_REQUEST and a missing project fails") {
        testing::log_capture logs = {};
        logs.allow_errors();
        testing::temp_directory directory = {};
        editor_options missing = {};
        missing.headless = true;
        missing.aiPort = u16{0};
        missing.projectPath = directory.path() / "nowhere";
        editor_application failed = {};
        CHECK_FALSE(failed.initialise(missing, {}).has_value());
        CHECK_FALSE(failed.is_initialised());

        write_project(directory.path(), "twice");
        editor_options options = {};
        options.headless = true;
        options.aiPort = u16{0};
        options.projectPath = directory.path();
        options.engineArguments = {"--log-level", "warn"};
        editor_application application = {};
        CONJURE_REQUIRE_OK(application.initialise(options, {}));
        CONJURE_CHECK_ERROR(application.initialise(options, {}), protocol_error_code::INVALID_REQUEST);
        application.shutdown();
    }

    TEST_CASE("editor_application - editor.quit requests the exit code through the host") {
        editor_test::headless_editor editor = {};
        const core::json::document result = editor.ok("editor.quit", core::json::object({{"exitCode", 3}}));
        CHECK(result.root()["exitCode"].as_i64() == 3);
        CHECK(editor.application().quit_requested());
        CHECK(editor.application().exit_code() == 3);
        CHECK(editor.application().run() == 3);
    }

    TEST_CASE("editor_application - editor.quit with saveChanges writes the dirty scene first") {
        editor_test::headless_editor editor = {};
        (void) editor.create_entity("saved on quit");
        CHECK(editor.context().scene_dirty());
        (void) editor.ok("editor.quit", core::json::object({{"saveChanges", true}}));
        CHECK(std::filesystem::is_regular_file(editor.project_directory() / "assets" / "scenes" / "test.scene.json"));
        CHECK_FALSE(editor.context().scene_dirty());
    }

    TEST_CASE("regression - code sweep F089 editor.quit with saveChanges while playing saves the edited scene") {
        editor_test::headless_editor editor = {};
        (void) editor.create_entity("EditedBeforePlay");
        REQUIRE(editor.context().scene_dirty());
        (void) editor.ok("play.start");
        (void) editor.create_entity("OnlyWhilePlaying");
        // scene.save is refused while playing, so the quit leaves play first: what it writes is the scene of
        // before the session, and the request does not fail on the refused save.
        (void) editor.ok("editor.quit", core::json::object({{"saveChanges", true}}));
        CHECK(editor.application().quit_requested());
        CHECK(editor.context().play().is_editing());
        const std::string saved = editor_test::read_text_file(
          editor.project_directory() / "assets" / "scenes" / "test.scene.json"
        );
        CHECK(saved.find("EditedBeforePlay") != std::string::npos);
        CHECK(saved.find("OnlyWhilePlaying") == std::string::npos);
        CHECK_FALSE(editor.context().scene_dirty());
    }

    TEST_CASE("regression - code sweep F089 editor.quit whose save fails after the play session was stopped answers "
      "the save's error and does not quit") {
        editor_test::headless_editor editor = {};
        (void) editor.create_entity("EditedBeforePlay");
        REQUIRE(editor.context().scene_dirty());
        (void) editor.ok("play.start");
        (void) editor.create_entity("OnlyWhilePlaying");
        // A folder where the scene file belongs: the save cannot write it.
        std::error_code ignored = {};
        REQUIRE(std::filesystem::create_directories(editor_test::test_scene_file(editor), ignored));
        editor.log().allow_errors();
        const protocol_response refused = editor.send("editor.quit", core::json::object({{"saveChanges", true}}));
        REQUIRE_FALSE(refused.ok);
        CHECK(refused.errorCode == protocol_error_code::IO_ERROR);
        CHECK_FALSE(editor.application().quit_requested());
        // The stop is not taken back: the editor is left in the editing state over the scene of before the
        // session, still unsaved.
        CHECK(editor.context().play().is_editing());
        CHECK(editor.context().scene_dirty());
        (void) editor.ok("entity.get", core::json::object({{"entity", "EditedBeforePlay"}}));
        CHECK(editor.fail(
          "entity.get",
          core::json::object({{"entity", "OnlyWhilePlaying"}})
        ) == protocol_error_code::ENTITY_NOT_FOUND);
        // Once the file can be written again the same request saves and quits.
        REQUIRE(std::filesystem::remove(editor_test::test_scene_file(editor), ignored));
        (void) editor.ok("editor.quit", core::json::object({{"saveChanges", true}}));
        CHECK(editor.application().quit_requested());
        CHECK(std::filesystem::is_regular_file(editor_test::test_scene_file(editor)));
    }

    TEST_CASE("editor_application - request_open_project validates the directory before scheduling the restart") {
        editor_test::headless_editor editor = {};
        CHECK_FALSE(editor.application().request_open_project(editor.project_directory() / "missing").has_value());
        CHECK_FALSE(editor.application().has_pending_project_open());
        const std::filesystem::path other = editor.project_directory() / "other";
        write_project(other, "other");
        CONJURE_REQUIRE_OK(editor.application().request_open_project(other));
        CHECK(editor.application().has_pending_project_open());
        editor.pump(1);
        CHECK_FALSE(editor.application().has_pending_project_open());
        CHECK(editor.context().project()->description().name == "other");
        CHECK(editor.application().options().projectPath == other);
        CHECK(editor.application().context().has_project());
    }

    TEST_CASE("editor_application - --exec runs the file and returns the runner's exit code") {
        testing::log_capture logs = {};
        logs.allow_errors();
        testing::temp_directory directory = {};
        write_project(directory.path(), "exec_test");
        const std::filesystem::path commands = directory.file("commands.json");
        {
            std::ofstream stream(commands, std::ios::binary | std::ios::trunc);
            stream << R"([{"id":1,"cmd":"scene.new","args":{"name":"s"}},{"id":2,"cmd":"entity.create",)"
            R"("args":{"name":"a"}}])";
        }
        editor_options options = {};
        options.headless = true;
        options.aiPort = u16{0};
        options.projectPath = directory.path();
        options.execFile = commands;
        options.outputFile = directory.file("out.ndjson");
        options.engineArguments = {"--log-level", "warn"};
        editor_application application = {};
        CONJURE_REQUIRE_OK(application.initialise(options, {}));
        CHECK(application.run() == 0);
        application.shutdown();
        const std::string output = editor_test::read_text_file(options.outputFile);
        CHECK(output.find("\"id\":2,\"ok\":true") != std::string::npos);
        // The project has no startup scene, so nothing is reported about one.
        CHECK(output.find("STARTUP_SCENE_FAILED") == std::string::npos);
    }

    TEST_CASE("editor_application - an --out that cannot be written ends an --exec run with exit code 2") {
        testing::log_capture logs = {};
        testing::temp_directory directory = {};
        write_project(directory.path(), "exec_out");
        const std::filesystem::path commands = directory.write_file(
          "commands.json",
          R"([{"cmd":"scene.new","args":{"name":"s","path":"scenes/never"}},{"cmd":"scene.save"}])"
        );
        editor_options options = exec_options(directory.path(), commands);
        // A directory where the output file should be.
        options.outputFile = directory.path() / "assets";
        editor_application application = {};
        CONJURE_REQUIRE_OK(application.initialise(options, {}));
        CHECK(application.run() == static_cast<int>(engine::exit_code::RUNTIME_FAILURE));
        application.shutdown();
        // No request ran.
        CHECK_FALSE(std::filesystem::exists(directory.path() / "assets" / "scenes" / "never.scene.json"));
    }

    TEST_CASE(
      "regression - code sweep F659 editor_application refuses an unusable --exec file before the engine starts"
    ) {
        testing::log_capture logs = {};
        testing::temp_directory directory = {};
        write_project(directory.path(), "exec_fail_fast");
        // The asset cache under .conjure/ is the trace an engine start leaves in a project.
        const std::filesystem::path engineTrace = directory.path() / ".conjure";

        SUBCASE("a file that does not exist") {
            const std::filesystem::path missing = directory.file("missing.json");
            editor_application application = {};
            const core::result<void> initialised = application.initialise(exec_options(directory.path(), missing), {});
            // BAD_ARGUMENTS is what conjure_editor's main turns into exit code 3.
            CONJURE_CHECK_ERROR(initialised, engine::engine_error::BAD_ARGUMENTS);
            REQUIRE_FALSE(initialised.has_value());
            CHECK(initialised.get_error().message().find("--exec: cannot read") != std::string::npos);
            CHECK(initialised.get_error().message().find("missing.json") != std::string::npos);
            CHECK_FALSE(application.is_initialised());
            CHECK_FALSE(std::filesystem::exists(engineTrace));
            CHECK(application.run() == static_cast<int>(engine::exit_code::INIT_FAILURE));
        }
        SUBCASE("a file that does not parse") {
            const std::filesystem::path malformed = directory.write_file(
              "malformed.json",
              "{\"cmd\":\"editor.state\"}\n{\"cmd\":\n"
            );
            editor_application application = {};
            const core::result<void> initialised = application.initialise(
              exec_options(directory.path(), malformed),
              {}
            );
            CONJURE_CHECK_ERROR(initialised, engine::engine_error::BAD_ARGUMENTS);
            REQUIRE_FALSE(initialised.has_value());
            CHECK(initialised.get_error().message().find("malformed.json") != std::string::npos);
            CHECK(initialised.get_error().message().find("line 2") != std::string::npos);
            CHECK_FALSE(application.is_initialised());
            CHECK_FALSE(std::filesystem::exists(engineTrace));
        }
        SUBCASE("a directory") {
            editor_application application = {};
            const core::result<void> initialised = application.initialise(
              exec_options(directory.path(), directory.path() / "assets"),
              {}
            );
            CONJURE_CHECK_ERROR(initialised, engine::engine_error::BAD_ARGUMENTS);
            CHECK_FALSE(std::filesystem::exists(engineTrace));
        }
        SUBCASE("a usable file starts the engine, which leaves its trace") {
            const std::filesystem::path commands = directory.write_file(
              "commands.json",
              R"([{"cmd":"editor.state"}])"
            );
            editor_options options = exec_options(directory.path(), commands);
            options.outputFile = directory.file("out.ndjson");
            editor_application application = {};
            CONJURE_REQUIRE_OK(application.initialise(options, {}));
            CHECK(std::filesystem::is_directory(engineTrace));
            // The file was read by initialise(): removing it now changes nothing for the run.
            std::error_code ignored = {};
            REQUIRE(std::filesystem::remove(commands, ignored));
            CHECK(application.run() == 0);
            application.shutdown();
            CHECK(editor_test::read_text_file(options.outputFile).find("\"ok\":true") != std::string::npos);
        }
    }

    TEST_CASE("editor_application - a bad flag or a missing project is reported before an unusable --exec file") {
        testing::log_capture logs = {};
        logs.allow_errors();
        testing::temp_directory directory = {};
        const std::filesystem::path missing = directory.file("missing.json");

        // An unknown option and an unusable command file: the option is what is reported.
        write_project(directory.path() / "good", "good");
        editor_options badFlag = exec_options(directory.path() / "good", missing);
        badFlag.engineArguments.emplace_back("--bogus-flag");
        editor_application first = {};
        const core::result<void> flagged = first.initialise(badFlag, {});
        CONJURE_CHECK_ERROR(flagged, engine::engine_error::BAD_ARGUMENTS);
        REQUIRE_FALSE(flagged.has_value());
        CHECK(flagged.get_error().message().find("--bogus-flag") != std::string::npos);
        CHECK(flagged.get_error().message().find("--exec") == std::string::npos);

        // A missing project and an unusable command file: a project failure (exit 2), not a bad argument.
        editor_application second = {};
        const core::result<void> noProject = second.initialise(exec_options(directory.path() / "nowhere", missing), {});
        CONJURE_CHECK_ERROR(noProject, engine::engine_error::PROJECT_NOT_FOUND);
    }

    TEST_CASE("editor_application - --export wins over --exec, whose file is then not read") {
        testing::log_capture logs = {};
        testing::temp_directory directory = {};
        write_project(directory.path() / "project", "export_wins");
        editor_options options = exec_options(directory.path() / "project", directory.file("missing.json"));
        options.exportDirectory = directory.path() / "exported";
        editor_application application = {};
        // run() would export and never look at the command file, so the missing file is not an error here.
        CONJURE_REQUIRE_OK(application.initialise(options, {}));
        application.shutdown();
    }

    TEST_CASE(
      "regression - code sweep F659 editor_application an --exec run over a missing startup scene warns first, "
      "repairs it and exits 0"
    ) {
        testing::log_capture logs = {};
        testing::temp_directory directory = {};
        write_project(directory.path(), "repair", R"("startup_scene": "scenes/main.scene.json")");
        const std::filesystem::path repair = directory.write_file(
          "repair.json",
          R"([{"id":"new","cmd":"scene.new","args":{"name":"main","path":"scenes/main"}},)"
          R"({"id":"entity","cmd":"entity.create","args":{"name":"Repaired"}},{"id":"save","cmd":"scene.save"}])"
        );
        editor_options options = exec_options(directory.path(), repair);
        options.outputFile = directory.file("repair.ndjson");
        {
            editor_application application = {};
            CONJURE_REQUIRE_OK(application.initialise(options, {}));
            REQUIRE(application.startup_scene_warning().has_value());
            // The exit code is the commands' alone: the failed startup scene does not change it.
            CHECK(application.run() == 0);
            application.shutdown();
        }
        const std::vector<std::string> lines = editor_test::non_empty_lines(
          editor_test::read_text_file(options.outputFile)
        );
        REQUIRE(lines.size() >= 4);
        const core::result<protocol_event> warning = parse_event(lines.front());
        CONJURE_REQUIRE_OK(warning);
        CHECK(warning.value().name == "warning");
        CHECK(warning.value().data.root()["code"].as_string() == "STARTUP_SCENE_FAILED");
        CHECK(warning.value().data.root()["message"].as_string().find("scenes/main.scene.json") != std::string::npos);
        usize responses = 0;
        for (const std::string &line : lines) {
            const core::result<protocol_response> response = parse_response(line);
            if (response.has_value()) {
                ++responses;
                CHECK_MESSAGE(response.value().ok, line);
            }
        }
        CHECK(responses == 3);
        REQUIRE(std::filesystem::is_regular_file(directory.path() / "assets" / "scenes" / "main.scene.json"));

        // The repaired project starts with its scene: the next run reports nothing.
        const std::filesystem::path check = directory.write_file(
          "check.json",
          R"([{"id":"find","cmd":"entity.find","args":{"name":"Repaired"}}])"
        );
        editor_options again = exec_options(directory.path(), check);
        again.outputFile = directory.file("check.ndjson");
        editor_application application = {};
        CONJURE_REQUIRE_OK(application.initialise(again, {}));
        CHECK_FALSE(application.startup_scene_warning().has_value());
        CHECK(application.run() == 0);
        application.shutdown();
        const std::string output = editor_test::read_text_file(again.outputFile);
        CHECK(output.find("STARTUP_SCENE_FAILED") == std::string::npos);
        CHECK(output.find("\"ok\":true") != std::string::npos);
    }

    TEST_CASE("regression - code sweep F659 editor_application an --exec run over a corrupt startup scene still runs") {
        testing::log_capture logs = {};
        // The importer reports the corrupt file as an error.
        logs.allow_errors();
        testing::temp_directory directory = {};
        write_project(directory.path(), "corrupt", R"("startup_scene": "scenes/main.scene.json")");
        (void) directory.write_file("assets/scenes/main.scene.json", "{\"version\": 1, \"entities\": [");
        // No scene is open over the empty world, so the file opens one before it edits.
        const std::filesystem::path commands = directory.write_file(
          "commands.json",
          R"([{"cmd":"editor.state"},{"cmd":"scene.new","args":{"name":"fresh"}},)"
          R"({"cmd":"entity.create","args":{"name":"a"}}])"
        );
        editor_options options = exec_options(directory.path(), commands);
        options.outputFile = directory.file("out.ndjson");
        editor_application application = {};
        CONJURE_REQUIRE_OK(application.initialise(options, {}));
        CHECK(application.run() == 0);
        application.shutdown();
        const std::vector<std::string> lines = editor_test::non_empty_lines(
          editor_test::read_text_file(options.outputFile)
        );
        REQUIRE_FALSE(lines.empty());
        const core::result<protocol_event> warning = parse_event(lines.front());
        CONJURE_REQUIRE_OK(warning);
        CHECK(warning.value().name == "warning");
        CHECK(warning.value().data.root()["code"].as_string() == "STARTUP_SCENE_FAILED");
        CHECK(warning.value().data.root()["message"].as_string().find("scenes/main.scene.json") != std::string::npos);
        usize warnings = 0;
        for (const std::string &line : lines) {
            if (line.find("STARTUP_SCENE_FAILED") != std::string::npos) {
                ++warnings;
            }
        }
        CHECK(warnings == 1);
    }

    TEST_CASE("editor_application - startup_scene_warning is empty without an engine and for a scene-less project") {
        editor_application idle = {};
        CHECK_FALSE(idle.startup_scene_warning().has_value());
        editor_test::headless_editor editor = {};
        CHECK_FALSE(editor.application().startup_scene_warning().has_value());
    }

    TEST_CASE(
      "regression - code sweep F659 editor_application a project.open restart reports the new project's failed "
      "startup scene to its clients"
    ) {
        editor_test::headless_editor editor = {};
        const std::filesystem::path broken = editor.project_directory() / "broken";
        write_project(broken, "broken", R"("startup_scene": "scenes/gone.scene.json")");
        const std::filesystem::path sound = editor.project_directory() / "sound";
        write_project(sound, "sound");

        editor.clear_events();
        CONJURE_REQUIRE_OK(editor.application().request_open_project(broken));
        editor.pump(1);
        REQUIRE(editor.context().project()->description().name == "broken");
        const std::optional<core::json::document> kept = editor.application().startup_scene_warning();
        REQUIRE(kept.has_value());
        CHECK(kept->root()["code"].as_string() == "STARTUP_SCENE_FAILED");
        usize warnings = 0;
        bool loadedFirst = false;
        for (const protocol_event &event : editor.events()) {
            if (event.name == "scene.loaded" && warnings == 0) {
                loadedFirst = true;
            }
            CAPTURE(event.name);
            if (event.name == "warning" && event.data.root()["code"].as_string() == "STARTUP_SCENE_FAILED") {
                ++warnings;
                CHECK(event.data.root()["message"].as_string().find("scenes/gone.scene.json") != std::string::npos);
            }
        }
        CHECK(warnings == 1);
        CHECK(loadedFirst);

        // A restart into a project that starts cleanly says nothing, and the kept failure is gone with the engine.
        editor.clear_events();
        CONJURE_REQUIRE_OK(editor.application().request_open_project(sound));
        editor.pump(1);
        REQUIRE(editor.context().project()->description().name == "sound");
        CHECK_FALSE(editor.application().startup_scene_warning().has_value());
        CHECK_FALSE(editor.saw_event("warning"));
    }

    TEST_CASE("editor_application - run without initialise reports INIT_FAILURE") {
        editor_application application = {};
        CHECK(application.run() == static_cast<int>(engine::exit_code::INIT_FAILURE));
    }

#if defined(CONJURE_EDITOR_TEST_HAS_BINDINGS) && CONJURE_EDITOR_TEST_HAS_BINDINGS
    TEST_CASE("regression - review4: start_engine adds the bindings subsystem and fills every bindings-owned slot") {
        // 04_editor.md 3.1 and 06_engine_loop.md step 16: the editor adds bindings::scripting_bindings_subsystem
        // before initialise(). Without it the script runtime starts with every non-core slot null (the handshake
        // validates the core entries only), so Transform, hierarchy, scene, prefab, physics, audio, asset,
        // diagnostics and module facades throw NotSupportedException in the desktop editor's play mode.
        editor_test::headless_editor editor = {};
        scripting::script_runtime *runtime = editor.context().scripting();
        REQUIRE(runtime != nullptr);
        const bindings::scripting_bindings_subsystem *subsystem = find_bindings(editor.application().engine());
        REQUIRE(subsystem != nullptr);
        CHECK(subsystem->is_filled());
        const std::string missing = null_bindings_slots(runtime->native_api());
        CHECK_MESSAGE(missing.empty(), std::format("null bindings slots: {}", missing));

        // project.open restarts the engine: the new application gets the subsystem as well.
        const std::filesystem::path other = editor.project_directory() / "other";
        write_project(other, "other");
        CONJURE_REQUIRE_OK(editor.application().request_open_project(other));
        editor.pump(1);
        REQUIRE(editor.context().scripting() != nullptr);
        REQUIRE(find_bindings(editor.application().engine()) != nullptr);
        CHECK(null_bindings_slots(editor.context().scripting()->native_api()).empty());
    }

    TEST_CASE("editor_application - --no-scripting starts without a script runtime and without a table to fill") {
        editor_test::headless_editor_options options = {};
        options.scripting = false;
        editor_test::headless_editor editor(options);
        CHECK(editor.context().scripting() == nullptr);
        const bindings::scripting_bindings_subsystem *subsystem = find_bindings(editor.application().engine());
        REQUIRE(subsystem != nullptr);
        CHECK_FALSE(subsystem->is_filled());
    }

    TEST_CASE(
      "regression - review4: a C# facade probe touches Transform.Position in headless play mode [dotnet]"
      * doctest::timeout(120)
    ) {
        CONJURE_TEST_REQUIRE(testing::precondition::DOTNET_RUNTIME);
        // The real CLR inside the editor application (--dotnet) with Conjure.TestScripts as the game assembly:
        // BindingFacadeProbe writes Transform.Position and reads one entry of every other facade group in its
        // first OnUpdate, recording any exception in its Errors field instead of letting it escape.
        editor_test::headless_editor_options options = {};
        options.dotnetHost = true;
        options.extraProjectJson = std::format(
          R"("scripting": {{"enabled": true, "assembly": "{}"}})",
          editor_test::test_scripts_assembly().generic_string()
        );
        editor_test::headless_editor editor(options);
        scripting::script_runtime *runtime = editor.context().scripting();
        REQUIRE(runtime != nullptr);
        REQUIRE(runtime->is_assembly_loaded());
        CHECK(null_bindings_slots(runtime->native_api()).empty());

        const std::string guid = editor.create_entity("probe");
        (void) editor.ok(
          "script.attach",
          core::json::object({{"entity", guid}, {"className", "Conjure.TestScripts.BindingFacadeProbe"}})
        );
        (void) editor.ok("play.start");
        editor.pump(3);

        const core::json::document reference = core::json::object({{"entity", guid}});
        const core::result<ecs::entity> entity = entity_ref::resolve(reference.root()["entity"], editor.scene());
        CONJURE_REQUIRE_OK(entity);
        const std::vector<scripting::script_entry_id> scripts = runtime->scripts_of(entity.value());
        REQUIRE(scripts.size() == 1);
        const core::result<core::json::document> fields = runtime->collect_entry_fields(scripts.front());
        CONJURE_REQUIRE_OK(fields);
        CHECK(fields.value().root()["Ran"].as_i64(-1) == 1);
        CHECK_MESSAGE(
          fields.value().root()["Errors"].as_string().empty(),
          std::string(fields.value().root()["Errors"].as_string())
        );
        CHECK(fields.value().root()["Errors"].as_string().find("NotSupportedException") == std::string_view::npos);
        CHECK(fields.value().root()["PositionY"].as_i64(-1) == 3);
        (void) editor.ok("play.stop");
    }
#endif
}

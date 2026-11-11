#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/temp_directory.h"

#include "editor_test_support.h"

#include "conjure/core/json/document.h"
#include "conjure/editor/editor_application.h"
#include "conjure/editor/editor_error.h"
#include "conjure/editor/editor_options.h"
#include "conjure/engine/application_desc.h"
#include "conjure/engine/engine_config.h"
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
#include <span>
#include <string>
#include <vector>

// editor_application.h (docs/design/04_editor.md 3.1): the process object behind conjure_editor.

using namespace conjure;
using namespace conjure::editor;

namespace {
    void write_project(const std::filesystem::path &directory, std::string_view name) {
        std::error_code ignored = {};
        std::filesystem::create_directories(directory / "assets", ignored);
        std::ofstream stream(directory / "project.json", std::ios::binary | std::ios::trunc);
        REQUIRE(stream.is_open());
        stream << "{\n  \"schema_version\": 1,\n  \"name\": \"" << name << "\",\n  \"assets_root\": \"assets\"\n}\n";
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

        editor_options bad = options;
        bad.execFile = directory.file("missing.json");
        editor_application failing = {};
        CONJURE_REQUIRE_OK(failing.initialise(bad, {}));
        CHECK(failing.run() == static_cast<int>(engine::exit_code::BAD_ARGUMENTS));
        failing.shutdown();
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

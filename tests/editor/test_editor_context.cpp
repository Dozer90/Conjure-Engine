#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"

#include "editor_test_support.h"

#include "conjure/core/json/document.h"
#include "conjure/core/uuid.h"
#include "conjure/core/vfs/memory_file_system.h"
#include "conjure/editor/builtin_commands.h"
#include "conjure/editor/command_dispatcher.h"
#include "conjure/editor/editor_application.h"
#include "conjure/editor/editor_context.h"
#include "conjure/editor/editor_error.h"
#include "conjure/editor/editor_options.h"
#include "conjure/editor/event_broadcaster.h"
#include "conjure/editor/log_category.h"
#include "conjure/editor/play_controller.h"
#include "conjure/editor/protocol/protocol_message.h"
#include "conjure/engine/application.h"
#include "conjure/engine/engine_config.h"
#include "conjure/engine/project.h"
#include "conjure/scene/scene_manager.h"

#include <filesystem>
#include <format>
#include <fstream>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

// editor_context.h: the service owner every command runs against.

using namespace conjure;
using namespace conjure::editor;

namespace {
    class counting_task final : public frame_task {
        private:
            int *mTicks = nullptr;
            int mRemaining = 0;

        public:
            counting_task(int *ticks, int remaining) :
                mTicks(ticks),
                mRemaining(remaining) {}

            ~counting_task() override = default;

        public:
            bool tick(editor_context &) override {
                ++*mTicks;
                --mRemaining;
                return mRemaining <= 0;
            }
    };

    /// <summary>
    /// The payloads of the selection.changed events the editor's client received since its last clear_events().
    /// </summary>
    std::vector<core::json::document> selection_payloads(const editor_test::headless_editor &editor) {
        std::vector<core::json::document> payloads = {};
        for (const protocol_event &event : editor.events()) {
            if (event.name == "selection.changed") {
                payloads.push_back(event.data);
            }
        }
        return payloads;
    }

    /// <summary>
    /// A scene file with one named root entity.
    /// </summary>
    std::string one_entity_scene_text(std::string_view name, std::string_view guid) {
        return std::format(
          R"({{"format":"conjure.scene","version":1,"name":"{0}","settings":{{}},"roots":["{1}"],)"
          R"("archive":{{"entities":[{{"components":{{"conjure.scene.name":{{"value":"{0}"}},)"
          R"("conjure.scene.guid":{{"value":"{1}"}}}}}}]}}}})",
          name,
          guid
        );
    }
}

TEST_SUITE("editor/editor_context") {

    TEST_CASE("editor_context - binds the engine services and reports the project and scene state") {
        editor_test::headless_editor editor = {};
        editor_context &context = editor.context();
        CHECK(context.headless());
        CHECK_FALSE(context.offscreen());
        CHECK(context.has_project());
        REQUIRE(context.project() != nullptr);
        CHECK(context.project()->description().name == "editor_test");
        CHECK(context.has_scene());
        CHECK(context.scene_path() == "assets://scenes/test.scene.json");
        CHECK_FALSE(context.scene_dirty());
        CHECK(context.components().types().size() > 0);
        CHECK(context.input() != nullptr);
        CHECK(context.modules() != nullptr);
        CHECK(context.asset_database() != nullptr);
        CHECK(context.assets() != nullptr);
        CHECK(context.host() != nullptr);
        CHECK(context.server() != nullptr);
        CHECK(context.registry().size() == builtin_command_count());
        CHECK(context.play().is_editing());
        CHECK(context.frame_index() == context.engine().time().frameIndex);
    }

    TEST_CASE("regression - N3 the open scene is the first loaded scene, not a later additive one") {
        engine::application_desc desc = {};
        desc.config = engine::engine_config::headless_defaults();
        desc.config.scripting.enabled = false;
        engine::application application(std::move(desc));
        CONJURE_REQUIRE_OK(application.initialise());
        REQUIRE(application.context().scenes != nullptr);
        scene::scene_manager &scenes = *application.context().scenes;
        core::vfs::memory_file_system files = {};
        CONJURE_REQUIRE_OK(files.write_text_file(
          "scenes/first.scene.json",
          one_entity_scene_text("First", "7a3c1e52-0d4b-4f6a-9b2e-3c5d7e9f1a01")
        ));
        CONJURE_REQUIRE_OK(files.write_text_file(
          "scenes/added.scene.json",
          one_entity_scene_text("Added", "7a3c1e52-0d4b-4f6a-9b2e-3c5d7e9f1a02")
        ));
        CONJURE_REQUIRE_OK(scenes.load_file(files, "scenes/first.scene.json", scene::load_mode::REPLACE));
        CONJURE_REQUIRE_OK(scenes.load_file(files, "scenes/added.scene.json", scene::load_mode::ADDITIVE));
        REQUIRE(scenes.loaded_scenes().size() == 2);

        // Declared after the application so it is destroyed first; the engine shuts down while the context's module
        // consumer is still alive, as editor_application::shutdown does it.
        editor_context context(application, nullptr, editor_display_mode::HEADLESS);
        CHECK(context.has_scene());
        CHECK(context.scene_path() == "scenes/first.scene.json");
        CHECK_FALSE(context.scene_dirty());
        application.shutdown();
    }

    TEST_CASE("editor_context - the display mode decides headless, offscreen and the viewport commands") {
        engine::application_desc desc = {};
        desc.config = engine::engine_config::headless_defaults();
        desc.config.scripting.enabled = false;
        engine::application application(std::move(desc));
        CONJURE_REQUIRE_OK(application.initialise());
        editor_display_mode mode = editor_display_mode::HEADLESS;
        bool headless = true;
        bool offscreen = false;
        SUBCASE("WINDOWED") {
            mode = editor_display_mode::WINDOWED;
            headless = false;
        }
        SUBCASE("HEADLESS") {
            mode = editor_display_mode::HEADLESS;
        }
        SUBCASE("HEADLESS_OFFSCREEN") {
            mode = editor_display_mode::HEADLESS_OFFSCREEN;
            offscreen = true;
        }
        // Declared after the application so it is destroyed first; the engine shuts down while the context's module
        // consumer is still alive, as editor_application::shutdown does it.
        editor_context context(application, nullptr, mode);
        const editor_display_mode reported = context.display_mode();
        const bool reportedHeadless = context.headless();
        const bool reportedOffscreen = context.offscreen();
        const core::json::document state = context.describe_state();

        // view.debug_draw needs a viewport: a window or an off-screen target, not a bare headless session.
        protocol_request request = {};
        request.id = make_id(1);
        request.command = "view.debug_draw";
        request.args = core::json::object({{"bounds", true}});
        const std::optional<protocol_response> response = context.dispatcher().dispatch(request);
        const bool boundsDrawn = context.debug_draw().bounds;
        application.shutdown();

        CHECK(reported == mode);
        CHECK(reportedHeadless == headless);
        CHECK(reportedOffscreen == offscreen);
        CHECK(state.root()["headless"].as_bool() == headless);
        CHECK(state.root()["offscreen"].as_bool() == offscreen);
        REQUIRE(response.has_value());
        CHECK(response->ok == (mode != editor_display_mode::HEADLESS));
        CHECK(boundsDrawn == (mode != editor_display_mode::HEADLESS));
        if (mode == editor_display_mode::HEADLESS) {
            CHECK(response->errorCode == protocol_error_code::NOT_AVAILABLE);
        }
    }

    TEST_CASE("regression - code sweep F634 the editor_context constructor takes a display mode, not two bools") {
        // (engine, host, true, true) and (engine, host, false, true) both compiled: the second asked for an
        // off-screen target with a window. Only the three display modes can be written now.
        CHECK(std::is_constructible_v<editor_context, engine::application &, editor_host *, editor_display_mode>);
        CHECK_FALSE(std::is_constructible_v<editor_context, engine::application &, editor_host *, bool, bool>);
        CHECK_FALSE(std::is_constructible_v<editor_context, engine::application &, editor_host *, bool>);
        CHECK_FALSE(std::is_constructible_v<editor_context, engine::application &, editor_host *, int>);
    }

    TEST_CASE("display_mode_for - the headless and offscreen options pick the display mode") {
        editor_options options = {};
        editor_display_mode expected = editor_display_mode::WINDOWED;
        SUBCASE("neither is WINDOWED") {
            expected = editor_display_mode::WINDOWED;
        }
        SUBCASE("headless alone is HEADLESS") {
            options.headless = true;
            expected = editor_display_mode::HEADLESS;
        }
        SUBCASE("headless with offscreen is HEADLESS_OFFSCREEN") {
            options.headless = true;
            options.offscreen = true;
            expected = editor_display_mode::HEADLESS_OFFSCREEN;
        }
        SUBCASE("offscreen without headless is WINDOWED") {
            // parse_editor_options refuses this pair; options built in code get a window, as make_engine_desc does.
            options.offscreen = true;
            expected = editor_display_mode::WINDOWED;
        }
        CHECK(display_mode_for(options) == expected);
    }

    TEST_CASE("editor_context - editor_application builds the context in the display mode of its options") {
        // A WINDOWED start needs a real window, so only the two headless modes are started here; display_mode_for
        // above covers the options that give WINDOWED.
        testing::log_capture logs = {};
        editor_options options = {};
        options.headless = true;
        options.aiPort = u16{0};
        options.engineArguments = {"--log-level", "warn"};
        editor_display_mode expected = editor_display_mode::HEADLESS;
        SUBCASE("headless") {
            expected = editor_display_mode::HEADLESS;
        }
        SUBCASE("headless with offscreen") {
#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
            options.offscreen = true;
            expected = editor_display_mode::HEADLESS_OFFSCREEN;
#else
            MESSAGE("an --offscreen start needs the renderer, which this build leaves out");
            return;
#endif
        }
        editor_application application = {};
        CONJURE_REQUIRE_OK(application.initialise(options, {}));
        const editor_context &context = application.context();
        CHECK(context.display_mode() == expected);
        CHECK(context.headless());
        CHECK(context.offscreen() == (expected == editor_display_mode::HEADLESS_OFFSCREEN));
        application.shutdown();
    }

    TEST_CASE("editor_context - the revision counts mutations and marks the scene dirty") {
        editor_test::headless_editor editor = {};
        editor_context &context = editor.context();
        const u64 before = context.revision();
        (void) editor.create_entity("a");
        CHECK(context.revision() == before + 1);
        CHECK(context.scene_dirty());
        context.set_scene_dirty(false);
        CHECK_FALSE(context.scene_dirty());
        const u64 bumped = context.bump_revision();
        CHECK(bumped == before + 2);
        // The revision alone is not a scene edit: scene.save keeps the flag clear after its own bump.
        CHECK_FALSE(context.scene_dirty());
        (void) editor.ok("history.undo");
        CHECK(context.scene_dirty());
    }

    TEST_CASE("editor_context - normalise_scene_path adds the mount and the extension") {
        CHECK(editor_context::normalise_scene_path("scenes/level") == "assets://scenes/level.scene.json");
        CHECK(editor_context::normalise_scene_path("/scenes/level.json") == "assets://scenes/level.scene.json");
        CHECK(editor_context::normalise_scene_path("assets://a.scene.json") == "assets://a.scene.json");
        CHECK(editor_context::normalise_scene_path("project://x/y") == "project://x/y.scene.json");
        CHECK(editor_context::normalise_scene_path("  ").empty());
    }

    TEST_CASE("regression - F754 normalise_scene_path canonicalises separators, dot segments and the suffix case") {
        CHECK(editor_context::normalise_scene_path("scenes\\back") == "assets://scenes/back.scene.json");
        CHECK(editor_context::normalise_scene_path("scenes//d") == "assets://scenes/d.scene.json");
        CHECK(
          editor_context::normalise_scene_path("./scenes/../scenes/x.scene.json") == "assets://scenes/x.scene.json"
        );
        CHECK(editor_context::normalise_scene_path("scenes/MAIN.SCENE.JSON") == "assets://scenes/MAIN.SCENE.JSON");
        CHECK(editor_context::normalise_scene_path("scenes/level.JSON") == "assets://scenes/level.scene.json");
        CHECK(editor_context::normalise_scene_path("assets://scenes\\a//b") == "assets://scenes/a/b.scene.json");
        CHECK(editor_context::normalise_scene_path("\\scenes\\level") == "assets://scenes/level.scene.json");
        CHECK(editor_context::normalise_scene_path("../out").empty());
        CHECK(editor_context::normalise_scene_path("..").empty());
        CHECK(editor_context::normalise_scene_path("scenes/../../out").empty());
        CHECK(editor_context::normalise_scene_path("assets://../out").empty());
        CHECK(editor_context::normalise_scene_path("./").empty());
        // A drive-letter path names a place outside every mount; a rooted ".." stays at the mount root.
        CHECK(editor_context::normalise_scene_path("C:/elsewhere/x").empty());
        CHECK(editor_context::normalise_scene_path("assets://C:/elsewhere/x").empty());
        CHECK(editor_context::normalise_scene_path("/../x") == "assets://x.scene.json");
    }

    TEST_CASE("editor_context - names_asset and is_open_scene tell the asset a path names") {
        editor_test::headless_editor editor = {};
        editor_context &context = editor.context();
        (void) editor.ok("scene.save");
        (void) editor_test::write_tiny_png(editor.project_directory() / "assets" / "plain.png");
        const core::json::document png = editor.ok("asset.import", core::json::object({{"source", "plain.png"}}));
        const std::optional<core::uuid> pngId = core::uuid::parse(png.root()["uuid"].as_string());
        const std::optional<core::uuid> sceneId = core::uuid::parse(editor.ok(
          "asset.info",
          core::json::object({{"asset", editor_test::TEST_SCENE}})
        ).root()["uuid"].as_string());
        REQUIRE(pngId.has_value());
        REQUIRE(sceneId.has_value());

        CHECK(context.names_asset("assets://plain.png", *pngId));
        CHECK(context.names_asset(editor_test::TEST_SCENE, *sceneId));
        // Another asset's path, no path, a file that is not there, another mount and an unknown asset name none.
        CHECK_FALSE(context.names_asset(editor_test::TEST_SCENE, *pngId));
        CHECK_FALSE(context.names_asset("", *pngId));
        CHECK_FALSE(context.names_asset("assets://absent.png", *pngId));
        CHECK_FALSE(context.names_asset("project://assets/plain.png", *pngId));
        CHECK_FALSE(context.names_asset("plain.png", *pngId));
        CHECK_FALSE(context.names_asset("assets://plain.png", core::uuid::generate()));

        CHECK(context.is_open_scene(*sceneId));
        CHECK_FALSE(context.is_open_scene(*pngId));
        // A scene that was never saved has no file, and a closed one is not open.
        (void) editor.ok("scene.new", core::json::object({{"name", "fresh"}}));
        CHECK_FALSE(context.is_open_scene(*sceneId));
        (void) editor.ok("scene.open", core::json::object({{"path", "scenes/test"}}));
        CHECK(context.is_open_scene(*sceneId));
        (void) editor.ok("scene.close");
        CHECK_FALSE(context.is_open_scene(*sceneId));
    }

    TEST_CASE("regression - code sweep F656 names_asset compares files, not spellings") {
        editor_test::headless_editor editor = {};
        editor_context &context = editor.context();
        (void) editor.ok("scene.save");
        const std::optional<core::uuid> sceneId = core::uuid::parse(editor.ok(
          "asset.info",
          core::json::object({{"asset", editor_test::TEST_SCENE}})
        ).root()["uuid"].as_string());
        REQUIRE(sceneId.has_value());
        // A second name of the same file that the database has not registered: a hard link stands in, on every
        // volume that has hard links, for the other spelling a volume that ignores case accepts.
        const std::filesystem::path alias = editor.project_directory() / "assets" / "scenes" / "alias.scene.json";
        std::error_code linkError = {};
        std::filesystem::create_hard_link(editor_test::test_scene_file(editor), alias, linkError);
        if (linkError) {
            MESSAGE("the volume refused a hard link (" << linkError.message() << "): the alias half is skipped");
            return;
        }
        CHECK(context.names_asset("assets://scenes/alias.scene.json", *sceneId));
        (void) editor.ok("scene.open", core::json::object({{"path", "scenes/alias"}}));
        REQUIRE(context.scene_path() == "assets://scenes/alias.scene.json");
        CHECK(context.is_open_scene(*sceneId));
        // A file of its own is another asset however alike the names are.
        std::error_code copyError = {};
        std::filesystem::copy_file(
          editor_test::test_scene_file(editor),
          editor.project_directory() / "assets" / "scenes" / "copy.scene.json",
          copyError
        );
        REQUIRE_FALSE(copyError);
        CHECK_FALSE(context.names_asset("assets://scenes/copy.scene.json", *sceneId));
    }

    TEST_CASE("editor_context - the viewport aspect starts at 16:9 and ignores values that are not positive") {
        editor_test::headless_editor editor = {};
        editor_context &context = editor.context();
        CHECK(context.viewport_aspect() == doctest::Approx(16.0f / 9.0f));
        context.set_viewport_aspect(1.5f);
        CHECK(context.viewport_aspect() == doctest::Approx(1.5f));
        context.set_viewport_aspect(0.0f);
        context.set_viewport_aspect(-2.0f);
        context.set_viewport_aspect(std::numeric_limits<f32>::infinity());
        context.set_viewport_aspect(std::numeric_limits<f32>::quiet_NaN());
        CHECK(context.viewport_aspect() == doctest::Approx(1.5f));
    }

    TEST_CASE("regression - F727 an editor.wait_frames whose client disconnected leaves the frame tasks at once") {
        editor_test::headless_editor editor = {};
        editor_context &context = editor.context();
        command_dispatcher &dispatcher = context.dispatcher();
        const u32 client = context.events().register_client({});
        const usize tasksBefore = context.frame_task_count();
        protocol_request request = {};
        request.id = make_id(1);
        request.command = "editor.wait_frames";
        request.args = core::json::object({{"frames", 100000000}});
        request.clientId = client;

        SUBCASE("the client disconnected") {
            CHECK_FALSE(dispatcher.dispatch(request).has_value());
            CHECK(context.frame_task_count() == tasksBefore + 1);
            dispatcher.cancel_client(client);
            editor.pump(1);
            CHECK(context.frame_task_count() == tasksBefore);
        }
        SUBCASE("the request timed out") {
            dispatcher.set_async_timeout_seconds(0.0);
            CHECK_FALSE(dispatcher.dispatch(request).has_value());
            CHECK(context.frame_task_count() == tasksBefore + 1);
            editor.pump(1);
            dispatcher.set_async_timeout_seconds(30.0);
            CHECK(dispatcher.pending_count() == 0);
            CHECK(context.frame_task_count() == tasksBefore);
        }
        context.events().unregister_client(client);
    }

    TEST_CASE("regression - F481 editor.exec_file whose file leaves a transaction open records one entry") {
        editor_test::headless_editor editor = {};
        const std::filesystem::path file = editor.project_directory() / "open_transaction.json";
        {
            std::ofstream output(file, std::ios::binary | std::ios::trunc);
            REQUIRE(output.is_open());
            output << R"([{"cmd":"editor.begin_transaction","args":{"label":"Left open"}},)"
                      R"({"cmd":"entity.create","args":{"name":"a"}},{"cmd":"entity.create","args":{"name":"b"}}])";
        }
        const usize clientsBefore = editor.context().events().client_count();
        const core::json::document summary = editor.ok(
          "editor.exec_file",
          core::json::object({{"path", file.generic_string()}})
        );
        CHECK(summary.root()["succeeded"].as_u64() == 3);
        CHECK_FALSE(editor.context().undo().has_open_transaction());
        REQUIRE(editor.context().undo().size() == 1);
        CHECK(editor.context().undo().entries().front()->label == "Left open");
        CHECK(editor.context().undo().entries().front()->undo.size() == 2);
        CHECK(editor.context().events().client_count() == clientsBefore);
        (void) editor.ok("history.undo");
        CHECK(editor.scene().world().entity_count() == 0);
    }

    TEST_CASE("regression - F481 editor.exec_file stops dispatching when its issuing client disconnects") {
        editor_test::headless_editor editor = {};
        editor_context &context = editor.context();
        const std::filesystem::path file = editor.project_directory() / "wait_then_create.json";
        {
            std::ofstream output(file, std::ios::binary | std::ios::trunc);
            REQUIRE(output.is_open());
            output << R"([{"cmd":"entity.create","args":{"name":"First"}},)"
                      R"({"cmd":"editor.wait_frames","args":{"frames":5}},)"
                      R"({"cmd":"entity.create","args":{"name":"Later"}}])";
        }
        const usize clientsBefore = context.events().client_count();
        int responses = 0;
        const u32 client = context.events().register_client({});
        context.register_response_sink(client, [&responses](const protocol_response &) {
            ++responses;
        });
        protocol_request request = {};
        request.id = make_id(3);
        request.command = "editor.exec_file";
        request.args = core::json::object({{"path", file.generic_string()}});
        request.clientId = client;
        CHECK_FALSE(context.dispatcher().dispatch(request).has_value());
        // The first frame runs the first request and parks on the wait; then the issuer goes away.
        editor.pump(1);
        CHECK(editor.ok("entity.find", core::json::object({{"name", "First"}})).root()["total"].as_u64() == 1);
        context.dispatcher().cancel_client(client);
        editor.pump(10);
        CHECK(editor.ok("entity.find", core::json::object({{"name", "Later"}})).root()["total"].as_u64() == 0);
        CHECK(context.frame_task_count() == 0);
        CHECK(context.dispatcher().pending_count() == 0);
        CHECK(responses == 0);
        context.unregister_response_sink(client);
        context.events().unregister_client(client);
        CHECK(context.events().client_count() == clientsBefore);
    }

    TEST_CASE("editor_context - describe_state carries project, scene, play state, selection and clients") {
        editor_test::headless_editor editor = {};
        const std::string guid = editor.create_entity("picked");
        (void) editor.ok("selection.set", core::json::object({{"entities", core::json::array({guid})}}));
        const core::json::document state = editor.context().describe_state();
        CHECK(state.root()["project"]["name"].as_string() == "editor_test");
        CHECK(state.root()["scene"]["open"].as_bool());
        CHECK(state.root()["playState"].as_string() == "editing");
        CHECK(state.root()["selection"].size() == 1);
        CHECK(state.root()["headless"].as_bool());
        CHECK(state.root().contains("port"));
        CHECK(state.root().contains("frame"));
        CHECK(state.root()["revision"].as_u64() == editor.context().revision());
        // No project is being asked about.
        REQUIRE(state.root().contains("trustPrompt"));
        CHECK(state.root()["trustPrompt"].is_null());
    }

    TEST_CASE("editor_context - the trust prompt is raised only where a window can ask, and closes for its project") {
        editor_test::headless_editor editor = {};
        editor_context &context = editor.context();
        const std::filesystem::path asked = editor.project_directory() / "asked";
        const std::filesystem::path other = editor.project_directory() / "other";
        // The application hands its project_trust to the context and leaves prompts off for a headless session.
        CHECK(context.trust() == &editor.application().trust());
        CHECK_FALSE(context.trust_prompt_enabled());
        CHECK_FALSE(context.raise_trust_prompt(asked));
        CHECK_FALSE(context.trust_prompt().has_value());

        context.set_trust_prompt_enabled(true);
        CHECK(context.trust_prompt_enabled());
        // No project, no question.
        CHECK_FALSE(context.raise_trust_prompt({}));
        CHECK_FALSE(context.trust_prompt().has_value());
        // The question names the folder an answer records: the project key, resolved by the file system. Compared
        // as keys, not with the spelling given: on macOS the scratch folder lies behind a link (/var is
        // /private/var), and on Windows a short 8.3 name in it is resolved to its long form.
        const std::string askedKey = editor_test::project_key_of(asked);
        const auto promptKey = [&context]() {
            REQUIRE(context.trust_prompt().has_value());
            return editor_test::project_key_of(*context.trust_prompt());
        };
        REQUIRE(context.raise_trust_prompt(asked));
        CHECK(promptKey() == askedKey);
        CHECK(context.describe_state().root()["trustPrompt"].as_string() == context.trust_prompt()->generic_string());

        SUBCASE("a later question replaces it") {
            REQUIRE(context.raise_trust_prompt(other));
            CHECK(promptKey() == editor_test::project_key_of(other));
        }
        SUBCASE("it closes for its own project, whatever the spelling, and for no other") {
            context.close_trust_prompt_for(other);
            CHECK(context.trust_prompt().has_value());
            context.close_trust_prompt_for(asked / "sub" / "..");
            CHECK_FALSE(context.trust_prompt().has_value());
            // Closing when nothing is asked is harmless.
            context.close_trust_prompt_for(asked);
            CHECK_FALSE(context.trust_prompt().has_value());
        }
        SUBCASE("it can be dismissed unanswered") {
            context.dismiss_trust_prompt();
            CHECK_FALSE(context.trust_prompt().has_value());
            CHECK(context.describe_state().root()["trustPrompt"].is_null());
            CHECK_FALSE(editor.application().trust().is_trusted(asked));
        }
        SUBCASE("it survives an engine restart over another project and closes when its own project opens") {
            editor_test::write_project(asked, "asked");
            editor_test::write_project(other, "other");
            editor.trust_project(other);
            (void) editor.ok("project.open", core::json::object({{"path", other.generic_string()}}));
            editor.pump(1);
            REQUIRE(context.project()->description().name == "other");
            CHECK(promptKey() == askedKey);
            editor.trust_project(asked);
            (void) editor.ok("project.open", core::json::object({{"path", asked.generic_string()}}));
            editor.pump(1);
            REQUIRE(context.project()->description().name == "asked");
            CHECK_FALSE(context.trust_prompt().has_value());
        }
        SUBCASE("turning prompts off keeps a question that is already there") {
            context.set_trust_prompt_enabled(false);
            CHECK(context.trust_prompt().has_value());
            CHECK_FALSE(context.raise_trust_prompt(other));
            CHECK(promptKey() == askedKey);
        }
    }

    TEST_CASE("regression - code sweep F187 editor_context the trust question shows the folder an answer records, not "
              "the spelling it was given") {
        editor_test::headless_editor editor = {};
        editor_context &context = editor.context();
        context.set_trust_prompt_enabled(true);
        std::filesystem::path spelling = {};
        SUBCASE("a spelling with a detour") {
            spelling = editor.project_directory() / "asked" / "sub" / "..";
        }
        SUBCASE("a spelling relative to the working directory") {
            spelling = "conjure_f187_relative_project";
        }
        CAPTURE(spelling.generic_string());
        // project.trust records the key; the window shows the question's text and editor.state reports it, so all
        // three must be the same folder, written out in full.
        const std::string key = editor_test::project_key_of(spelling);
        REQUIRE(context.raise_trust_prompt(spelling));
        REQUIRE(context.trust_prompt().has_value());
        const std::u8string shown = context.trust_prompt()->generic_u8string();
        CHECK(std::string(shown.begin(), shown.end()) == key);
        CHECK(context.trust_prompt()->is_absolute());
        CHECK(context.describe_state().root()["trustPrompt"].as_string() == context.trust_prompt()->generic_string());
    }

    TEST_CASE("editor_context - frame tasks tick until they finish") {
        editor_test::headless_editor editor = {};
        editor_context &context = editor.context();
        int ticks = 0;
        context.add_frame_task(std::make_unique<counting_task>(&ticks, 3));
        context.add_frame_task(nullptr);
        CHECK(context.frame_task_count() == 1);
        editor.pump(1);
        CHECK(ticks == 1);
        editor.pump(5);
        CHECK(ticks == 3);
        CHECK(context.frame_task_count() == 0);
    }

    TEST_CASE("regression - editor.exec_file whose file opens a project answers CANCELLED and leaves no sink") {
        editor_test::headless_editor editor = {};
        const std::filesystem::path other = editor.project_directory() / "second";
        (void) editor.ok("project.create", core::json::object({{"path", other.generic_string()}, {"name", "Second"}}));
        // project.open restarts the engine at the next frame boundary; the wait_frames that follows is still
        // parked when that happens.
        const std::filesystem::path file = editor.project_directory() / "open_then_wait.json";
        {
            std::ofstream output(file, std::ios::binary | std::ios::trunc);
            REQUIRE(output.is_open());
            output << std::format(
              R"([{{"cmd":"project.open","args":{{"path":"{}"}}}},)"
              R"({{"cmd":"editor.wait_frames","args":{{"frames":5}}}}])",
              other.generic_string()
            );
        }
        const usize clientsBefore = editor.context().events().client_count();
        const protocol_response response = editor.send(
          "editor.exec_file",
          core::json::object({{"path", file.generic_string()}})
        );
        CHECK_FALSE(response.ok);
        CHECK(response.errorCode == protocol_error_code::CANCELLED);
        REQUIRE(editor.context().project() != nullptr);
        CHECK(editor.context().project()->description().name == "Second");
        // Nothing of the old task survives: no pending ticket, no frame task, no exec client, and pumping well
        // past where a stale ticket would have timed out delivers nothing to a destroyed sink.
        CHECK(editor.context().dispatcher().pending_count() == 0);
        CHECK(editor.context().frame_task_count() == 0);
        CHECK(editor.context().events().client_count() == clientsBefore);
        editor.context().dispatcher().set_async_timeout_seconds(0.0);
        editor.pump(5);
        editor.context().dispatcher().set_async_timeout_seconds(30.0);
        CHECK(editor.ok("project.info").root()["name"].as_string() == "Second");
    }

    TEST_CASE("regression - rebind cancels the pending asynchronous requests instead of letting them time out") {
        editor_test::headless_editor editor = {};
        command_dispatcher &dispatcher = editor.context().dispatcher();
        std::optional<protocol_response> captured = std::nullopt;
        const u32 client = editor.context().events().register_client({});
        editor.context().register_response_sink(client, [&captured](const protocol_response &response) {
            captured = response;
        });
        protocol_request request = {};
        request.id = make_id(9);
        request.command = "editor.wait_frames";
        request.args = core::json::object({{"frames", 100000}});
        request.clientId = client;
        CHECK_FALSE(dispatcher.dispatch(request).has_value());
        CHECK(dispatcher.pending_count(client) == 1);
        const std::filesystem::path other = editor.project_directory() / "second";
        (void) editor.ok("project.create", core::json::object({{"path", other.generic_string()}, {"name", "Second"}}));
        (void) editor.ok("project.open", core::json::object({{"path", other.generic_string()}}));
        editor.pump(2);
        REQUIRE(captured.has_value());
        CHECK_FALSE(captured->ok);
        CHECK(captured->errorCode == protocol_error_code::CANCELLED);
        CHECK(captured->id.root().as_i64() == 9);
        CHECK(dispatcher.pending_count() == 0);
        CHECK(editor.context().frame_task_count() == 0);
        editor.context().unregister_response_sink(client);
        editor.context().events().unregister_client(client);
    }

    TEST_CASE("editor_context - response sinks receive the responses of their client") {
        editor_test::headless_editor editor = {};
        editor_context &context = editor.context();
        std::vector<protocol_response> received = {};
        const u32 client = context.events().register_client({});
        context.register_response_sink(client, [&received](const protocol_response &response) {
            received.push_back(response);
        });
        protocol_response response = make_ok_response(make_id(4).root(), core::json::document::object());
        response.clientId = client;
        context.deliver_response(response);
        REQUIRE(received.size() == 1);
        CHECK(received[0].id.root().as_i64() == 4);
        context.unregister_response_sink(client);
        context.deliver_response(response);
        CHECK(received.size() == 1);
        context.events().unregister_client(client);
    }

    TEST_CASE("editor_context - console records are forwarded as log events to subscribed clients") {
        editor_test::headless_editor editor = {};
        editor_context &context = editor.context();
        context.events().set_subscriptions(editor.client_id(), default_subscriptions() | event_bit(editor_event::LOG));
        context.events().set_min_log_level(editor.client_id(), core::log_level::WARN);
        editor.clear_events();
        CONJURE_LOG_WARN(LOG_EDITOR, "context forwarding probe");
        editor.pump(1);
        bool found = false;
        for (const protocol_event &event : editor.events()) {
            if (
              event.name == "log" && event.data.root()["message"].as_string().find(
                "forwarding probe"
              ) != std::string::npos
            ) {
                found = true;
                CHECK(event.data.root()["level"].as_string() == core::to_string(core::log_level::WARN));
                CHECK(event.data.root()["category"].as_string() == "editor");
            }
        }
        CHECK(found);
    }

    TEST_CASE("editor_context - the selection emits selection.changed") {
        editor_test::headless_editor editor = {};
        const std::string guid = editor.create_entity("sel");
        editor.clear_events();
        (void) editor.ok("selection.set", core::json::object({{"entities", core::json::array({guid})}}));
        CHECK(editor.saw_event("selection.changed"));
    }

    TEST_CASE("regression - F657 every selection.changed carries guids and primary, null when nothing is selected") {
        editor_test::headless_editor editor = {};
        const std::string first = editor.create_entity("first");
        const std::string second = editor.create_entity("second");

        // A selection change: the primary is the guid named last.
        editor.clear_events();
        (void) editor.ok("selection.set", core::json::object({{"entities", core::json::array({first, second})}}));
        std::vector<core::json::document> payloads = selection_payloads(editor);
        REQUIRE(payloads.size() == 1);
        CHECK(payloads[0].root()["guids"].size() == 2);
        CHECK(payloads[0].root()["primary"].as_string() == second);

        // A primary-only change carries the new primary.
        editor.clear_events();
        (void) editor.ok(
          "selection.set",
          core::json::object({{"entities", core::json::array({first})}, {"mode", "add"}})
        );
        payloads = selection_payloads(editor);
        REQUIRE(payloads.size() == 1);
        CHECK(payloads[0].root()["primary"].as_string() == first);

        // play.stop emits too, with the same shape.
        (void) editor.ok("play.start");
        editor.clear_events();
        (void) editor.ok("play.stop");
        payloads = selection_payloads(editor);
        REQUIRE_FALSE(payloads.empty());
        for (const core::json::document &payload : payloads) {
            CHECK(payload.root()["guids"].size() == 2);
            REQUIRE(payload.root().contains("primary"));
            CHECK(payload.root()["primary"].as_string() == first);
        }

        // Nothing selected: primary null, as selection.get answers.
        editor.clear_events();
        (void) editor.ok("selection.clear");
        payloads = selection_payloads(editor);
        REQUIRE(payloads.size() == 1);
        CHECK(payloads[0].root()["guids"].size() == 0);
        REQUIRE(payloads[0].root().contains("primary"));
        CHECK(payloads[0].root()["primary"].is_null());
        CHECK(editor.ok("selection.get").root()["primary"].is_null());

        // The emitter itself, called directly, sends the same shape.
        editor.clear_events();
        editor.context().emit_selection_changed();
        payloads = selection_payloads(editor);
        REQUIRE(payloads.size() == 1);
        CHECK(payloads[0].root()["guids"].size() == 0);
        CHECK(payloads[0].root()["primary"].is_null());
    }

    TEST_CASE("editor_context - selection_payload is the guids and primary of selection.get and selection.changed") {
        editor_test::headless_editor editor = {};
        const editor_context &context = editor.context();

        // Nothing selected: an empty guid list and primary null, nothing else.
        const core::json::document empty = context.selection_payload();
        CHECK(empty.root()["guids"].size() == 0);
        REQUIRE(empty.root().contains("primary"));
        CHECK(empty.root()["primary"].is_null());
        CHECK(empty.root().size() == 2);

        const std::string first = editor.create_entity("first");
        const std::string second = editor.create_entity("second");
        editor.clear_events();
        const core::json::document answer = editor.ok(
          "selection.set",
          core::json::object({{"entities", core::json::array({second, first})}})
        );
        const core::json::document payload = context.selection_payload();
        CHECK(payload.root()["guids"].size() == 2);
        CHECK(payload.root()["primary"].as_string() == first);
        CHECK(payload.root().size() == 2);

        // The answer is the payload plus "entities" and "count"; the event carries the payload as it is.
        CHECK(answer.root()["guids"] == payload.root()["guids"]);
        CHECK(answer.root()["primary"] == payload.root()["primary"]);
        CHECK(answer.root()["count"].as_u64() == 2);
        CHECK(answer.root()["entities"].size() == 2);
        const std::vector<core::json::document> events = selection_payloads(editor);
        REQUIRE(events.size() == 1);
        CHECK(events[0] == payload);
    }

    TEST_CASE("editor_context - without a project the context still works and commands report PROJECT_NOT_OPEN") {
        editor_test::headless_editor_options options = {};
        options.withProject = false;
        options.withScene = false;
        editor_test::headless_editor editor(options);
        CHECK_FALSE(editor.context().has_project());
        CHECK(editor.context().project() == nullptr);
        CHECK(editor.fail("project.info") == protocol_error_code::PROJECT_NOT_OPEN);
        CHECK(editor.fail("entity.create", core::json::object({{"name", "x"}})) == protocol_error_code::SCENE_NOT_OPEN);
        const core::json::document state = editor.ok("editor.state");
        CHECK(state.root()["project"].is_null());
    }

    TEST_CASE("regression - code sweep F187 editor.exec_file reads a relative path against the project root") {
        editor_test::headless_editor editor = {};
        const std::filesystem::path root = editor.context().project()->root_directory();
        std::filesystem::create_directories(root / "commands");
        {
            std::ofstream output(root / "commands" / "nested.json", std::ios::binary | std::ios::trunc);
            REQUIRE(output.is_open());
            output << R"([{"cmd":"entity.create","args":{"name":"from a relative file"}}])";
        }
        const core::json::document summary = editor.ok(
          "editor.exec_file",
          core::json::object({{"path", "commands/nested.json"}})
        );
        CHECK(summary.root()["succeeded"].as_u64() == 1);
        CHECK(editor.scene().world().entity_count() == 1);

        // A relative path that names nothing is looked for in the project, and the answer says where.
        const protocol_response missing = editor.send(
          "editor.exec_file",
          core::json::object({{"path", "commands/none.json"}})
        );
        REQUIRE_FALSE(missing.ok);
        CHECK(missing.errorCode == protocol_error_code::PATH_NOT_FOUND);
        const std::string expected = (root / "commands" / "none.json").generic_string();
        CHECK_MESSAGE(missing.errorMessage.find(expected) != std::string::npos, missing.errorMessage);
        // An empty path is not turned into the project root: it names nothing, as before.
        const protocol_error_code emptyPath = editor.fail("editor.exec_file", core::json::object({{"path", ""}}));
        CHECK(emptyPath == protocol_error_code::PATH_NOT_FOUND);
    }

    TEST_CASE("editor.exec_file - without a project a relative path is left to the working directory") {
        editor_test::headless_editor_options options = {};
        options.withProject = false;
        options.withScene = false;
        editor_test::headless_editor editor(options);
        const protocol_response missing = editor.send(
          "editor.exec_file",
          core::json::object({{"path", "conjure_f187_no_such_folder/none.json"}})
        );
        REQUIRE_FALSE(missing.ok);
        CHECK(missing.errorCode == protocol_error_code::PATH_NOT_FOUND);
        // The path is quoted as given: nothing was put in front of it.
        CHECK_MESSAGE(
          missing.errorMessage.find("'conjure_f187_no_such_folder/none.json'") != std::string::npos,
          missing.errorMessage
        );
    }
}

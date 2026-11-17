#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/temp_directory.h"

#include "editor_test_support.h"

#include "conjure/core/json/document.h"
#include "conjure/editor/editor_application.h"
#include "conjure/editor/editor_error.h"
#include "conjure/editor/project_trust.h"
#include "conjure/editor/protocol/protocol_message.h"
#include "conjure/editor/protocol/protocol_server.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

// project.trust, project.revoke_trust, project.trust_state and what project.open and project.create do about
// trust (docs/design/04_editor.md D27, 4.4). The store itself is covered in test_project_trust.cpp, the start of
// the editor in test_editor_application.cpp.

using namespace conjure;
using namespace conjure::editor;

namespace {
    [[nodiscard]] core::json::document path_args(const std::filesystem::path &path) {
        return core::json::object({{"path", path.generic_string()}});
    }

    /// <summary>
    /// A project that came from elsewhere: written by hand beside the editor's own, with a name of its own.
    /// </summary>
    [[nodiscard]] std::filesystem::path foreign_project(editor_test::headless_editor &editor, std::string_view name) {
        const std::filesystem::path root = editor.project_directory() / std::filesystem::path(name);
        editor_test::write_project(root, name);
        return root;
    }

    /// <summary>
    /// Gives the editor a trust store in a folder of its scratch project (created); false, and the case then
    /// returns, on a volume that keeps no owner-only files, where nothing can be remembered.
    /// </summary>
    [[nodiscard]] bool use_trust_store(editor_test::headless_editor &editor, const std::filesystem::path &store) {
        std::filesystem::create_directories(store.parent_path());
        if (!editor_test::owner_only_files_available(store.parent_path())) {
            return false;
        }
        editor.application().trust().set_store_file(store);
        return true;
    }
}

TEST_SUITE("editor/commands_project_trust") {

    TEST_CASE("regression - code sweep F187 project.open refuses a project nobody answered for and opens it after "
              "project.trust") {
        editor_test::headless_editor editor = {};
        const std::filesystem::path other = foreign_project(editor, "downloaded");
        const std::string entity = editor.create_entity("still here");

        const protocol_response refused = editor.send("project.open", path_args(other));
        REQUIRE_FALSE(refused.ok);
        CHECK(refused.errorCode == protocol_error_code::PROJECT_NOT_TRUSTED);
        // The answer says what to send, and its details carry the project as it would be remembered.
        CHECK(refused.errorMessage.find("project.trust") != std::string::npos);
        CHECK(refused.errorDetails.root()["path"].as_string() == editor_test::project_key_of(other));
        CHECK(refused.errorDetails.root()["state"].as_string() == "untrusted");
        // Nobody sits at a headless editor: nothing is asked there.
        CHECK_FALSE(refused.errorDetails.root()["prompt"].as_bool(true));
        CHECK_FALSE(editor.context().trust_prompt().has_value());
        CHECK_FALSE(editor.application().has_pending_project_open());
        editor.pump(2);
        CHECK(editor.ok("project.info").root()["name"].as_string() == "editor_test");
        (void) editor.ok("entity.get", core::json::object({{"entity", entity}}));
        // Asking again changes nothing: a refusal is never an answer.
        CHECK(editor.fail("project.open", path_args(other)) == protocol_error_code::PROJECT_NOT_TRUSTED);
        CHECK(editor.ok("project.trust_state", path_args(other)).root()["state"].as_string() == "untrusted");

        const core::json::document trusted = editor.ok("project.trust", path_args(other));
        CHECK(trusted.root()["path"].as_string() == editor_test::project_key_of(other));
        CHECK(trusted.root()["trusted"].as_bool());
        const core::json::document opened = editor.ok("project.open", path_args(other));
        CHECK(opened.root()["pending"].as_bool());
        editor.pump(1);
        CHECK(editor.ok("project.info").root()["name"].as_string() == "downloaded");
    }

    TEST_CASE("project.trust - the answer is for the session without a store and remembered with one") {
        editor_test::headless_editor editor = {};
        const std::filesystem::path other = foreign_project(editor, "downloaded");
        const std::filesystem::path store = editor.project_directory() / "state" / "trusted_projects.json";

        SUBCASE("no trust store: the session, whatever remember says") {
            const core::json::document trusted = editor.ok("project.trust", path_args(other));
            CHECK(trusted.root()["state"].as_string() == "session");
            CHECK_FALSE(trusted.root()["remembered"].as_bool(true));
            CHECK(editor.application().trust().state_of(other) == trust_state::SESSION);
            CHECK_FALSE(std::filesystem::exists(store));
        }
        SUBCASE("a trust store: remembered, and the entry says the editor itself answered") {
            if (!use_trust_store(editor, store)) {
                return;
            }
            const core::json::document trusted = editor.ok("project.trust", path_args(other));
            CHECK(trusted.root()["state"].as_string() == "remembered");
            CHECK(trusted.root()["remembered"].as_bool());
            const std::vector<trusted_project> remembered = editor.application().trust().remembered();
            REQUIRE(remembered.size() == 1);
            CHECK(remembered.front().path == editor_test::project_key_of(other));
            CHECK(remembered.front().origin == "editor");
        }
        SUBCASE("a trust store and remember false: the session") {
            if (!use_trust_store(editor, store)) {
                return;
            }
            core::json::document args = path_args(other);
            args.set("remember", false);
            const core::json::document trusted = editor.ok("project.trust", std::move(args));
            CHECK(trusted.root()["state"].as_string() == "session");
            CHECK_FALSE(trusted.root()["remembered"].as_bool(true));
            CHECK(editor.application().trust().remembered().empty());
        }
        SUBCASE("a protocol client: the entry names its transport") {
            if (!use_trust_store(editor, store)) {
                return;
            }
            editor_test::loopback_session session(editor);
            const core::json::document trusted = session.ok("project.trust", path_args(other));
            CHECK(trusted.root()["state"].as_string() == "remembered");
            const std::vector<trusted_project> remembered = editor.application().trust().remembered();
            REQUIRE(remembered.size() == 1);
            CHECK(remembered.front().origin == "loopback");
        }
    }

    TEST_CASE("project.trust - a path that holds no project is refused and nothing is recorded") {
        editor_test::headless_editor editor = {};
        const std::filesystem::path root = editor.project_directory();
        std::error_code ignored = {};
        std::filesystem::path target = {};
        protocol_error_code expected = protocol_error_code::NONE;
        SUBCASE("a folder that does not exist") {
            target = root / "nowhere";
            expected = protocol_error_code::PATH_NOT_FOUND;
        }
        SUBCASE("a folder without a project.json") {
            target = root / "empty_folder";
            REQUIRE(std::filesystem::create_directories(target, ignored));
            expected = protocol_error_code::PATH_NOT_FOUND;
        }
        SUBCASE("a file where the folder is expected") {
            target = root / "project.json";
            expected = protocol_error_code::IO_ERROR;
        }
        SUBCASE("a project.json that does not parse") {
            target = root / "malformed";
            REQUIRE(std::filesystem::create_directories(target, ignored));
            std::ofstream output(target / "project.json", std::ios::binary | std::ios::trunc);
            REQUIRE(output.is_open());
            output << "{ not a project";
            expected = protocol_error_code::INVALID_ARGS;
        }
        CHECK(editor.fail("project.trust", path_args(target)) == expected);
        CHECK(editor.application().trust().state_of(target) == trust_state::UNTRUSTED);
    }

    TEST_CASE("project.trust - the arguments are checked: a path is needed and remember is a boolean") {
        editor_test::headless_editor editor = {};
        const std::filesystem::path other = foreign_project(editor, "downloaded");
        CHECK(editor.fail("project.trust") == protocol_error_code::INVALID_ARGS);
        CHECK(editor.fail("project.trust", core::json::object({{"path", 7}})) == protocol_error_code::INVALID_ARGS);
        core::json::document wrongType = path_args(other);
        wrongType.set("remember", "yes");
        CHECK(editor.fail("project.trust", std::move(wrongType)) == protocol_error_code::INVALID_ARGS);
        core::json::document unknownMember = path_args(other);
        unknownMember.set("forever", true);
        CHECK(editor.fail("project.trust", std::move(unknownMember)) == protocol_error_code::INVALID_ARGS);
        CHECK(editor.fail("project.revoke_trust") == protocol_error_code::INVALID_ARGS);
        CHECK(
          editor.fail("project.revoke_trust", core::json::object({{"path", ""}})) == protocol_error_code::INVALID_ARGS
        );
        CHECK(editor.fail(
          "project.trust_state",
          core::json::object({{"path", core::json::array({1})}})
        ) == protocol_error_code::INVALID_ARGS);
        CHECK(editor.application().trust().state_of(other) == trust_state::UNTRUSTED);
    }

    TEST_CASE("project.revoke_trust - takes the answer back, leaves the open project open and asks before it is "
              "opened again") {
        editor_test::headless_editor editor = {};
        const std::filesystem::path own = editor.project_directory();
        const std::filesystem::path second = own / "second";
        (void) editor.ok("project.create", core::json::object({{"path", second.generic_string()}, {"name", "Second"}}));

        // Nothing to take back: not an error, and the answer says so.
        const core::json::document nothing = editor.ok("project.revoke_trust", path_args(own / "never_trusted"));
        CHECK_FALSE(nothing.root()["revoked"].as_bool(true));
        CHECK(nothing.root()["state"].as_string() == "untrusted");

        const core::json::document revoked = editor.ok("project.revoke_trust", path_args(own));
        CHECK(revoked.root()["revoked"].as_bool());
        CHECK(revoked.root()["path"].as_string() == editor_test::project_key_of(own));
        CHECK(revoked.root()["state"].as_string() == "untrusted");
        CHECK_FALSE(revoked.root()["trusted"].as_bool(true));
        // The project that is open stays open and usable.
        CHECK(editor.ok("project.info").root()["name"].as_string() == "editor_test");
        (void) editor.create_entity("still editing");
        CHECK(editor.ok("project.trust_state").root()["state"].as_string() == "untrusted");

        // Away to another project and back: now it is asked about.
        (void) editor.ok("project.open", path_args(second));
        editor.pump(1);
        REQUIRE(editor.ok("project.info").root()["name"].as_string() == "Second");
        CHECK(editor.fail("project.open", path_args(own)) == protocol_error_code::PROJECT_NOT_TRUSTED);
        // A folder that is gone can still be forgotten.
        std::error_code ignored = {};
        const std::filesystem::path gone = foreign_project(editor, "gone");
        (void) editor.ok("project.trust", path_args(gone));
        REQUIRE(std::filesystem::remove_all(gone, ignored) > 0);
        CHECK(editor.ok("project.revoke_trust", path_args(gone)).root()["revoked"].as_bool());
    }

    TEST_CASE("project.revoke_trust - a trust store that cannot be rewritten answers IO_ERROR and the project stays "
              "trusted") {
        editor_test::headless_editor editor = {};
        const std::filesystem::path other = foreign_project(editor, "downloaded");
        const std::filesystem::path second = foreign_project(editor, "second");
        const std::filesystem::path store = editor.project_directory() / "state" / "trusted_projects.json";
        if (!use_trust_store(editor, store)) {
            return;
        }
        CHECK(editor.ok("project.trust", path_args(other)).root()["state"].as_string() == "remembered");
        const editor_test::replace_blocker blocker(store);
        if (!blocker.active()) {
            return;
        }
        const protocol_response refused = editor.send("project.revoke_trust", path_args(other));
        REQUIRE_FALSE(refused.ok);
        CHECK(refused.errorCode == protocol_error_code::IO_ERROR);
        CHECK(refused.errorMessage.find("still trusted") != std::string::npos);
        CHECK(editor.ok("project.trust_state", path_args(other)).root()["state"].as_string() == "remembered");
        // An answer the store cannot take is still an answer: for the session, and the result says so.
        const core::json::document trusted = editor.ok("project.trust", path_args(second));
        CHECK(trusted.root()["state"].as_string() == "session");
        CHECK_FALSE(trusted.root()["remembered"].as_bool(true));
        CHECK(editor.log().contains(core::log_level::WARN, "cannot be remembered"));
    }

    TEST_CASE("project.trust_state - reports the open project or the one named and changes nothing") {
        editor_test::headless_editor editor = {};
        const std::filesystem::path other = foreign_project(editor, "downloaded");
        // The fixture trusted its own project for the session, as --trust-project does.
        const core::json::document own = editor.ok("project.trust_state");
        CHECK(own.root()["path"].as_string() == editor_test::project_key_of(editor.project_directory()));
        CHECK(own.root()["state"].as_string() == "session");
        CHECK(own.root()["trusted"].as_bool());
        const core::json::document named = editor.ok("project.trust_state", path_args(other));
        CHECK(named.root()["path"].as_string() == editor_test::project_key_of(other));
        CHECK(named.root()["state"].as_string() == "untrusted");
        CHECK_FALSE(named.root()["trusted"].as_bool(true));
        // A path that names nothing is simply untrusted: the command reads no folder.
        CHECK(editor.ok(
          "project.trust_state",
          path_args(editor.project_directory() / "nowhere")
        ).root()["state"].as_string() == "untrusted");
        // Asking is not answering.
        CHECK(editor.application().trust().state_of(other) == trust_state::UNTRUSTED);
        CHECK_FALSE(editor.context().trust_prompt().has_value());
    }

    TEST_CASE("project.trust_state - without an open project the request must name one") {
        editor_test::headless_editor_options options = {};
        options.withProject = false;
        options.withScene = false;
        editor_test::headless_editor editor(options);
        CHECK(editor.fail("project.trust_state") == protocol_error_code::INVALID_ARGS);
        const std::filesystem::path other = foreign_project(editor, "downloaded");
        CHECK(editor.ok("project.trust_state", path_args(other)).root()["state"].as_string() == "untrusted");
        // The trust commands need no open project: this is how a session without one gets to open its first.
        CHECK(editor.ok("project.trust", path_args(other)).root()["state"].as_string() == "session");
        (void) editor.ok("project.open", path_args(other));
        editor.pump(1);
        CHECK(editor.ok("project.trust_state").root()["state"].as_string() == "session");
    }

    TEST_CASE("regression - code sweep F187 project.create trusts a project in a folder it made and no project made "
              "in a folder that was already there") {
        editor_test::headless_editor editor = {};
        const std::filesystem::path root = editor.project_directory();
        std::error_code ignored = {};
        std::filesystem::path target = {};
        bool expectTrusted = false;
        SUBCASE("a folder that does not exist") {
            target = root / "made" / "deeper";
            expectTrusted = true;
        }
        SUBCASE("an empty folder") {
            // Whoever made it, or may write it, could add a module.json once the project is trusted.
            target = root / "empty_folder";
            REQUIRE(std::filesystem::create_directories(target, ignored));
        }
        SUBCASE("a folder that already holds a native module") {
            target = root / "seeded";
            REQUIRE(std::filesystem::create_directories(target, ignored));
            std::ofstream planted(target / "module.json", std::ios::binary | std::ios::trunc);
            REQUIRE(planted.is_open());
            planted << "{}";
        }
        SUBCASE("a folder that already holds an empty folder") {
            target = root / "almost_empty";
            REQUIRE(std::filesystem::create_directories(target / "scripts", ignored));
        }
        const core::json::document created = editor.ok(
          "project.create",
          core::json::object({{"path", target.generic_string()}, {"name", "Made"}})
        );
        CHECK(created.root()["trust"].as_string() == (expectTrusted ? "session" : "untrusted"));
        CHECK(editor.application().trust().is_trusted(target) == expectTrusted);
        const protocol_response opened = editor.send("project.open", path_args(target));
        CHECK(opened.ok == expectTrusted);
        if (!expectTrusted) {
            CHECK(opened.errorCode == protocol_error_code::PROJECT_NOT_TRUSTED);
        }
    }

    TEST_CASE("project.create - a new project is remembered in the trust store as created by the editor") {
        editor_test::headless_editor editor = {};
        const std::filesystem::path store = editor.project_directory() / "trusted_projects.json";
        if (!use_trust_store(editor, store)) {
            return;
        }
        const std::filesystem::path target = editor.project_directory() / "made";
        const core::json::document created = editor.ok(
          "project.create",
          core::json::object({{"path", target.generic_string()}, {"name", "Made"}})
        );
        CHECK(created.root()["trust"].as_string() == "remembered");
        const std::vector<trusted_project> remembered = editor.application().trust().remembered();
        REQUIRE(remembered.size() == 1);
        CHECK(remembered.front().path == editor_test::project_key_of(target));
        CHECK(remembered.front().origin == "created");
    }

    TEST_CASE("project.trust - the trust commands cannot run inside a batch, which could not take them back") {
        editor_test::headless_editor editor = {};
        const std::filesystem::path other = foreign_project(editor, "downloaded");
        for (const std::string_view command : {"project.trust", "project.revoke_trust"}) {
            CAPTURE(command);
            const core::json::document batch = core::json::object({
              {"commands",
               core::json::array({
                 core::json::object({{"cmd", "entity.create"}, {"args", core::json::object({{"name", "a"}})}}),
                 core::json::object({{"cmd", command}, {"args", path_args(other)}}),
               })},
            });
            const protocol_response refused = editor.send("editor.batch", batch);
            REQUIRE_FALSE(refused.ok);
            CHECK(refused.errorCode == protocol_error_code::INVALID_ARGS);
            CHECK(refused.errorMessage.find("cannot run inside a batch") != std::string::npos);
        }
        CHECK(editor.application().trust().state_of(other) == trust_state::UNTRUSTED);
        CHECK(editor.application().trust().state_of(editor.project_directory()) == trust_state::SESSION);
        CHECK(editor.scene().world().entity_count() == 0);
    }

    TEST_CASE("regression - code sweep F187 an inspect-only client may ask whether a project is trusted and may "
              "neither trust one nor take trust back") {
        editor_test::headless_editor editor = {};
        const std::filesystem::path other = foreign_project(editor, "downloaded");
        protocol_server &server = editor.application().server();
        CONJURE_REQUIRE_OK(server.set_token("tok-7f3a9c"));
        CONJURE_REQUIRE_OK(server.set_inspect_token("look-51d0e2"));
        editor_test::loopback_session session(editor);
        (void) session.ok("session.hello", core::json::object({{"token", "look-51d0e2"}}));

        CHECK(session.ok("project.trust_state", path_args(other)).root()["state"].as_string() == "untrusted");
        const protocol_response trust = session.request("project.trust", path_args(other));
        REQUIRE_FALSE(trust.ok);
        CHECK(trust.errorCode == protocol_error_code::UNAUTHORISED);
        const protocol_response revoke = session.request("project.revoke_trust", path_args(editor.project_directory()));
        REQUIRE_FALSE(revoke.ok);
        CHECK(revoke.errorCode == protocol_error_code::UNAUTHORISED);
        const protocol_response open = session.request("project.open", path_args(other));
        REQUIRE_FALSE(open.ok);
        CHECK(open.errorCode == protocol_error_code::UNAUTHORISED);
        CHECK(editor.application().trust().state_of(other) == trust_state::UNTRUSTED);
        CHECK(editor.application().trust().state_of(editor.project_directory()) == trust_state::SESSION);

        // The full-control token is the user: the same connection may answer once it presents it.
        (void) session.ok("session.hello", core::json::object({{"token", "tok-7f3a9c"}}));
        CHECK(session.ok("project.trust", path_args(other)).root()["trusted"].as_bool());
    }

    TEST_CASE("project.trust - a session that keeps no trust answers NOT_AVAILABLE and project.create still works") {
        editor_test::headless_editor editor = {};
        const std::filesystem::path other = foreign_project(editor, "downloaded");
        // A context built over a bare application has no project_trust.
        editor.context().set_trust(nullptr);
        CHECK(editor.fail("project.trust", path_args(other)) == protocol_error_code::NOT_AVAILABLE);
        CHECK(editor.fail("project.revoke_trust", path_args(other)) == protocol_error_code::NOT_AVAILABLE);
        CHECK(editor.fail("project.trust_state", path_args(other)) == protocol_error_code::NOT_AVAILABLE);
        const core::json::document created = editor.ok(
          "project.create",
          core::json::object({{"path", (editor.project_directory() / "made").generic_string()}, {"name", "Made"}})
        );
        CHECK(created.root()["trust"].as_string() == "untrusted");
        editor.context().set_trust(&editor.application().trust());
        CHECK(editor.application().trust().state_of(other) == trust_state::UNTRUSTED);
    }

    TEST_CASE("regression - code sweep F187 project.open in a window asks the person at it, and project.trust from "
              "anybody settles the question") {
        editor_test::headless_editor editor = {};
        const std::filesystem::path first = foreign_project(editor, "first");
        const std::filesystem::path second = foreign_project(editor, "second");
        // What editor_application turns on for a windowed session; a headless editor is used to run the commands.
        editor.context().set_trust_prompt_enabled(true);

        const protocol_response refused = editor.send("project.open", path_args(first));
        REQUIRE_FALSE(refused.ok);
        CHECK(refused.errorCode == protocol_error_code::PROJECT_NOT_TRUSTED);
        CHECK(refused.errorDetails.root()["prompt"].as_bool());
        REQUIRE(editor.context().trust_prompt().has_value());
        // Compared as project keys: the question names the resolved folder (on macOS the scratch folder lies behind
        // the /var link), and editor.state reports exactly what the window shows.
        CHECK(editor_test::project_key_of(*editor.context().trust_prompt()) == editor_test::project_key_of(first));
        const std::string shown = editor.context().trust_prompt()->generic_string();
        CHECK(editor.ok("editor.state").root()["trustPrompt"].as_string() == shown);
        // Being asked is not an answer.
        CHECK(editor.application().trust().state_of(first) == trust_state::UNTRUSTED);

        SUBCASE("a later refusal replaces the question") {
            CHECK(editor.fail("project.open", path_args(second)) == protocol_error_code::PROJECT_NOT_TRUSTED);
            REQUIRE(editor.context().trust_prompt().has_value());
            CHECK(editor_test::project_key_of(*editor.context().trust_prompt()) == editor_test::project_key_of(second));
        }
        SUBCASE("an answer for another project leaves the question open") {
            (void) editor.ok("project.trust", path_args(second));
            REQUIRE(editor.context().trust_prompt().has_value());
            CHECK(editor_test::project_key_of(*editor.context().trust_prompt()) == editor_test::project_key_of(first));
        }
        SUBCASE("project.trust for the project closes it, under another spelling of the folder as well") {
            (void) editor.ok("project.trust", path_args(first / "assets" / ".."));
            CHECK_FALSE(editor.context().trust_prompt().has_value());
            CHECK(editor.ok("editor.state").root()["trustPrompt"].is_null());
        }
        SUBCASE("revoking and asking about the state do not close it") {
            (void) editor.ok("project.revoke_trust", path_args(first));
            (void) editor.ok("project.trust_state", path_args(first));
            CHECK(editor.context().trust_prompt().has_value());
        }
    }
}

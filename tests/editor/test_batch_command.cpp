#include "conjure/testing/doctest.h"

#include "editor_test_support.h"

#include "conjure/core/json/document.h"
#include "conjure/editor/command_dispatcher.h"
#include "conjure/editor/protocol/protocol_message.h"
#include "conjure/editor/undo_stack.h"

#include <span>
#include <string>

// editor.batch and the transaction commands (docs/design/04_editor.md 3.4, D8).

using namespace conjure;
using namespace conjure::editor;

namespace {
    core::json::document create(std::string_view name) {
        return core::json::object({{"cmd", "entity.create"}, {"args", core::json::object({{"name", name}})}});
    }

    core::json::document rename_missing() {
        return core::json::object({
          {"cmd", "entity.rename"},
          {"args", core::json::object({{"entity", "Nobody/Here"}, {"name", "x"}})},
        });
    }
}

TEST_SUITE("editor/batch_command") {

    TEST_CASE("editor.batch - several mutations become one undo entry") {
        editor_test::headless_editor editor = {};
        const core::json::document result = editor.ok(
          "editor.batch",
          core::json::object({
            {"label", "Three"},
            {"commands", core::json::array({create("a"), create("b"), create("c")})},
          })
        );
        REQUIRE(result.root()["results"].size() == 3);
        CHECK(result.root()["results"][0]["ok"].as_bool());
        CHECK(editor.scene().world().entity_count() == 3);
        REQUIRE(editor.context().undo().size() == 1);
        CHECK(editor.context().undo().entries().front()->label == "Three");
        CHECK(editor.context().undo().entries().front()->undo.size() == 3);
        (void) editor.ok("history.undo");
        CHECK(editor.scene().world().entity_count() == 0);
        (void) editor.ok("history.redo");
        CHECK(editor.scene().world().entity_count() == 3);
    }

    TEST_CASE("editor.batch - an atomic batch rolls back when the third child fails") {
        editor_test::headless_editor editor = {};
        const editor_test::editor_snapshot before = editor_test::capture_snapshot(editor);
        const core::json::document failing = core::json::object({
          {"cmd", "entity.destroy"},
          {"args", core::json::object({{"entity", "Nobody/Here"}})},
        });
        const protocol_response response = editor.send(
          "editor.batch",
          core::json::object({{"commands", core::json::array({create("a"), create("b"), failing})}})
        );
        REQUIRE_FALSE(response.ok);
        CHECK(response.errorDetails.root()["failedIndex"].as_i64() == 2);
        CHECK(editor.scene().world().entity_count() == 0);
        CHECK(editor.context().undo().size() == 0);
        const editor_test::editor_snapshot after = editor_test::capture_snapshot(editor);
        CHECK_MESSAGE(before == after, editor_test::describe_difference(before, after));
    }

    TEST_CASE("editor.batch - a non-atomic batch continues past failures and reports each result") {
        editor_test::headless_editor editor = {};
        const core::json::document failing = core::json::object({
          {"cmd", "entity.destroy"},
          {"args", core::json::object({{"entity", "Nobody/Here"}})},
        });
        const core::json::document result = editor.ok(
          "editor.batch",
          core::json::object({{"atomic", false}, {"commands", core::json::array({create("a"), failing, create("c")})}})
        );
        REQUIRE(result.root()["results"].size() == 3);
        CHECK(result.root()["results"][0]["ok"].as_bool());
        CHECK_FALSE(result.root()["results"][1]["ok"].as_bool());
        CHECK(result.root()["results"][1]["error"]["code"].as_string() == "ENTITY_NOT_FOUND");
        CHECK(result.root()["results"][2]["ok"].as_bool());
        CHECK(editor.scene().world().entity_count() == 2);
        CHECK(editor.context().undo().size() == 1);
    }

    TEST_CASE("editor.batch - asynchronous and non-batchable children are rejected up front") {
        editor_test::headless_editor editor = {};
        const core::json::document wait = core::json::object({
          {"cmd", "editor.wait_frames"},
          {"args", core::json::object({{"frames", 1}})},
        });
        protocol_response response = editor.send(
          "editor.batch",
          core::json::object({{"commands", core::json::array({create("a"), wait})}})
        );
        REQUIRE_FALSE(response.ok);
        CHECK(response.errorCode == protocol_error_code::INVALID_ARGS);
        CHECK(response.errorDetails.root()["failedIndex"].as_i64() == 1);
        CHECK(editor.scene().world().entity_count() == 0);

        const core::json::document nested = core::json::object({
          {"cmd", "editor.batch"},
          {"args", core::json::object({{"commands", core::json::array({create("x")})}})},
        });
        response = editor.send("editor.batch", core::json::object({{"commands", core::json::array({nested})}}));
        REQUIRE_FALSE(response.ok);
        CHECK(response.errorCode == protocol_error_code::INVALID_ARGS);

        const core::json::document unknown = core::json::object({
          {"cmd", "entity.nope"},
          {"args", core::json::document::object()},
        });
        response = editor.send("editor.batch", core::json::object({{"commands", core::json::array({unknown})}}));
        REQUIRE_FALSE(response.ok);
        CHECK(editor.scene().world().entity_count() == 0);
        CHECK(editor.fail(
          "editor.batch",
          core::json::object({{"commands", "nope"}})
        ) == protocol_error_code::INVALID_ARGS);
    }

    TEST_CASE("editor.begin_transaction / end_transaction - group commands of one client into one entry") {
        editor_test::headless_editor editor = {};
        (void) editor.ok("editor.begin_transaction", core::json::object({{"label", "Grouped"}}));
        (void) editor.create_entity("one");
        (void) editor.create_entity("two");
        CHECK(editor.context().undo().size() == 0);
        CHECK(editor.fail("editor.begin_transaction") == protocol_error_code::INVALID_REQUEST);
        (void) editor.ok("editor.end_transaction");
        REQUIRE(editor.context().undo().size() == 1);
        CHECK(editor.context().undo().entries().front()->label == "Grouped");
        CHECK(editor.fail("editor.end_transaction") == protocol_error_code::INVALID_REQUEST);
        (void) editor.ok("history.undo");
        CHECK(editor.scene().world().entity_count() == 0);
    }

    TEST_CASE("regression - F086 an atomic batch rolls back while playing") {
        editor_test::headless_editor editor = {};
        (void) editor.create_entity("keep");
        (void) editor.ok("play.start");
        const usize before = editor.scene().world().entity_count();
        const protocol_response response = editor.send(
          "editor.batch",
          core::json::object({{"commands", core::json::array({create("InBatch"), rename_missing()})}})
        );
        REQUIRE_FALSE(response.ok);
        CHECK(response.errorDetails.root()["failedIndex"].as_i64() == 1);
        CHECK(editor.scene().world().entity_count() == before);
        const core::json::document found = editor.ok("entity.find", core::json::object({{"name", "InBatch"}}));
        CHECK(found.root()["total"].as_u64() == 0);
        (void) editor.ok("play.stop");
        CHECK(editor.context().undo().size() == 1);
    }

    TEST_CASE("regression - F086 an inverse editor.batch that fails during undo rolls back and records nothing") {
        editor_test::headless_editor editor = {};
        (void) editor.create_entity("keep");
        const usize entries = editor.context().undo().size();
        command_inverse inverse = {};
        inverse.name = "editor.batch";
        inverse.args = core::json::object({{"commands", core::json::array({create("Ghost"), rename_missing()})}});
        const core::result<void> undone = editor.context().dispatcher().dispatch_inverses(
          std::span<const command_inverse>(&inverse, 1),
          editor.client_id()
        );
        CHECK_FALSE(undone.has_value());
        const core::json::document found = editor.ok("entity.find", core::json::object({{"name", "Ghost"}}));
        CHECK(found.root()["total"].as_u64() == 0);
        CHECK(editor.scene().world().entity_count() == 1);
        CHECK(editor.context().undo().size() == entries);
    }

    TEST_CASE("regression - F156 history.undo, redo and clear are INVALID_REQUEST while a transaction is open") {
        editor_test::headless_editor editor = {};
        (void) editor.create_entity("a");
        (void) editor.create_entity("b");
        (void) editor.ok("history.undo");
        REQUIRE(editor.context().undo().size() == 2);
        REQUIRE(editor.context().undo().cursor() == 1);
        (void) editor.ok("editor.begin_transaction", core::json::object({{"label", "Open"}}));
        CHECK(editor.fail("history.undo") == protocol_error_code::INVALID_REQUEST);
        CHECK(editor.fail("history.redo") == protocol_error_code::INVALID_REQUEST);
        CHECK(editor.fail("history.clear") == protocol_error_code::INVALID_REQUEST);
        CHECK(editor.context().undo().size() == 2);
        CHECK(editor.context().undo().cursor() == 1);
        (void) editor.ok("editor.end_transaction");
        CHECK(editor.context().undo().size() == 2);
        CHECK(editor.context().undo().cursor() == 1);
        (void) editor.ok("history.redo");
        CHECK(editor.scene().world().entity_count() == 2);
    }

    TEST_CASE("editor.batch - inside an open transaction the batch joins the transaction entry") {
        editor_test::headless_editor editor = {};
        (void) editor.ok("editor.begin_transaction", core::json::object({{"label", "Outer"}}));
        (void) editor.ok(
          "editor.batch",
          core::json::object({{"commands", core::json::array({create("a"), create("b")})}})
        );
        (void) editor.create_entity("c");
        (void) editor.ok("editor.end_transaction");
        REQUIRE(editor.context().undo().size() == 1);
        CHECK(editor.context().undo().entries().front()->undo.size() == 3);
        (void) editor.ok("history.undo");
        CHECK(editor.scene().world().entity_count() == 0);
    }
}

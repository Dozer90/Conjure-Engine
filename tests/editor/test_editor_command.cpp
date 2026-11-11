#include "conjure/testing/doctest.h"

#include "conjure/editor/editor_command.h"

#include <string>

// editor_command.h: the in-process command types, the flag names and async_reply without a dispatcher.

using namespace conjure;
using namespace conjure::editor;

TEST_SUITE("editor/editor_command") {

    TEST_CASE("command_request - defaults to the local client with empty object arguments") {
        const command_request request = {};
        CHECK(request.name.empty());
        CHECK(request.args.root().is_object());
        CHECK(request.clientId == 0);
        CHECK(request.coalesceKey == 0);
    }

    TEST_CASE("command_outcome - carries an optional inverse and replay") {
        command_outcome outcome = {};
        CHECK_FALSE(outcome.inverse.has_value());
        CHECK_FALSE(outcome.replay.has_value());
        outcome.inverse = command_inverse{"entity.destroy", core::json::object({{"entity", "x"}})};
        CHECK(outcome.inverse->name == "entity.destroy");
        CHECK(outcome.inverse->args.root()["entity"].as_string() == "x");
    }

    TEST_CASE("command_flag_names - lists the set flags in declaration order") {
        const core::json::document names = command_flag_names(
          command_flags::MUTATES | command_flags::REQUIRES_SCENE | command_flags::NO_UNDO
        );
        REQUIRE(names.root().size() == 3);
        CHECK(names.root()[0].as_string() == "mutates");
        CHECK(names.root()[1].as_string() == "requires_scene");
        CHECK(names.root()[2].as_string() == "no_undo");
        CHECK(command_flag_names(command_flags::NONE).root().size() == 0);
        CHECK(command_flag_name(command_flags::ASYNC) == "async");
        CHECK(command_flag_name(command_flags::UI_ONLY) == "ui_only");
        CHECK(command_flag_name(command_flags::NONE).empty());
    }

    TEST_CASE("has_command_flag - reads a descriptor's flags") {
        command_descriptor descriptor = {};
        descriptor.flags = command_flags::ASYNC | command_flags::REQUIRES_PROJECT;
        CHECK(has_command_flag(descriptor, command_flags::ASYNC));
        CHECK_FALSE(has_command_flag(descriptor, command_flags::MUTATES));
        CHECK(descriptor.owner == "editor");
        CHECK(descriptor.argsSchema == "{}");
    }

    TEST_CASE("async_reply - a default reply is invalid and completing it is a no-op") {
        const async_reply reply = {};
        CHECK_FALSE(reply.valid());
        CHECK(reply.ticket() == 0);
        reply.succeed(core::json::document::object());
        reply.fail(protocol_error_code::INTERNAL, "ignored");
        CHECK_FALSE(reply.valid());
    }
}

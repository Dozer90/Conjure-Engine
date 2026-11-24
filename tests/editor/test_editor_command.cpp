#include "conjure/testing/doctest.h"

#include "editor_test_support.h"

#include "conjure/editor/command_dispatcher.h"
#include "conjure/editor/command_registry.h"
#include "conjure/editor/editor_command.h"
#include "conjure/editor/editor_context.h"
#include "conjure/editor/protocol/protocol_message.h"

#include <optional>
#include <string>
#include <utility>

// editor_command.h: the in-process command types, the flag names and async_reply, alone and parked by the
// dispatcher of a headless editor.

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

    TEST_CASE("command_flag_names - on_disk is the wire name of ON_DISK and comes last") {
        CHECK(command_flag_name(command_flags::ON_DISK) == "on_disk");
        const core::json::document names = command_flag_names(
          command_flags::ON_DISK | command_flags::MUTATES | command_flags::REQUIRES_PROJECT
        );
        REQUIRE(names.root().size() == 3);
        CHECK(names.root()[0].as_string() == "mutates");
        CHECK(names.root()[1].as_string() == "requires_project");
        CHECK(names.root()[2].as_string() == "on_disk");
        // Two flags are not a single flag.
        CHECK(command_flag_name(command_flags::ON_DISK | command_flags::MUTATES).empty());
    }

    TEST_CASE("command_flag_names - read_only is the wire name of READ_ONLY and comes after on_disk") {
        CHECK(command_flag_name(command_flags::READ_ONLY) == "read_only");
        const core::json::document names = command_flag_names(
          command_flags::READ_ONLY | command_flags::REQUIRES_SCENE | command_flags::ASYNC
        );
        REQUIRE(names.root().size() == 3);
        CHECK(names.root()[0].as_string() == "requires_scene");
        CHECK(names.root()[1].as_string() == "async");
        CHECK(names.root()[2].as_string() == "read_only");
    }

    TEST_CASE("required_access - a read_only command asks for inspect access, every other one for full control") {
        static_assert(required_access(command_flags::READ_ONLY) == access_level::INSPECT);
        static_assert(required_access(command_flags::NONE) == access_level::FULL_CONTROL);
        CHECK(required_access(command_flags::READ_ONLY | command_flags::REQUIRES_SCENE) == access_level::INSPECT);
        CHECK(required_access(command_flags::READ_ONLY | command_flags::ASYNC) == access_level::INSPECT);
        // No other flag opens a command, alone or together: not being a mutation is not being harmless.
        const command_flags others = command_flags::MUTATES | command_flags::REQUIRES_PROJECT |
          command_flags::REQUIRES_SCENE | command_flags::EDITING_ONLY | command_flags::PLAYING_ONLY |
          command_flags::ASYNC | command_flags::UI_ONLY | command_flags::NOT_IN_BATCH | command_flags::NO_UNDO |
          command_flags::ON_DISK | command_flags::SYNC_IN_BATCH;
        CHECK(required_access(others) == access_level::FULL_CONTROL);
        for (u32 bit = 0; bit < 12; ++bit) {
            CAPTURE(bit);
            const command_flags flag = static_cast<command_flags>(1u << bit);
            if (flag == command_flags::READ_ONLY) {
                continue;
            }
            CHECK(required_access(flag) == access_level::FULL_CONTROL);
        }
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
        CHECK_FALSE(reply.is_valid());
        CHECK(reply.ticket() == 0);
        reply.succeed(core::json::document::object());
        reply.fail(protocol_error_code::INTERNAL, "ignored");
        CHECK_FALSE(reply.is_valid());
    }

    TEST_CASE("async_reply - the former name valid() answers as is_valid() for one wave") {
        editor_test::headless_editor editor = {};
        editor_context &context = editor.context();
        std::optional<protocol_response> answered = std::nullopt;
        const u32 client = context.events().register_client({});
        context.register_response_sink(client, [&answered](const protocol_response &response) {
            answered = response;
        });
        async_reply parked = {};
        command_descriptor descriptor = {};
        descriptor.name = "test.park_reply";
        descriptor.flags = command_flags::ASYNC;
        descriptor.handler = [&parked](command_invocation &invocation) -> core::result<command_outcome> {
            parked = invocation.reply;
            return command_outcome{};
        };
        CONJURE_REQUIRE_OK(context.registry().add(std::move(descriptor)));
        protocol_request request = {};
        request.id = make_id(1);
        request.command = "test.park_reply";
        request.clientId = client;
        REQUIRE_FALSE(context.dispatcher().dispatch(request).has_value());
        bool expected = false;

        SUBCASE("a reply to a parked request") {
            expected = true;
        }
        SUBCASE("a reply that was completed") {
            parked.succeed(core::json::document::object());
            editor.pump(1);
            REQUIRE(answered.has_value());
            CHECK(answered->ok);
            expected = false;
        }
        SUBCASE("a default reply") {
            parked = {};
            expected = false;
        }

        CONJURE_TEST_DEPRECATED_BEGIN
        const bool former = parked.valid();
        CONJURE_TEST_DEPRECATED_END
        CHECK(parked.is_valid() == expected);
        CHECK(former == expected);
        context.dispatcher().cancel_client(client);
        context.unregister_response_sink(client);
        context.events().unregister_client(client);
    }

    TEST_CASE("command_flag_names - sync_in_batch is the wire name of SYNC_IN_BATCH and comes last") {
        CHECK(command_flag_name(command_flags::SYNC_IN_BATCH) == "sync_in_batch");
        const core::json::document names = command_flag_names(
          command_flags::SYNC_IN_BATCH | command_flags::ASYNC | command_flags::MUTATES
        );
        REQUIRE(names.root().size() == 3);
        CHECK(names.root()[0].as_string() == "mutates");
        CHECK(names.root()[1].as_string() == "async");
        CHECK(names.root()[2].as_string() == "sync_in_batch");
    }

    TEST_CASE("async_reply - succeed_mutation of a default reply is a no-op") {
        const async_reply reply = {};
        reply.succeed_mutation(command_outcome{});
        CHECK_FALSE(reply.is_valid());
    }
}

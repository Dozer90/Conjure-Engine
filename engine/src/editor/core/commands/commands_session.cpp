#include "editor/core/commands/command_support.h"

#include "conjure/editor/log_category.h"

#include "conjure/editor/event_broadcaster.h"

#include "conjure/core/log.h"
#include "conjure/engine/engine_version.h"

#include <format>

// session.* (docs/design/04_editor.md 4.4): the handshake, event subscriptions and the ping.

namespace conjure::editor::support {

    namespace {
        core::result<command_outcome> session_hello(command_invocation &invocation) {
            editor_context &context = invocation.context;
            const std::string_view token = string_or(invocation.args, "token", "");
            // The server decides, with the comparison its own handshake uses: a token gates the transports that
            // require one (TCP always; stdio and loopback under --ai-token or --ai-inspect-token), and the
            // token presented decides what the client may run from here on (D25). In-process clients (an
            // --exec file, an editor.exec_file task, the UI) have no transport entry and are trusted.
            protocol_server *server = context.server();
            if (server != nullptr && !server->accept_hello(invocation.clientId, token)) {
                return make_editor_error(protocol_error_code::UNAUTHORISED, "session.hello: the token does not match");
            }
            const access_level access =
              server != nullptr ? server->access_of_client(invocation.clientId) : access_level::FULL_CONTROL;
            const std::string_view clientName = string_or(invocation.args, "clientName", "");
            // A client that put its token into the wrong argument must not see it land in the log.
            const bool nameIsSafe = server == nullptr || !server->contains_token(clientName);
            if (!clientName.empty() && nameIsSafe) {
                CONJURE_LOG_INFO(
                  LOG_EDITOR,
                  "client {} says hello as '{}' ({} access)",
                  invocation.clientId,
                  clientName,
                  to_string(access)
                );
            }
            command_outcome outcome = {};
            outcome.result = core::json::object({
              {"protocolVersion", PROTOCOL_VERSION},
              {"engineVersion", engine::engine_version_string()},
              {"schemaHash", std::format("{:016x}", context.registry().schema_hash())},
              {"clientId", invocation.clientId},
              {"headless", context.headless()},
              {"access", to_string(access)},
            });
            return outcome;
        }

        core::result<command_outcome> session_subscribe(command_invocation &invocation) {
            editor_context &context = invocation.context;
            const core::json::value events = invocation.args["events"];
            u64 mask = 0;
            core::json::document subscribed = core::json::document::array();
            for (const core::json::value name : events) {
                const std::optional<editor_event> event = parse_editor_event(name.as_string());
                if (!event.has_value()) {
                    return invalid_args(std::format("args.events: unknown event '{}'", name.as_string()));
                }
                mask |= event_bit(*event);
                subscribed.push_back(name.as_string());
            }
            if (invocation.args.contains("minLogLevel")) {
                // "off" parses, and is rejected on purpose: OFF is not a level a line can have, so it cannot
                // be the minimum of a filter (code sweep F209).
                const std::optional<core::log_level> level =
                  core::parse_log_level(string_or(invocation.args, "minLogLevel", "info"));
                if (!level.has_value() || *level == core::log_level::OFF) {
                    return invalid_args("args.minLogLevel: expected trace, debug, info, warn, error or fatal");
                }
                context.events().set_min_log_level(invocation.clientId, *level);
            }
            context.events().set_subscriptions(invocation.clientId, mask);
            command_outcome outcome = {};
            outcome.result = core::json::object({{"subscribed", subscribed}});
            return outcome;
        }

        core::result<command_outcome> session_ping(command_invocation &invocation) {
            command_outcome outcome = {};
            outcome.result = core::json::object({
              {"frame", invocation.context.frame_index()},
              {"revision", invocation.context.revision()},
            });
            return outcome;
        }
    }

    void register_session_commands(command_registry &registry) {
        add_command(
          registry,
          make_command(
            "session.hello",
            "Handshake: reports the protocol and engine versions, the schema hash, the client id and the access "
            "the token grants. The first request of a TCP client, with a token the editor wrote under "
            "<project>/.conjure: ai_token opens every command (access full_control), ai_token_inspect the "
            "commands flagged read_only (access inspect). A later hello with the other token changes the access.",
            R"({"type":"object","properties":{"token":{"type":"string"},"clientName":{"type":"string"}}})",
            R"({"type":"object","properties":{"protocolVersion":{"type":"integer"},"engineVersion":{"type":"string"},)"
            R"("schemaHash":{"type":"string"},"clientId":{"type":"integer"},"headless":{"type":"boolean"},)"
            R"("access":{"type":"string","enum":["inspect","full_control"]}}})",
            command_flags::READ_ONLY,
            &session_hello
          )
        );
        add_command(
          registry,
          make_command(
            "session.subscribe",
            "Replaces the client's event subscriptions and, optionally, the minimum level of log events.",
            R"({"type":"object","properties":{"events":{"type":"array","items":{"type":"string"}},)"
            R"("minLogLevel":{"type":"string","enum":["trace","debug","info","warn","error","fatal"]}},)"
            R"("required":["events"]})",
            R"({"type":"object","properties":{"subscribed":{"type":"array","items":{"type":"string"}}}})",
            command_flags::READ_ONLY,
            &session_subscribe
          )
        );
        add_command(
          registry,
          make_command(
            "session.ping",
            "The current frame and scene revision.",
            R"({"type":"object"})",
            R"({"type":"object","properties":{"frame":{"type":"integer"},"revision":{"type":"integer"}}})",
            command_flags::READ_ONLY,
            &session_ping
          )
        );
    }
}

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
            if (!context.token().empty() && token != context.token()) {
                return make_editor_error(protocol_error_code::UNAUTHORISED, "session.hello: the token does not match");
            }
            if (context.server() != nullptr) {
                context.server()->mark_authenticated(invocation.clientId);
            }
            const std::string_view clientName = string_or(invocation.args, "clientName", "");
            if (!clientName.empty()) {
                CONJURE_LOG_INFO(LOG_EDITOR, "client {} says hello as '{}'", invocation.clientId, clientName);
            }
            command_outcome outcome = {};
            outcome.result = core::json::object({
              {"protocolVersion", PROTOCOL_VERSION},
              {"engineVersion", engine::engine_version_string()},
              {"schemaHash", std::format("{:016x}", context.registry().schema_hash())},
              {"clientId", invocation.clientId},
              {"headless", context.headless()},
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
                const core::log_level level = core::parse_log_level(string_or(invocation.args, "minLogLevel", "info"));
                if (level == core::log_level::OFF) {
                    return invalid_args("args.minLogLevel: expected trace, debug, info, warn, error or fatal");
                }
                context.events().set_min_log_level(invocation.clientId, level);
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
            "Handshake: reports the protocol and engine versions, the schema hash and the client id.",
            R"({"type":"object","properties":{"token":{"type":"string"},"clientName":{"type":"string"}}})",
            R"({"type":"object","properties":{"protocolVersion":{"type":"integer"},"engineVersion":{"type":"string"},)"
            R"("schemaHash":{"type":"string"},"clientId":{"type":"integer"},"headless":{"type":"boolean"}}})",
            command_flags::NONE,
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
            command_flags::NONE,
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
            command_flags::NONE,
            &session_ping
          )
        );
    }
}

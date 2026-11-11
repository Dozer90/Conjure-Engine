#include "conjure/editor/protocol/protocol_message.h"

#include "conjure/core/json/writer.h"

#include <format>
#include <utility>

namespace conjure::editor {

    namespace {
        using core::json::value;

        [[nodiscard]] core::error protocol_failure(protocol_error_code code, std::string message) {
            return make_editor_error(code, std::move(message));
        }

        [[nodiscard]] bool is_valid_id(const value &id) noexcept {
            return !id.is_valid() || id.is_null() || id.is_integer() || id.is_string();
        }

        [[nodiscard]] core::result<core::json::document> parse_line(std::string_view line) {
            if (line.size() > MAX_MESSAGE_BYTES) {
                return protocol_failure(
                  protocol_error_code::MESSAGE_TOO_LARGE,
                  std::format("line of {} bytes exceeds the limit of {}", line.size(), MAX_MESSAGE_BYTES)
                );
            }
            core::result<core::json::document> parsed = core::json::document::parse(line);
            if (!parsed.has_value()) {
                return protocol_failure(protocol_error_code::PARSE_ERROR, parsed.get_error().message());
            }
            if (!parsed.value().root().is_object()) {
                return protocol_failure(protocol_error_code::INVALID_REQUEST, "a message is a JSON object");
            }
            return parsed;
        }

        void write_id(core::json::writer &out, const core::json::document &id) {
            const value root = id.root();
            if (!root.is_valid() || root.is_null()) {
                out.value(nullptr);
            } else if (root.is_string()) {
                out.value(root.as_string());
            } else if (root.is_integer()) {
                if (root.as_i64(0) < 0) {
                    out.value(root.as_i64());
                } else {
                    out.value(root.as_u64());
                }
            } else {
                out.write_raw_json(core::json::document::from_value(root).to_string());
            }
        }
    }

    core::result<protocol_request> parse_request(std::string_view line) {
        CONJURE_TRY_ASSIGN(const core::json::document parsed, parse_line(line));
        const value root = parsed.root();
        const value command = root["cmd"];
        if (!command.is_string() || command.as_string().empty()) {
            return protocol_failure(protocol_error_code::INVALID_REQUEST, "\"cmd\" must be a non-empty string");
        }
        const value args = root["args"];
        if (args.is_valid() && !args.is_object()) {
            return protocol_failure(protocol_error_code::INVALID_REQUEST, "\"args\" must be an object");
        }
        const value id = root["id"];
        if (!is_valid_id(id)) {
            return protocol_failure(
              protocol_error_code::INVALID_REQUEST,
              "\"id\" must be an integer, a string or null"
            );
        }
        const value coalesce = root["coalesceKey"];
        if (coalesce.is_valid() && (!coalesce.is_integer() || coalesce.as_i64(-1) < 0)) {
            return protocol_failure(
              protocol_error_code::INVALID_REQUEST,
              "\"coalesceKey\" must be an unsigned integer"
            );
        }

        protocol_request request = {};
        request.command = std::string(command.as_string());
        request.id = id.is_valid() ? core::json::document::from_value(id) : null_id();
        request.args = args.is_valid() ? core::json::document::from_value(args) : core::json::document::object();
        request.coalesceKey = coalesce.is_valid() ? coalesce.as_u64() : 0;
        return request;
    }

    core::result<protocol_response> parse_response(std::string_view line) {
        CONJURE_TRY_ASSIGN(const core::json::document parsed, parse_line(line));
        const value root = parsed.root();
        const value ok = root["ok"];
        if (!ok.is_bool()) {
            return protocol_failure(protocol_error_code::INVALID_REQUEST, "a response carries a boolean \"ok\"");
        }
        protocol_response response = {};
        response.ok = ok.as_bool();
        const value id = root["id"];
        response.id = id.is_valid() ? core::json::document::from_value(id) : null_id();
        if (response.ok) {
            const value result = root["result"];
            response.result = result.is_valid() ? core::json::document::from_value(
              result
            ) : core::json::document::object();
        } else {
            const value failure = root["error"];
            if (!failure.is_object()) {
                return protocol_failure(
                  protocol_error_code::INVALID_REQUEST,
                  "a failed response carries an \"error\" object"
                );
            }
            const std::optional<protocol_error_code> code = parse_error_code(failure["code"].as_string());
            response.errorCode = code.value_or(protocol_error_code::INTERNAL);
            response.errorMessage = std::string(failure["message"].as_string());
            const value details = failure["details"];
            response.errorDetails =
              details.is_object() ? core::json::document::from_value(details) : core::json::document::object();
        }
        return response;
    }

    core::result<protocol_event> parse_event(std::string_view line) {
        CONJURE_TRY_ASSIGN(const core::json::document parsed, parse_line(line));
        const value root = parsed.root();
        const value name = root["event"];
        if (!name.is_string()) {
            return protocol_failure(protocol_error_code::INVALID_REQUEST, "an event carries a string \"event\"");
        }
        protocol_event event = {};
        event.name = std::string(name.as_string());
        event.seq = root["seq"].as_u64();
        event.frame = root["frame"].as_u64();
        const value data = root["data"];
        event.data = data.is_valid() ? core::json::document::from_value(data) : core::json::document::object();
        return event;
    }

    std::string serialise(const protocol_response &response) {
        core::json::writer out = {};
        out.begin_object();
        out.key("id");
        write_id(out, response.id);
        out.key("ok");
        out.value(response.ok);
        if (response.ok) {
            out.key("result");
            const value result = response.result.root();
            out.write_raw_json(result.is_valid() ? core::json::document::from_value(result).to_string() : "{}");
        } else {
            out.key("error");
            out.begin_object();
            out.key("code");
            out.value(to_string(response.errorCode));
            out.key("message");
            out.value(std::string_view(response.errorMessage));
            out.key("details");
            const value details = response.errorDetails.root();
            out.write_raw_json(details.is_object() ? core::json::document::from_value(details).to_string() : "{}");
            out.end_object();
        }
        out.end_object();
        return out.finish();
    }

    std::string serialise(const protocol_event &event) {
        core::json::writer out = {};
        out.begin_object();
        out.key("event");
        out.value(std::string_view(event.name));
        out.key("seq");
        out.value(event.seq);
        out.key("frame");
        out.value(event.frame);
        out.key("data");
        const value data = event.data.root();
        out.write_raw_json(data.is_valid() ? core::json::document::from_value(data).to_string() : "{}");
        out.end_object();
        return out.finish();
    }

    std::string serialise(const protocol_request &request) {
        core::json::writer out = {};
        out.begin_object();
        out.key("id");
        write_id(out, request.id);
        out.key("cmd");
        out.value(std::string_view(request.command));
        out.key("args");
        const value args = request.args.root();
        out.write_raw_json(args.is_object() ? core::json::document::from_value(args).to_string() : "{}");
        if (request.coalesceKey != 0) {
            out.key("coalesceKey");
            out.value(request.coalesceKey);
        }
        out.end_object();
        return out.finish();
    }

    protocol_response make_ok_response(const value &id, core::json::document result) {
        protocol_response response = {};
        response.ok = true;
        response.id = id.is_valid() ? core::json::document::from_value(id) : null_id();
        response.result = std::move(result);
        return response;
    }

    protocol_response make_error_response(
      const value &id,
      protocol_error_code code,
      std::string message,
      core::json::document details
    ) {
        protocol_response response = {};
        response.ok = false;
        response.id = id.is_valid() ? core::json::document::from_value(id) : null_id();
        response.errorCode = code;
        response.errorMessage = std::move(message);
        response.errorDetails = std::move(details);
        return response;
    }

    core::json::document null_id() {
        core::result<core::json::document> parsed = core::json::document::parse("null");
        if (parsed.has_value()) {
            return std::move(parsed).value();
        }
        return core::json::document();
    }

    core::json::document make_id(i64 value) {
        core::result<core::json::document> parsed = core::json::document::parse(std::format("{}", value));
        if (parsed.has_value()) {
            return std::move(parsed).value();
        }
        return null_id();
    }

    core::json::document make_id(std::string_view value) {
        core::json::writer out = {};
        out.value(value);
        core::result<core::json::document> parsed = core::json::document::parse(out.finish());
        if (parsed.has_value()) {
            return std::move(parsed).value();
        }
        return null_id();
    }
}

#include "conjure/editor/headless_runner.h"

#include "conjure/editor/editor_application.h"
#include "conjure/editor/editor_error.h"
#include "conjure/editor/log_category.h"

#include "conjure/core/string_utils.h"

#include <format>
#include <fstream>
#include <iostream>
#include <sstream>
#include <utility>

namespace conjure::editor {

    namespace {
        void expand_value(
          core::json::document &document,
          const std::string &pointer,
          const core::json::value &node,
          std::string_view root
        ) {
            if (node.is_string()) {
                const std::string_view text = node.as_string();
                if (text.find(headless_runner::PROJECT_PLACEHOLDER) != std::string_view::npos) {
                    document.set_pointer(
                      pointer,
                      core::str::replace_all(text, headless_runner::PROJECT_PLACEHOLDER, root)
                    );
                }
                return;
            }
            if (node.is_object()) {
                for (const core::json::member entry : node.members()) {
                    const std::string key = core::str::replace_all(
                      core::str::replace_all(entry.key, "~", "~0"),
                      "/",
                      "~1"
                    );
                    expand_value(document, pointer + "/" + key, entry.item, root);
                }
                return;
            }
            if (node.is_array()) {
                usize index = 0;
                for (const core::json::value element : node) {
                    expand_value(document, std::format("{}/{}", pointer, index), element, root);
                    ++index;
                }
            }
        }
    }

    core::json::document exec_summary::to_json() const {
        return core::json::object({
          {"executed", executed},
          {"succeeded", succeeded},
          {"failed", failed},
          {"firstFailedIndex", firstFailedIndex},
          {"exitCode", exitCode},
        });
    }

    headless_runner::headless_runner() = default;
    headless_runner::~headless_runner() = default;

    core::result<exec_summary> headless_runner::run(editor_application &application, const editor_options &options) {
        if (options.execFile.empty()) {
            return make_editor_error(protocol_error_code::INVALID_ARGS, "--exec needs a command file");
        }
        std::ifstream input(options.execFile, std::ios::binary);
        if (!input.is_open()) {
            return make_editor_error(
              protocol_error_code::INVALID_ARGS,
              std::format("--exec: cannot read '{}'", options.execFile.generic_string())
            );
        }
        std::stringstream buffer = {};
        buffer << input.rdbuf();
        CONJURE_TRY_ASSIGN(std::vector<protocol_request> requests, parse_command_file(buffer.str()));

        std::ofstream file = {};
        std::ostream *out = &std::cout;
        if (!options.outputFile.empty()) {
            std::error_code ignored = {};
            if (options.outputFile.has_parent_path()) {
                std::filesystem::create_directories(options.outputFile.parent_path(), ignored);
            }
            file.open(options.outputFile, std::ios::binary | std::ios::trunc);
            if (!file.is_open()) {
                return make_editor_error(
                  protocol_error_code::IO_ERROR,
                  std::format("--out: cannot write '{}'", options.outputFile.generic_string())
                );
            }
            out = &file;
        }
        return run_requests(application, requests, *out, options.keepGoing);
    }

    exec_summary headless_runner::run_requests(
      editor_application &application,
      std::span<const protocol_request> requests,
      std::ostream &out,
      bool keepGoing
    ) {
        exec_summary summary = {};
        mFramesPumped = 0;
        editor_context &context = application.context();
        const std::filesystem::path projectRoot =
          context.project() != nullptr ? context.project()->root_directory() : std::filesystem::path();

        const u32 clientId = context.events().register_client([&out](const protocol_event &event) {
            out << serialise(event) << '\n';
        });
        std::optional<protocol_response> captured = std::nullopt;
        context.register_response_sink(clientId, [&captured](const protocol_response &response) {
            captured = response;
        });

        for (usize index = 0; index < requests.size(); ++index) {
            protocol_request request = {};
            request.id = requests[index].id;
            request.command = requests[index].command;
            request.args = requests[index].args;
            request.coalesceKey = requests[index].coalesceKey;
            request.clientId = clientId;
            if (!projectRoot.empty()) {
                expand_placeholders(request, projectRoot);
            }
            captured.reset();
            std::optional<protocol_response> response = context.dispatcher().dispatch(request);
            if (!response.has_value()) {
                for (u32 frame = 0; frame < MAX_FRAMES_PER_REQUEST && !captured.has_value(); ++frame) {
                    const core::result<void> stepped = application.pump_frame();
                    ++mFramesPumped;
                    if (!stepped.has_value()) {
                        captured = make_error_response(
                          request.id.root(),
                          protocol_error_code::INTERNAL,
                          std::format("the engine frame failed: {}", stepped.get_error().to_string())
                        );
                        break;
                    }
                    // The response may have arrived through the sink already; the dispatcher's own timeout
                    // produces a TIMEOUT response otherwise.
                }
                if (!captured.has_value()) {
                    captured = make_error_response(
                      request.id.root(),
                      protocol_error_code::TIMEOUT,
                      std::format("'{}' did not answer within {} frames", request.command, MAX_FRAMES_PER_REQUEST)
                    );
                }
                response = std::move(captured);
            }
            out << serialise(*response) << '\n';
            out.flush();
            ++summary.executed;
            if (response->ok) {
                ++summary.succeeded;
            } else {
                ++summary.failed;
                if (summary.firstFailedIndex < 0) {
                    summary.firstFailedIndex = static_cast<i32>(index);
                }
                CONJURE_LOG_WARN(
                  LOG_EDITOR,
                  "exec: request {} '{}' failed: {} {}",
                  index,
                  request.command,
                  to_string(response->errorCode),
                  response->errorMessage
                );
                if (!keepGoing) {
                    break;
                }
            }
            // Apply a project restart requested by this command before the next one runs.
            if (application.has_pending_project_open()) {
                const core::result<void> stepped = application.pump_frame();
                ++mFramesPumped;
                if (!stepped.has_value()) {
                    CONJURE_LOG_ERROR(LOG_EDITOR, "exec: the engine frame failed: {}", stepped.get_error().to_string());
                    break;
                }
            }
        }

        application.context().unregister_response_sink(clientId);
        application.context().events().unregister_client(clientId);
        summary.exitCode = summary.failed == 0 ? 0 : 1;
        return summary;
    }

    core::result<std::vector<protocol_request>> headless_runner::parse_command_file(std::string_view text) {
        std::vector<protocol_request> requests = {};
        const std::string_view trimmed = core::str::trim(text);
        if (trimmed.empty()) {
            return requests;
        }
        if (trimmed.front() == '[') {
            core::result<core::json::document> parsed = core::json::document::parse(trimmed);
            if (!parsed.has_value()) {
                return make_editor_error(
                  protocol_error_code::INVALID_ARGS,
                  std::format("command file: {}", parsed.get_error().message())
                );
            }
            usize index = 0;
            for (const core::json::value entry : parsed.value().root()) {
                core::result<protocol_request> request = parse_request(
                  core::json::document::from_value(entry).to_string()
                );
                if (!request.has_value()) {
                    return make_editor_error(
                      protocol_error_code::INVALID_ARGS,
                      std::format("command file: entry {}: {}", index, request.get_error().message())
                    );
                }
                if (request.value().id.root().is_null()) {
                    request.value().id = make_id(static_cast<i64>(index));
                }
                requests.push_back(std::move(request).value());
                ++index;
            }
            return requests;
        }
        usize lineNumber = 0;
        usize index = 0;
        for (const std::string_view line : core::str::split_lines(text)) {
            ++lineNumber;
            const std::string_view content = core::str::trim(line);
            if (content.empty()) {
                continue;
            }
            core::result<protocol_request> request = parse_request(content);
            if (!request.has_value()) {
                return make_editor_error(
                  protocol_error_code::INVALID_ARGS,
                  std::format("command file: line {}: {}", lineNumber, request.get_error().message())
                );
            }
            if (request.value().id.root().is_null()) {
                request.value().id = make_id(static_cast<i64>(index));
            }
            requests.push_back(std::move(request).value());
            ++index;
        }
        return requests;
    }

    void headless_runner::expand_placeholders(protocol_request &request, const std::filesystem::path &projectRoot) {
        const std::string root = projectRoot.generic_string();
        core::json::document copy = request.args;
        expand_value(request.args, "", copy.root(), root);
    }
}

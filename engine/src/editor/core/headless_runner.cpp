#include "conjure/editor/headless_runner.h"

#include "conjure/editor/editor_application.h"
#include "conjure/editor/editor_error.h"
#include "conjure/editor/event_broadcaster.h"
#include "conjure/editor/log_category.h"

#include "conjure/core/clock.h"
#include "conjure/core/string_utils.h"
#include "conjure/engine/exit_code.h"
#include "conjure/engine/project.h"

#include <format>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <system_error>
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

    void exec_summary::record(bool ok, u32 index) noexcept {
        ++executed;
        if (ok) {
            ++succeeded;
            return;
        }
        ++failed;
        if (firstFailedIndex < 0) {
            firstFailedIndex = static_cast<i32>(index);
        }
    }

    headless_runner::headless_runner() = default;
    headless_runner::~headless_runner() = default;

    core::result<exec_summary> headless_runner::run(
      editor_application &application,
      const editor_options &options,
      std::span<const protocol_request> requests
    ) {
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
        // The context survives a project restart (start_engine rebinds it), so the reference, the event client
        // and the response sink stay valid across a project.open in the file.
        editor_context &context = application.context();

        const u32 clientId = context.events().register_client([&out](const protocol_event &event) {
            out << serialise(event) << '\n';
        });
        std::optional<protocol_response> captured = std::nullopt;
        context.register_response_sink(clientId, [&captured](const protocol_response &response) {
            captured = response;
        });

        // Decision D22: an editor engine starts over an empty world when the startup scene does not load, which
        // only its log says. The run's reader learns it from the first line, before any request has run.
        if (std::optional<core::json::document> warning = application.startup_scene_warning(); warning.has_value()) {
            (void) context.events().emit_to(clientId, editor_event::WARNING, std::move(*warning));
        }

        bool outputFailed = false;
        for (usize index = 0; index < requests.size(); ++index) {
            protocol_request request = {};
            request.id = requests[index].id;
            request.command = requests[index].command;
            request.args = requests[index].args;
            request.coalesceKey = requests[index].coalesceKey;
            request.clientId = clientId;
            // ${project} is the project open when this request runs: an earlier project.open may have moved it.
            if (const engine::project *project = context.project(); project != nullptr) {
                expand_placeholders(request, project->root_directory());
            }
            captured.reset();
            std::optional<protocol_response> response = context.dispatcher().dispatch(request);
            bool abandoned = false;
            if (!response.has_value()) {
                // An asynchronous reply arrives through the sink some frames later. The dispatcher's own timeout
                // runs on the engine clock (wall time) and answers every pending request with TIMEOUT, so a reply
                // always arrives in a well-behaved editor; this bound is a fallback for a dispatcher that never
                // answers. It is in seconds, not frames: a request that waits on a real process (script.compile
                // runs dotnet) needs real seconds, however fast a headless frame is. engine() is fetched on every
                // frame because a project restart replaces the engine.
                const f64 pumpStart = application.engine().clock().now_seconds();
                const f64 pumpLimit = context.dispatcher().async_timeout_seconds() * 2.0 + 5.0;
                while (!captured.has_value()) {
                    const core::result<void> stepped = application.pump_frame();
                    ++mFramesPumped;
                    if (!stepped.has_value()) {
                        captured = make_error_response(
                          request.id.root(),
                          protocol_error_code::INTERNAL,
                          std::format("the engine frame failed: {}", stepped.get_error().to_string())
                        );
                        abandoned = true;
                        break;
                    }
                    if (application.engine().clock().now_seconds() - pumpStart >= pumpLimit) {
                        break;
                    }
                }
                if (!captured.has_value()) {
                    captured = make_error_response(
                      request.id.root(),
                      protocol_error_code::TIMEOUT,
                      std::format("'{}' did not answer within {} seconds", request.command, pumpLimit)
                    );
                    abandoned = true;
                }
                response = std::move(captured);
            }
            out << serialise(*response) << '\n';
            out.flush();
            if (!out) {
                // The request ran and is counted below, but no later answer could reach the caller.
                outputFailed = true;
                CONJURE_LOG_ERROR(LOG_EDITOR, "exec: the response of request #{} could not be written", index + 1);
            }
            summary.record(response->ok, static_cast<u32>(index));
            if (!response->ok) {
                CONJURE_LOG_WARN(
                  LOG_EDITOR,
                  "exec: request #{} (id {}) '{}' failed: {} {}",
                  index + 1,
                  request.id.to_string(),
                  request.command,
                  to_string(response->errorCode),
                  response->errorMessage
                );
                if (!keepGoing) {
                    break;
                }
            }
            if (outputFailed) {
                break;
            }
            if (abandoned) {
                // The engine is broken, or the request is still pending and its late reply would be taken for the
                // answer to a later one: nothing after it can run reliably.
                break;
            }
            if (application.quit_requested()) {
                // editor.quit (or a quit from another client) ends the run after its reply.
                break;
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
        if (outputFailed) {
            summary.exitCode = static_cast<int>(engine::exit_code::RUNTIME_FAILURE);
        } else {
            summary.exitCode = summary.failed == 0 ? 0 : 1;
        }
        // A non-zero quit code wins: editor.quit's exitCode, or INIT_FAILURE from a project.open whose restart failed
        // for both projects (that quit ends the run through the failed frame, not through the check above).
        // editor.quit's default 0 still reports an earlier failure under --keep-going.
        if (application.quit_requested() && application.exit_code() != 0) {
            summary.exitCode = application.exit_code();
        }
        return summary;
    }

    core::result<std::vector<protocol_request>> headless_runner::load_command_file(const std::filesystem::path &file) {
        if (file.empty()) {
            return make_editor_error(protocol_error_code::INVALID_ARGS, "--exec needs a command file");
        }
        // A directory opens as a stream on some platforms and then reads as an empty file. Only a directory is
        // turned away here: a pipe or a device is as good a source as a regular file.
        std::error_code ignored = {};
        std::ifstream input = {};
        if (!std::filesystem::is_directory(file, ignored)) {
            input.open(file, std::ios::binary);
        }
        if (!input.is_open()) {
            return make_editor_error(
              protocol_error_code::INVALID_ARGS,
              std::format("--exec: cannot read '{}'", file.generic_string())
            );
        }
        std::stringstream buffer = {};
        buffer << input.rdbuf();
        core::result<std::vector<protocol_request>> requests = parse_command_file(buffer.str());
        if (!requests.has_value()) {
            return make_editor_error(
              protocol_error_code::INVALID_ARGS,
              std::format("--exec: cannot parse '{}': {}", file.generic_string(), requests.get_error().message())
            );
        }
        return requests;
    }

    core::result<std::vector<protocol_request>> headless_runner::parse_command_file(std::string_view text) {
        // Windows PowerShell 5.1 writes UTF-8 files with a byte order mark, which the JSON parser refuses.
        constexpr std::string_view UTF8_BOM = "\xEF\xBB\xBF";
        if (core::str::starts_with(text, UTF8_BOM)) {
            text.remove_prefix(UTF8_BOM.size());
        }
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

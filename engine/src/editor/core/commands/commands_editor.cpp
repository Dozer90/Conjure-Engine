#include "editor/core/commands/command_support.h"

#include "conjure/editor/headless_runner.h"

#include "conjure/core/string_utils.h"

#include <format>
#include <fstream>
#include <sstream>
#include <utility>

// editor.* (docs/design/04_editor.md 3.4, 4.4): the catalogue, the state, batches and transactions, frame
// waits, command files and quitting.

namespace conjure::editor::support {

    namespace {
        /// <summary>
        /// editor.wait_frames: answers once the frame index advanced by the count.
        /// </summary>
        class wait_frames_task final : public frame_task {
            private:
                async_reply mReply = {};
                u64 mTargetFrame = 0;

            public:
                wait_frames_task(async_reply reply, u64 targetFrame) :
                    mReply(reply),
                    mTargetFrame(targetFrame) {}

                ~wait_frames_task() override {
                    if (mReply.valid()) {
                        mReply.fail(protocol_error_code::CANCELLED, "editor.wait_frames was dropped before it ended");
                    }
                }

            public:
                bool tick(editor_context &context) override {
                    if (context.frame_index() < mTargetFrame) {
                        return false;
                    }
                    if (mReply.valid()) {
                        mReply.succeed(core::json::object({{"frame", context.frame_index()}}));
                    }
                    return true;
                }
        };

        /// <summary>
        /// editor.exec_file: dispatches one request per tick (waiting for asynchronous ones) and answers with
        /// the summary. The file's requests run as a private in-process client whose response sink lives
        /// exactly as long as the task: destroyed early (the engine restarted under project.open, shutdown),
        /// the task unregisters the sink and fails its reply with CANCELLED.
        /// </summary>
        class exec_file_task final : public frame_task {
            private:
                std::vector<protocol_request> mRequests = {};
                async_reply mReply = {};
                std::optional<protocol_response> mCaptured = std::nullopt;
                exec_summary mSummary = {};
                editor_context *mContext = nullptr;         // set with the sink; the context outlives its tasks
                usize mNext = 0;
                u32 mClientId = 0;
                u32 mWaitingFrames = 0;
                bool mKeepGoing = false;
                bool mWaiting = false;
                bool mSinkInstalled = false;

            public:
                exec_file_task(
                  std::vector<protocol_request> requests,
                  async_reply reply,
                  u32 clientId,
                  bool keepGoing
                ) :
                    mRequests(std::move(requests)),
                    mReply(reply),
                    mClientId(clientId),
                    mKeepGoing(keepGoing) {}

                ~exec_file_task() override {
                    release_client();
                    if (mReply.valid()) {
                        mReply.fail(protocol_error_code::CANCELLED, "editor.exec_file was dropped before it finished");
                    }
                }

            public:
                bool tick(editor_context &context) override {
                    if (!mSinkInstalled) {
                        // The exec client is a fresh in-process client so its asynchronous replies can be
                        // observed here without touching the issuing client's stream.
                        mContext = &context;
                        mClientId = context.events().register_client({});
                        context.register_response_sink(mClientId, [this](const protocol_response &response) {
                            mCaptured = response;
                        });
                        mSinkInstalled = true;
                    }
                    for (u32 budget = 0; budget < 64; ++budget) {
                        if (mWaiting) {
                            if (!mCaptured.has_value()) {
                                ++mWaitingFrames;
                                return false;
                            }
                            record(*mCaptured);
                            mCaptured.reset();
                            mWaiting = false;
                            if (mSummary.failed > 0 && !mKeepGoing) {
                                return finish(context);
                            }
                        }
                        if (mNext >= mRequests.size()) {
                            return finish(context);
                        }
                        protocol_request request = mRequests[mNext];
                        ++mNext;
                        request.clientId = mClientId;
                        if (context.project() != nullptr) {
                            headless_runner::expand_placeholders(request, context.project()->root_directory());
                        }
                        mCaptured.reset();
                        const std::optional<protocol_response> response = context.dispatcher().dispatch(request);
                        if (!response.has_value()) {
                            mWaiting = true;
                            return false;
                        }
                        record(*response);
                        if (mSummary.failed > 0 && !mKeepGoing) {
                            return finish(context);
                        }
                    }
                    return false;
                }

            private:
                void record(const protocol_response &response) {
                    ++mSummary.executed;
                    if (response.ok) {
                        ++mSummary.succeeded;
                    } else {
                        ++mSummary.failed;
                        if (mSummary.firstFailedIndex < 0) {
                            mSummary.firstFailedIndex = static_cast<i32>(mSummary.executed - 1);
                        }
                    }
                }

                bool finish(editor_context &context) {
                    (void) context;
                    mSummary.exitCode = mSummary.failed == 0 ? 0 : 1;
                    release_client();
                    if (mReply.valid()) {
                        mReply.succeed(mSummary.to_json());
                    }
                    return true;
                }

                void release_client() {
                    if (!mSinkInstalled || mContext == nullptr) {
                        return;
                    }
                    mContext->unregister_response_sink(mClientId);
                    mContext->events().unregister_client(mClientId);
                    mSinkInstalled = false;
                }
        };

        core::json::document response_to_json(const protocol_response &response) {
            core::result<core::json::document> parsed = core::json::document::parse(serialise(response));
            if (parsed.has_value()) {
                return std::move(parsed).value();
            }
            return core::json::object({{"ok", response.ok}});
        }

        core::result<command_outcome> editor_schema(command_invocation &invocation) {
            editor_context &context = invocation.context;
            command_outcome outcome = {};
            if (invocation.args.contains("command")) {
                const std::string_view name = string_or(invocation.args, "command", "");
                const command_descriptor *descriptor = context.registry().find(name);
                if (descriptor == nullptr) {
                    return make_editor_error(
                      protocol_error_code::UNKNOWN_COMMAND,
                      std::format("unknown command '{}'", name)
                    );
                }
                outcome.result = context.registry().describe(*descriptor);
                return outcome;
            }
            outcome.result = context.registry().describe_all(false);
            return outcome;
        }

        core::result<command_outcome> editor_state(command_invocation &invocation) {
            command_outcome outcome = {};
            outcome.result = invocation.context.describe_state();
            return outcome;
        }

        core::result<command_outcome> editor_batch(command_invocation &invocation) {
            editor_context &context = invocation.context;
            const std::string_view label = string_or(invocation.args, "label", "Batch");
            const bool atomic = bool_or(invocation.args, "atomic", true);
            core::result<std::vector<protocol_response>> responses = context.dispatcher().run_batch(
              invocation.args["commands"],
              label,
              atomic,
              invocation.clientId,
              invocation.errorDetails
            );
            if (!responses.has_value()) {
                return core::error(responses.get_error());
            }
            core::json::document results = core::json::document::array();
            for (const protocol_response &response : responses.value()) {
                results.push_back(response_to_json(response));
            }
            command_outcome outcome = {};
            outcome.label = std::string(label);
            outcome.result = core::json::object({{"results", results}, {"count", responses.value().size()}});
            return outcome;
        }

        core::result<command_outcome> editor_begin_transaction(command_invocation &invocation) {
            const std::string_view label = string_or(invocation.args, "label", "Transaction");
            CONJURE_TRY(invocation.context.undo().begin_transaction(label, invocation.clientId));
            command_outcome outcome = {};
            outcome.result = core::json::object({{"label", label}});
            return outcome;
        }

        core::result<command_outcome> editor_end_transaction(command_invocation &invocation) {
            CONJURE_TRY(invocation.context.undo().end_transaction(invocation.clientId));
            command_outcome outcome = {};
            outcome.result = core::json::object({{"entries", invocation.context.undo().size()}});
            return outcome;
        }

        core::result<command_outcome> editor_wait_frames(command_invocation &invocation) {
            CONJURE_TRY_ASSIGN(const u32 frames, u32_or(invocation.args, "frames", 1));
            if (frames == 0) {
                return invalid_args("args.frames: expected at least 1");
            }
            const u64 target = invocation.context.frame_index() + frames;
            invocation.context.add_frame_task(std::make_unique<wait_frames_task>(invocation.reply, target));
            return command_outcome{};
        }

        core::result<command_outcome> editor_exec_file(command_invocation &invocation) {
            CONJURE_TRY_ASSIGN(const std::string_view pathText, require_string(invocation.args, "path"));
            const bool keepGoing = bool_or(invocation.args, "keepGoing", false);
            std::ifstream input(std::filesystem::path(std::string(pathText)), std::ios::binary);
            if (!input.is_open()) {
                return make_editor_error(protocol_error_code::IO_ERROR, std::format("cannot read '{}'", pathText));
            }
            std::stringstream buffer = {};
            buffer << input.rdbuf();
            CONJURE_TRY_ASSIGN(
              std::vector<protocol_request> requests,
              headless_runner::parse_command_file(buffer.str())
            );
            invocation.context.add_frame_task(
              std::make_unique<exec_file_task>(std::move(requests), invocation.reply, invocation.clientId, keepGoing)
            );
            return command_outcome{};
        }

        core::result<command_outcome> editor_quit(command_invocation &invocation) {
            editor_context &context = invocation.context;
            if (context.host() == nullptr) {
                return not_available("editor.quit: this session has no process host");
            }
            CONJURE_TRY_ASSIGN(const u32 exitCode, u32_or(invocation.args, "exitCode", 0));
            const bool save = bool_or(invocation.args, "saveChanges", false);
            if (save && context.has_scene() && context.scene_dirty() && !context.scene_path().empty()) {
                command_request request = {};
                request.name = "scene.save";
                request.clientId = invocation.clientId;
                CONJURE_TRY(context.dispatcher().execute(request));
            }
            context.host()->request_quit(static_cast<int>(exitCode));
            command_outcome outcome = {};
            outcome.result = core::json::object({{"exitCode", exitCode}});
            return outcome;
        }
    }

    void register_editor_commands(command_registry &registry) {
        add_command(
          registry,
          make_command(
            "editor.schema",
            "The whole command catalogue with argument and result schemas, or one command's descriptor.",
            R"({"type":"object","properties":{"command":{"type":"string"}}})",
            ANY_OBJECT_SCHEMA.data(),
            command_flags::NONE,
            &editor_schema
          )
        );
        add_command(
          registry,
          make_command(
            "editor.state",
            "Project, scene, play state, revision, selection and transport facts.",
            R"({"type":"object"})",
            ANY_OBJECT_SCHEMA.data(),
            command_flags::NONE,
            &editor_state
          )
        );
        add_command(
          registry,
          make_command(
            "editor.batch",
            "Runs several commands as one undo entry; atomic batches roll back on the first failure.",
            R"({"type":"object","properties":{"commands":{"type":"array","items":{"type":"object","properties":)"
            R"({"cmd":{"type":"string"},"args":{"type":"object","additionalProperties":true}},"required":["cmd"]}},)"
            R"("label":{"type":"string"},"atomic":{"type":"boolean"}},"required":["commands"]})",
            R"({"type":"object","properties":{"results":{"type":"array"},"count":{"type":"integer"},)"
            R"("revision":{"type":"integer"}}})",
            command_flags::MUTATES | command_flags::NO_UNDO | command_flags::NOT_IN_BATCH,
            &editor_batch
          )
        );
        add_command(
          registry,
          make_command(
            "editor.begin_transaction",
            "Opens an undo transaction for the client; mutating commands until end_transaction form one entry.",
            R"({"type":"object","properties":{"label":{"type":"string"}}})",
            R"({"type":"object","properties":{"label":{"type":"string"}}})",
            command_flags::NOT_IN_BATCH,
            &editor_begin_transaction
          )
        );
        add_command(
          registry,
          make_command(
            "editor.end_transaction",
            "Closes the client's undo transaction.",
            R"({"type":"object"})",
            R"({"type":"object","properties":{"entries":{"type":"integer"}}})",
            command_flags::NOT_IN_BATCH,
            &editor_end_transaction
          )
        );
        add_command(
          registry,
          make_command(
            "editor.wait_frames",
            "Answers once the engine ran the given number of frames.",
            R"({"type":"object","properties":{"frames":{"type":"integer","minimum":1}}})",
            R"({"type":"object","properties":{"frame":{"type":"integer"}}})",
            command_flags::ASYNC,
            &editor_wait_frames
          )
        );
        add_command(
          registry,
          make_command(
            "editor.exec_file",
            "Runs a command file (JSON array or NDJSON) and answers with the execution summary.",
            R"({"type":"object","properties":{"path":{"type":"string"},"keepGoing":{"type":"boolean"}},)"
            R"("required":["path"]})",
            R"({"type":"object","properties":{"executed":{"type":"integer"},"succeeded":{"type":"integer"},)"
            R"("failed":{"type":"integer"},"firstFailedIndex":{"type":"integer"},"exitCode":{"type":"integer"}}})",
            command_flags::ASYNC,
            &editor_exec_file
          )
        );
        add_command(
          registry,
          make_command(
            "editor.quit",
            "Stops the editor process after the current frame, saving the scene first when asked.",
            R"({"type":"object","properties":{"exitCode":{"type":"integer","minimum":0},)"
            R"("saveChanges":{"type":"boolean"}}})",
            R"({"type":"object","properties":{"exitCode":{"type":"integer"}}})",
            command_flags::NOT_IN_BATCH,
            &editor_quit
          )
        );
    }
}

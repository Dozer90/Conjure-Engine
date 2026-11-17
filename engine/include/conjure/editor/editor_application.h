#pragma once

#include "conjure/editor/editor_context.h"
#include "conjure/editor/editor_options.h"
#include "conjure/editor/project_trust.h"
#include "conjure/editor/protocol/protocol_message.h"
#include "conjure/editor/protocol/protocol_server.h"

#include "conjure/core/api.h"
#include "conjure/core/json/document.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"
#include "conjure/engine/application.h"
#include "conjure/engine/application_desc.h"
#include "conjure/engine/layer.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// The editor process object (docs/design/04_editor.md 3.1, 3.2, 5): owns the engine::application, the
// editor_context and the protocol_server, drives the frame integration headless (through a core layer) or
// with the UI layer the executable injects, restarts the engine when project.open names another project and
// runs --export / --exec / the interactive loop. The command file of --exec is read and parsed before the engine
// starts, so a missing or malformed file fails at once (decision D22).
//
// It is also the gate of project trust (decision D27): an engine starts over a project only when the
// application's project_trust says somebody answered for it. The three places a project is opened ask it:
// initialise() for --project, request_open_project() for project.open, and the restart itself, in case the
// answer was revoked between the request and the frame boundary.

namespace conjure::editor {

    /// <summary>
    /// What a session does about the project its options name (decision D27), as
    /// editor_application::resolve_startup_project decides it. At most one of the two paths is set.
    /// </summary>
    struct startup_project {
        std::filesystem::path open = {};            // the project the engine starts over; empty = none
        std::filesystem::path unanswered = {};      // left out because nobody answered for it: the window asks
    };

    /// <summary>
    /// What initialise() starts from (decision D27), as editor_application::plan_startup decides it: the one
    /// engine description, made over the project the trust check let through and no other, and the project
    /// that was left out for the window to ask about (empty when none was).
    /// </summary>
    struct startup_plan {
        engine::application_desc desc = {};         // what the engine is started from
        std::filesystem::path unanswered = {};      // startup_project::unanswered
    };

    class CONJURE_API editor_application final : public editor_host {
        public:
            using layer_factory = std::function<std::unique_ptr<engine::layer>(editor_context &context)>;

        private:
            editor_options mOptions = {};
            layer_factory mUiFactory = {};
            std::unique_ptr<engine::application> mEngine = nullptr;
            std::unique_ptr<editor_context> mContext = nullptr;
            std::unique_ptr<protocol_server> mServer = nullptr;
            project_trust mTrust = {};                              // which projects may be opened (D27)
            std::optional<std::filesystem::path> mPendingProject = std::nullopt;
            std::vector<protocol_request> mExecRequests = {};       // the parsed --exec file, read by initialise()
            int mExitCode = 0;
            u32 mTicks = 0;
            bool mQuitRequested = false;
            bool mInitialised = false;

        public:
            editor_application();
            ~editor_application() override;

        public:
            /// <summary>
            /// Builds the engine over the options (headless: null platform, rhi and audio; the scripting
            /// runtime on the null host so script.* commands work without .NET), initialises it, creates the
            /// context, pushes the frame layer (and the UI layer from the factory when not headless) and
            /// starts the protocol server. Fails with the engine's error or, headless, IO_ERROR when an explicit
            /// port is busy (a windowed editor logs that and runs without the server). Nothing heavy starts
            /// before the cheap checks are through, in this order: the arguments, project.json and the merged
            /// configuration (make_engine_desc), then the command file of --exec when run() will execute it
            /// (headless_runner::load_command_file; a missing or malformed file is the engine's BAD_ARGUMENTS
            /// with the runner's message), then whether the project may be opened, and only then the engine.
            /// conjure_editor's main exits 3 for BAD_ARGUMENTS or an INVALID_CONFIG that a flag caused,
            /// otherwise 2.
            ///
            /// Project trust (decision D27). options.trustStoreFile names the store of remembered answers
            /// (empty: nothing is remembered) and options.trustProject answers for options.projectPath for
            /// this session. A project nobody answered for is not opened. Where nobody can be asked (headless,
            /// --exec, --export) initialise fails with PROJECT_NOT_TRUSTED and no engine starts. A windowed
            /// session that runs neither --exec nor --export starts without the project instead (options()
            /// then names none) and asks: the context's trust_prompt() names the project, a WARN says so, and
            /// project.trust followed by project.open opens it. resolve_startup_project is that decision, and
            /// the engine is started from the description plan_startup makes from its answer alone.
            /// </summary>
            [[nodiscard]] core::result<void> initialise(const editor_options &options, layer_factory uiFactory);

            /// <summary>
            /// --export: runs project.export and returns 0 or 1. --exec: runs the requests initialise() parsed
            /// and returns the headless runner's exit code (2 when --out cannot be written). --export wins when
            /// both are given. Otherwise frames until quit; returns the exit code. A headless --ai-stdio session
            /// without a TCP server quits by itself and returns 0 once standard input ends.
            /// </summary>
            [[nodiscard]] int run();

            /// <summary>
            /// Runs one engine frame (the frame layer ticks the context) and applies a pending project
            /// restart afterwards. Tests and the headless runner pump with it.
            /// </summary>
            [[nodiscard]] core::result<void> pump_frame();

            /// <summary>
            /// The frame body outside a frame (tests): what the frame layer calls once per frame.
            /// </summary>
            void headless_tick();

            /// <summary>
            /// Stops the server and shuts the engine down (idempotent).
            /// </summary>
            void shutdown();

            void request_quit(int exitCode) override;

            /// <summary>
            /// Checks project.json in the directory, then that the project is trusted (PROJECT_NOT_TRUSTED
            /// otherwise, and nothing is scheduled), then schedules the engine restart over it for the next
            /// frame boundary. A project whose trust is revoked before that boundary is not opened: the restart
            /// is dropped with a WARN and the editor stays on the project it has.
            /// </summary>
            [[nodiscard]] core::result<void> request_open_project(const std::filesystem::path &directory) override;
            [[nodiscard]] bool has_pending_project_open() const noexcept override;

            [[nodiscard]] bool quit_requested() const noexcept {
                return mQuitRequested;
            }

            [[nodiscard]] int exit_code() const noexcept {
                return mExitCode;
            }

            [[nodiscard]] editor_context &context() noexcept {
                return *mContext;
            }

            [[nodiscard]] engine::application &engine() noexcept {
                return *mEngine;
            }

            [[nodiscard]] protocol_server &server() noexcept {
                return *mServer;
            }

            [[nodiscard]] const editor_options &options() const noexcept {
                return mOptions;
            }

            /// <summary>
            /// Whether a session with these options has somebody to ask about a project nobody answered for:
            /// a window, and neither --exec nor --export, which run to their end unattended. initialise()
            /// starts such a session without its untrusted project and asks; every other session is refused.
            /// </summary>
            [[nodiscard]] static bool can_ask_about_trust(const editor_options &options) noexcept;

            /// <summary>
            /// What initialise() does about options.projectPath (decision D27), decided before any engine
            /// exists. No project named: nothing to open, nothing to ask. options.trustProject first records
            /// a session answer in 'trust' ("command line"). A project 'trust' holds an answer for is the one
            /// to open. One nobody answered for is left out and named as unanswered where
            /// can_ask_about_trust(options) holds; everywhere else the call fails with PROJECT_NOT_TRUSTED,
            /// and the message names the project and the ways to answer. Fails as project_trust::trust does
            /// when --trust-project names a path that has no key. Whether the folder holds a project is not
            /// looked at here (make_engine_desc does that first).
            /// </summary>
            [[nodiscard]] static core::result<startup_project> resolve_startup_project(
              const editor_options &options,
              project_trust &trust
            );

            /// <summary>
            /// resolve_startup_project, then make_engine_desc over the project it lets through: the description
            /// initialise() starts the engine from. A project left out for the window is in neither the
            /// description nor anything made from it (its project.json, native modules and game assembly are
            /// not loaded); plan.unanswered names it. Fails as either of the two does.
            /// </summary>
            [[nodiscard]] static core::result<startup_plan> plan_startup(
              const editor_options &options,
              project_trust &trust
            );

            /// <summary>
            /// The answers about which projects this application opens (decision D27). Valid before
            /// initialise() and across engine restarts; the context's trust() points at it.
            /// </summary>
            [[nodiscard]] project_trust &trust() noexcept {
                return mTrust;
            }

            [[nodiscard]] bool is_initialised() const noexcept {
                return mInitialised;
            }

            /// <summary>
            /// The data of the `warning` event that reports a startup scene the running engine could not load:
            /// {code: "STARTUP_SCENE_FAILED", message}, the message being the engine's
            /// (engine::application::startup_scene_failure). nullopt when the scene loaded, when the project
            /// names none, or without an engine. The headless runner writes it as the first line of its output;
            /// after a project.open restart the application emits it to every client by itself.
            /// </summary>
            [[nodiscard]] std::optional<core::json::document> startup_scene_warning() const;

            /// <summary>
            /// Frames pumped since initialise().
            /// </summary>
            [[nodiscard]] u32 ticks() const noexcept {
                return mTicks;
            }

            /// <summary>
            /// The engine configuration the options produce for a project directory (empty = none): the
            /// merge of engine_config::defaults() (editor = true), --headless, project.json and the engine
            /// arguments, plus the editor's scripting policy. Fails with BAD_ARGUMENTS naming every option
            /// neither the editor nor the engine knows, before the project is loaded. Exposed for tests.
            /// </summary>
            [[nodiscard]] static core::result<engine::application_desc> make_engine_desc(
              const editor_options &options,
              const std::filesystem::path &projectPath
            );

        private:
            [[nodiscard]] core::result<void> require_trusted(const std::filesystem::path &projectPath) const;
            [[nodiscard]] core::result<void> start_engine(const std::filesystem::path &projectPath);
            [[nodiscard]] core::result<void> boot_engine(engine::application_desc desc);
            [[nodiscard]] core::result<void> apply_pending_project_open();
            [[nodiscard]] int run_export();
            [[nodiscard]] int run_exec();
    };
}

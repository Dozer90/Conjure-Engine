#include "conjure/editor/editor_application.h"

#include "conjure/editor/editor_error.h"
#include "conjure/editor/headless_runner.h"
#include "conjure/editor/log_category.h"

#include "conjure/core/platform_detect.h"
#include "conjure/engine/command_line.h"
#include "conjure/engine/engine_config.h"
#include "conjure/engine/engine_error.h"
#include "conjure/engine/exit_code.h"
#include "conjure/engine/project.h"

#if defined(CONJURE_BUILD_EDITOR) && CONJURE_BUILD_EDITOR
    #include "conjure/engine/imgui_layer.h"
#endif
#if defined(CONJURE_EDITOR_WITH_BINDINGS) && CONJURE_EDITOR_WITH_BINDINGS
    #include "conjure/bindings/bindings_subsystem.h"
#endif

#include <filesystem>
#include <format>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace conjure::editor {

    namespace {
        /// <summary>
        /// The core layer that drives the frame integration (docs 3.2) in every mode; the UI layer only draws.
        /// </summary>
        class editor_tick_layer final : public engine::layer {
            private:
                editor_application *mApplication = nullptr;

            public:
                explicit editor_tick_layer(editor_application &application) :
                    layer("editor-core"),
                    mApplication(&application) {}

                ~editor_tick_layer() override = default;

            public:
                void on_update(engine::engine_context &context) override {
                    (void) context;
                    mApplication->headless_tick();
                }
        };

        /// <summary>
        /// The absolute form of a command-line path, resolved against the working directory, as text for a
        /// request; the path as given when the working directory cannot be read.
        /// </summary>
        [[nodiscard]] std::string working_directory_path(const std::filesystem::path &path) {
            std::error_code failure = {};
            const std::filesystem::path absolute = std::filesystem::absolute(path, failure);
            if (failure) {
                return path.generic_string();
            }
            return absolute.generic_string();
        }

        /// <summary>
        /// The refusal of a project nobody answered for (decision D27): it names the project and the three
        /// explicit ways to answer.
        /// </summary>
        [[nodiscard]] core::error not_trusted(const std::filesystem::path &projectPath) {
            return make_editor_error(
              protocol_error_code::PROJECT_NOT_TRUSTED,
              std::format(
                "the project '{}' is not trusted on this machine. Opening a project runs its scripts and native "
                "modules with your rights, so the editor opens one only after an explicit answer: the "
                "project.trust command, --trust-project for one run, or the question the editor window asks",
                projectPath.generic_string()
              )
            );
        }
    }

    editor_application::editor_application() = default;

    editor_application::~editor_application() {
        shutdown();
    }

    core::result<engine::application_desc> editor_application::make_engine_desc(
      const editor_options &options,
      const std::filesystem::path &projectPath
    ) {
        engine::engine_config base = engine::engine_config::defaults();
        base.editor = true;

        std::vector<std::string> storage = options.engineArguments;
        if (options.headless) {
            storage.emplace_back("--headless");
        }
        const std::string projectText = projectPath.generic_string();
        if (!projectPath.empty()) {
            storage.emplace_back("--project");
            storage.push_back(projectText);
        }
        std::vector<std::string_view> arguments = {};
        arguments.reserve(storage.size());
        for (const std::string &argument : storage) {
            arguments.emplace_back(argument);
        }
        CONJURE_TRY_ASSIGN(const engine::command_line_options parsed, engine::parse_command_line(arguments));
        if (!parsed.unrecognisedArguments.empty()) {
            // The editor took its own flags first, so what the engine does not know either is a mistake. It is
            // refused before the merge loads the project, so a missing project does not hide the bad option.
            std::string unknown = {};
            for (const std::string &argument : parsed.unrecognisedArguments) {
                if (!unknown.empty()) {
                    unknown += ' ';
                }
                unknown += argument;
            }
            return engine::make_error(
              engine::engine_error::BAD_ARGUMENTS,
              std::format("unknown option(s): {}", unknown)
            );
        }
        CONJURE_TRY_ASSIGN(engine::application_desc desc, engine::application_desc::from_options(parsed, base));
        desc.config.editor = true;

        if (options.headless) {
            // Scripting on the null host so script.* commands work without .NET (docs 7); --dotnet keeps the
            // real host for play tests that run C# headless.
            desc.config.scripting.enabled = !options.noScripting;
            desc.config.scripting.useNullHost = !options.dotnetHost;
            desc.config.scripting.required = false;
            if (options.offscreen) {
                engine::engine_config attempt = desc.config;
                attempt.rendering.enabled = true;
                attempt.rendering.backend = engine::rhi_backend_choice::AUTO;
                if (attempt.validate().has_value()) {
                    desc.config = attempt;
                } else {
                    CONJURE_LOG_WARN(
                      LOG_EDITOR,
                      "--offscreen: this engine build refuses a real rhi device without a window; "
                      "rendering runs on the null device and view.screenshot answers NOT_AVAILABLE"
                    );
                    desc.config.rendering.enabled = true;
                    desc.config.rendering.backend = engine::rhi_backend_choice::NULL_DEVICE;
                }
            }
        } else {
            desc.config.scripting.required = false;
            if (options.noScripting) {
                desc.config.scripting.enabled = false;
            }
        }
        CONJURE_TRY(desc.config.validate());
        return desc;
    }

    core::result<void> editor_application::initialise(const editor_options &options, layer_factory uiFactory) {
        if (mInitialised) {
            return make_editor_error(protocol_error_code::INVALID_REQUEST, "editor_application is already initialised");
        }
        mOptions = options;
        mUiFactory = std::move(uiFactory);
        mExecRequests.clear();
        // Fail fast (decision D22): everything that can be wrong without an engine is checked before one starts.
        // The arguments and the project come first, so a bad flag or a missing project is still what is reported
        // when the command file is unusable as well. Only checked here: the description the engine starts
        // from is made further down, from the project the trust check lets through.
        CONJURE_TRY(make_engine_desc(options, options.projectPath));
        if (!options.execFile.empty() && options.exportDirectory.empty()) {
            core::result<std::vector<protocol_request>> requests = headless_runner::load_command_file(options.execFile);
            if (!requests.has_value()) {
                // The file is an argument: BAD_ARGUMENTS is what conjure_editor's main turns into exit code 3.
                return engine::make_error(engine::engine_error::BAD_ARGUMENTS, requests.get_error().message());
            }
            mExecRequests = std::move(requests).value();
        }
        // The last check that needs no engine (decision D27): starting the engine over a project loads the
        // project's native modules and its game assembly, so nothing of it may start before somebody answered.
        mTrust.set_store_file(options.trustStoreFile);
        CONJURE_TRY_ASSIGN(startup_plan plan, plan_startup(options, mTrust));
        mOptions.projectPath = plan.desc.project.has_value() ? options.projectPath : std::filesystem::path();
        CONJURE_TRY(boot_engine(std::move(plan.desc)));
        mContext->set_trust(&mTrust);
        mContext->set_trust_prompt_enabled(can_ask_about_trust(options));
        if (mContext->raise_trust_prompt(plan.unanswered)) {
            CONJURE_LOG_WARN(
              LOG_EDITOR,
              "project trust: '{}' was not opened because nobody answered for it on this machine; the editor "
              "window asks, and project.trust followed by project.open opens it",
              plan.unanswered.generic_string()
            );
        }
        mServer = std::make_unique<protocol_server>(mContext->events());
        mContext->set_server(mServer.get());
        mContext->dispatcher().set_async_timeout_seconds(options.timeoutSeconds);
        const std::filesystem::path projectRoot =
          mContext->project() != nullptr ? mContext->project()->root_directory() : std::filesystem::path();
        const core::result<void> started = mServer->start(options, projectRoot);
        if (!started.has_value()) {
            if (options.headless) {
                return started;
            }
            CONJURE_LOG_ERROR(
              LOG_EDITOR,
              "{}; the editor runs without the AI protocol server",
              started.get_error().message()
            );
        }
        mInitialised = true;
        return core::result<void>::success();
    }

    int editor_application::run() {
        if (!mInitialised) {
            return static_cast<int>(engine::exit_code::INIT_FAILURE);
        }
        if (!mOptions.exportDirectory.empty()) {
            return run_export();
        }
        if (!mOptions.execFile.empty()) {
            return run_exec();
        }
        while (!mQuitRequested && !mEngine->is_quit_requested()) {
            const core::result<void> stepped = pump_frame();
            if (!stepped.has_value()) {
                CONJURE_LOG_ERROR(LOG_EDITOR, "frame failed: {}", stepped.get_error().to_string());
                mExitCode = static_cast<int>(engine::exit_code::RUNTIME_FAILURE);
                break;
            }
            // A headless session whose only transport is stdio has nobody left to serve once standard input
            // ended; a windowed editor, or one with a TCP server, keeps running for its other clients. A quit
            // asked for in the same frame keeps its own exit code.
            const bool stdioOnly = mOptions.headless && mOptions.aiStdio && !mOptions.wants_tcp_server();
            const bool quitPending = mQuitRequested || mEngine->is_quit_requested();
            if (stdioOnly && !quitPending && mServer != nullptr && mServer->stdio_closed()) {
                CONJURE_LOG_INFO(LOG_EDITOR, "ai protocol: standard input closed; quitting");
                request_quit(static_cast<int>(engine::exit_code::OK));
            }
        }
        if (!mQuitRequested && mEngine->is_quit_requested()) {
            mExitCode = static_cast<int>(mEngine->current_exit_code());
        }
        return mExitCode;
    }

    core::result<void> editor_application::pump_frame() {
        if (mEngine == nullptr) {
            return make_editor_error(protocol_error_code::INTERNAL, "no engine to pump");
        }
        CONJURE_TRY(mEngine->step_frame());
        ++mTicks;
        if (mPendingProject.has_value()) {
            CONJURE_TRY(apply_pending_project_open());
        }
        return core::result<void>::success();
    }

    void editor_application::headless_tick() {
        if (mContext != nullptr) {
            mContext->tick();
        }
    }

    void editor_application::shutdown() {
        if (mServer != nullptr) {
            mServer->stop();
        }
        if (mContext != nullptr) {
            mContext->set_server(nullptr);
            if (!mContext->play().is_editing()) {
                // The play snapshot and the frozen undo stack refer to the live scene: leave play before the
                // engine tears it down.
                (void) mContext->play().stop();
            }
        }
        if (mEngine != nullptr) {
            // The context's module consumer must outlive the engine's module manager shutdown.
            mEngine->shutdown();
        }
        mContext.reset();
        mEngine.reset();
        mServer.reset();
        mInitialised = false;
    }

    void editor_application::request_quit(int exitCode) {
        mQuitRequested = true;
        mExitCode = exitCode;
        if (mEngine != nullptr) {
            mEngine->request_quit(engine::exit_code::OK);
        }
    }

    core::result<void> editor_application::request_open_project(const std::filesystem::path &directory) {
        CONJURE_TRY(engine::project::load(directory));
        CONJURE_TRY(require_trusted(directory));
        mPendingProject = directory;
        return core::result<void>::success();
    }

    bool editor_application::has_pending_project_open() const noexcept {
        return mPendingProject.has_value();
    }

    std::optional<core::json::document> editor_application::startup_scene_warning() const {
        if (mEngine == nullptr) {
            return std::nullopt;
        }
        const std::optional<core::error> &failure = mEngine->startup_scene_failure();
        if (!failure.has_value()) {
            return std::nullopt;
        }
        return core::json::object({
          {"code", "STARTUP_SCENE_FAILED"},
          {"message", std::string_view(failure->message())},
        });
    }

    bool editor_application::can_ask_about_trust(const editor_options &options) noexcept {
        return !options.headless && options.execFile.empty() && options.exportDirectory.empty();
    }

    core::result<startup_project> editor_application::resolve_startup_project(
      const editor_options &options,
      project_trust &trust
    ) {
        startup_project startup = {};
        if (options.projectPath.empty()) {
            return startup;
        }
        if (options.trustProject) {
            CONJURE_TRY(trust.trust(options.projectPath, false, "command line"));
        }
        if (trust.is_trusted(options.projectPath)) {
            startup.open = options.projectPath;
            return startup;
        }
        if (!can_ask_about_trust(options)) {
            return not_trusted(options.projectPath);
        }
        // Somebody sits at a window: the editor starts without the project and asks there.
        startup.unanswered = options.projectPath;
        return startup;
    }

    core::result<startup_plan> editor_application::plan_startup(const editor_options &options, project_trust &trust) {
        CONJURE_TRY_ASSIGN(const startup_project startup, resolve_startup_project(options, trust));
        startup_plan plan = {};
        // Over startup.open and nothing else, so a project nobody answered for has none of its modules or
        // scripts loaded, whatever --project named.
        CONJURE_TRY_ASSIGN(plan.desc, make_engine_desc(options, startup.open));
        plan.unanswered = startup.unanswered;
        return plan;
    }

    core::result<void> editor_application::require_trusted(const std::filesystem::path &projectPath) const {
        if (mTrust.is_trusted(projectPath)) {
            return core::result<void>::success();
        }
        return not_trusted(projectPath);
    }

    core::result<void> editor_application::start_engine(const std::filesystem::path &projectPath) {
        CONJURE_TRY_ASSIGN(engine::application_desc desc, make_engine_desc(mOptions, projectPath));
        return boot_engine(std::move(desc));
    }

    core::result<void> editor_application::boot_engine(engine::application_desc desc) {
        std::unique_ptr<engine::application> engine = std::make_unique<engine::application>(std::move(desc));
#if defined(CONJURE_BUILD_EDITOR) && CONJURE_BUILD_EDITOR
        if (!mOptions.headless) {
            engine->layers().push_layer(std::make_unique<engine::imgui_layer>());
        }
#endif
#if defined(CONJURE_EDITOR_WITH_BINDINGS) && CONJURE_EDITOR_WITH_BINDINGS
        // Startup step 16 (docs 3.1, 06_engine_loop.md 3.4): the bindings fill the non-core native table before
        // scripting's post_startup loads the game assembly. A no-op when scripting is off, so --no-scripting
        // keeps working; on the null host it fills the same table conjure_runtime gets.
        engine->add_subsystem(std::make_unique<bindings::scripting_bindings_subsystem>());
#endif
        engine->layers().push_layer(std::make_unique<editor_tick_layer>(*this));
        CONJURE_TRY(engine->initialise());
        if (mContext == nullptr) {
            mContext = std::make_unique<editor_context>(*engine, this, display_mode_for(mOptions));
        } else {
            mContext->rebind(*engine);
        }
        if (!mOptions.headless && mUiFactory) {
            std::unique_ptr<engine::layer> ui = mUiFactory(*mContext);
            if (ui != nullptr) {
                engine->layers().push_layer(std::move(ui));
            }
        }
        mEngine = std::move(engine);
        return core::result<void>::success();
    }

    core::result<void> editor_application::apply_pending_project_open() {
        const std::filesystem::path next = *mPendingProject;
        mPendingProject.reset();
        // Asked again, before anything of the running engine is touched: the answer may have been revoked since
        // the request was accepted (project.revoke_trust in the same frame, another editor of this user).
        const core::result<void> trusted = require_trusted(next);
        if (!trusted.has_value()) {
            CONJURE_LOG_WARN(LOG_EDITOR, "project.open dropped: {}", trusted.get_error().message());
            return core::result<void>::success();
        }
        const std::filesystem::path previous = mOptions.projectPath;
        CONJURE_LOG_INFO(LOG_EDITOR, "opening project '{}'", next.generic_string());
        if (mContext != nullptr && !mContext->play().is_editing()) {
            // As in shutdown(): the play snapshot and the frozen undo stack refer to the old engine's scene, and
            // rebind keeps the play controller, so no play session may cross the restart.
            (void) mContext->play().stop();
        }
        mEngine->shutdown();
        std::unique_ptr<engine::application> old = std::move(mEngine);
        core::result<void> started = start_engine(next);
        if (!started.has_value()) {
            CONJURE_LOG_ERROR(
              LOG_EDITOR,
              "project.open '{}' failed: {}",
              next.generic_string(),
              started.get_error().to_string()
            );
            // Back to the project that was open, asked again like any other opening: its answer may have been
            // revoked while it was open, and starting it loads its modules and game assembly once more.
            std::filesystem::path fallback = previous;
            if (!fallback.empty() && !require_trusted(fallback).has_value()) {
                CONJURE_LOG_WARN(
                  LOG_EDITOR,
                  "the previous project '{}' is no longer trusted; the editor goes on without a project",
                  fallback.generic_string()
                );
                fallback.clear();
            }
            started = start_engine(fallback);
            if (!started.has_value()) {
                request_quit(static_cast<int>(engine::exit_code::INIT_FAILURE));
                return started;
            }
            mOptions.projectPath = fallback;
        } else {
            mOptions.projectPath = next;
        }
        old.reset();
        if (mServer != nullptr && mContext != nullptr) {
            // The session files under <project>/.conjure (ai_port and the two token files) and the audit log
            // follow the open project: the previous project's session files are removed, the new ones name the
            // port and the tokens, and later commands are recorded in the new project.
            mServer->set_project_root(
              mContext->project() != nullptr ? mContext->project()->root_directory() : std::filesystem::path()
            );
        }
        if (mContext != nullptr) {
            mContext->events().emit(
              editor_event::SCENE_LOADED,
              core::json::object({
                {"path", std::string_view(mContext->scene_path())},
                {"project", mOptions.projectPath.generic_string()},
              })
            );
            // The clients of the session outlive the restart: they hear at once that the project they now work
            // in started without its startup scene (the runner's first line covers the first start only).
            if (std::optional<core::json::document> warning = startup_scene_warning(); warning.has_value()) {
                (void) mContext->events().emit(editor_event::WARNING, std::move(*warning));
            }
        }
        return core::result<void>::success();
    }

    int editor_application::run_export() {
        protocol_request request = {};
        request.id = make_id(0);
        request.command = "project.export";
        // A path on the command line is relative to the working directory, as in every shell; project.export
        // would read a relative one against the project root (04_editor.md D24), so both go in absolute.
        request.args = core::json::object({{"directory", working_directory_path(mOptions.exportDirectory)}});
        if (!mOptions.exportDistribution.empty()) {
            request.args.set("distributionDirectory", working_directory_path(mOptions.exportDistribution));
        }
        std::optional<protocol_response> captured = std::nullopt;
        const u32 clientId = mContext->events().register_client({});
        mContext->register_response_sink(clientId, [&captured](const protocol_response &response) {
            captured = response;
        });
        request.clientId = clientId;
        std::optional<protocol_response> response = mContext->dispatcher().dispatch(request);
        while (!response.has_value() && !captured.has_value()) {
            const core::result<void> stepped = pump_frame();
            if (!stepped.has_value()) {
                break;
            }
        }
        if (!response.has_value()) {
            response = captured;
        }
        mContext->unregister_response_sink(clientId);
        mContext->events().unregister_client(clientId);
        if (!response.has_value() || !response->ok) {
            std::cerr << "export failed: " << (
              response.has_value() ? response->errorMessage : std::string("no response")
            ) << '\n';
            return 1;
        }
        std::cout << serialise(*response) << '\n';
        return 0;
    }

    int editor_application::run_exec() {
        // initialise() read and parsed the file before the engine started; all that can still fail is the output.
        headless_runner runner = {};
        const core::result<exec_summary> summary = runner.run(*this, mOptions, mExecRequests);
        if (!summary.has_value()) {
            std::cerr << summary.get_error().message() << '\n';
            return static_cast<int>(engine::exit_code::RUNTIME_FAILURE);
        }
        return summary.value().exitCode;
    }
}
